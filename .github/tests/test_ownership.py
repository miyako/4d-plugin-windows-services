"""Full unchanged SDK/all wrappers; unsigned, in-memory ownership regression."""

import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASELINE = "89163513f0c523fa42981062113391d8ea35e4b6"
SDK = {
    "4DPluginAPI.cpp": "85fecf672a2bb58a6aae257a5ef943fc37f952c9",
    "4DPluginAPI.h": "16d1402f4c96891bdf75ecbc7c36a712ff00284e",
    "PublicTypes.h": "a22ac75de6d3a7159204c4e27523c00f78f9943b",
    "PrivateTypes.h": "347c45f30b1aedf7dff1f1008324ede025284f04",
    "EntryPoints.h": "524d7ea7b153b203629bd83b190ed582b4c449de",
    "Flags.h": "8c0aaaa655500fa478e47d9ddca81737dbebee0a",
    "4DPluginAPI.def": "996ecc4ab11ad165de9192eba2e7478db141c757",
}
WRAPPERS = {
    "ARRAY_BOOLEAN", "ARRAY_INTEGER", "ARRAY_LONGINT", "ARRAY_REAL", "ARRAY_TEXT",
    "C_BLOB", "C_DATE", "C_INTEGER", "C_LONGINT", "C_PICTURE", "C_REAL",
    "C_TEXT", "C_TIME",
}
FIXTURES = ("ASCII", "\u65e5", "\U0001f600", "")
NONCOPY = (
    "ARRAY_BOOLEAN", "ARRAY_INTEGER", "ARRAY_LONGINT", "ARRAY_REAL", "ARRAY_TEXT",
    "C_BLOB", "CPicture", "C_PICTURE",
)


def cases():
    for text in FIXTURES:
        for empty in ("allocated", "null"):
            for name in ("path", "arrays", "scalar"):
                yield ("mac", name, text, empty, "none")
            for platform in ("mac", "windows"):
                for mode in ("success", "denied"):
                    yield (platform, "dispatch", text, empty, mode)
    for name in ("bytes", "gray", "size", "thumbnail"):
        yield ("mac", name, "", "allocated", "none")
    for name in ("path", "gray", "size", "thumbnail"):
        for failure in ("command", "handle"):
            for empty in ("allocated", "null"):
                yield ("mac", name, "", empty, failure)
    for name in (*NONCOPY, "C_TEXT", "values"):
        yield ("mac", "traits", name, "allocated", "none")
    yield ("mac", "creation-failure", "", "allocated", "none")


def baseline_failure(case):
    return (case[1] in ("path", "bytes", "gray", "size", "thumbnail", "creation-failure")
            or (case[1] == "traits" and case[2] in NONCOPY))


def baseline_diagnostic(case):
    if case[1] == "traits":
        return "unowned picture disposal" if case[2] == "CPicture" else \
            "AddressSanitizer: heap-use-after-free"
    if case[1] == "path":
        return "unbalanced picture references" if case[2:4] == ("", "null") else \
            "duplicate/unowned string disposal"
    if case[1] == "creation-failure":
        return "retain dead picture"
    return "unbalanced picture references"


@unittest.skipUnless(sys.platform == "darwin", "macOS synthetic host compiler")
class OwnershipTests(unittest.TestCase):
    def build(self, source, directory):
        api = source / "4D Plugin API"
        for name, wanted in SDK.items():
            contents = (api / name).read_bytes()
            blob = hashlib.sha1(f"blob {len(contents)}\0".encode() + contents).hexdigest()
            self.assertEqual(blob, wanted, f"Protected SDK changed: {name}")
        wrappers = sorted((api / "Classes").glob("*.cpp"))
        self.assertEqual({path.stem for path in wrappers}, WRAPPERS)
        tests = ROOT / ".github/tests"
        flags = [
            "clang++", "-std=c++14", "-x", "objective-c++",
            "-include", str(tests / "ownership_platform.h"),
            "-I", str(api), "-I", str(api / "Classes"), "-I", str(source),
            "-I", str(tests), "-fsanitize=address,undefined",
            "-fno-sanitize=alignment", "-fno-omit-frame-pointer", "-g",
            "-Wno-deprecated-declarations", "-Wno-return-type-c-linkage",
            "-Wno-ignored-attributes", "-Wno-writable-strings",
        ]
        windows = []
        for name in ("4DPlugin", "functions_wnds"):
            obj = directory / f"{name}.o"
            command = [
                *flags, "-include", str(tests / "ownership_windows.h"),
                "-c", str(source / f"{name}.cpp"), "-o", str(obj),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            windows.append(obj)
        binaries = {}
        for platform in ("mac", "windows"):
            binary = directory / platform
            consumers = (
                [str(source / "4DPlugin.cpp"), str(source / "functions_wnds.cpp")]
                if platform == "mac" else
                ["-DOWNERSHIP_WINDOWS_DISPATCH", "-x", "none", *map(str, windows),
                 "-x", "objective-c++"]
            )
            command = [
                *flags, str(api / "4DPluginAPI.cpp"), *map(str, wrappers), *consumers,
                str(tests / "ownership_host.mm"),
                "-framework", "Cocoa", "-framework", "Carbon", "-o", str(binary),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            imports = subprocess.check_output(["nm", "-u", str(binary)], text=True)
            symbols = {line.split()[-1] for line in imports.splitlines() if line.split()}
            forbidden = {
                "_system", "_popen", "_execve", "_posix_spawn", "_kill", "_socket",
                "_connect", "_fopen", "_open", "_opendir", "_NSLog",
                "_OBJC_CLASS_$_NSURL", "_OBJC_CLASS_$_NSFileManager",
                "_OBJC_CLASS_$_NSWorkspace", "_OBJC_CLASS_$_NSProcessInfo",
                "_CFURLCreateWithFileSystemPath", "_CFURLCopyFileSystemPath",
            }
            rejected = forbidden & symbols
            rejected |= {symbol for symbol in symbols if symbol.startswith((
                "_OpenSCManager", "_OpenService", "_QueryService", "_EnumServices",
                "_CreateService", "_DeleteService", "_StartService", "_ControlService",
                "_ChangeService", "_Reg", "_CreateProcess", "_ShellExecute",
            ))}
            self.assertFalse(rejected, f"Real platform boundary linked: {rejected}")
            binaries[platform] = binary
        return binaries

    def test_fixed_and_immutable_baseline(self):
        records = []
        with tempfile.TemporaryDirectory(prefix="services-ownership-") as temp:
            directory = Path(temp)
            baseline = directory / "baseline"
            baseline.mkdir()
            archive = subprocess.check_output([
                "git", "archive", BASELINE, "4D Plugin API", "4DPlugin.cpp",
                "4DPlugin.h", "functions_wnds.cpp", "functions_wnds.h",
            ], cwd=ROOT)
            with tarfile.open(fileobj=io.BytesIO(archive)) as files:
                for member in files.getmembers():
                    path = Path(member.name)
                    self.assertFalse(path.is_absolute() or ".." in path.parts)
                    self.assertTrue(member.isfile() or member.isdir())
                files.extractall(baseline, filter="data")
            for label, source in (("fixed", ROOT), ("baseline", baseline)):
                build = directory / f"{label}-build"
                build.mkdir()
                binaries = self.build(source, build)
                passed = failed = 0
                for case in cases():
                    platform, *args = case
                    result = subprocess.run(
                        [str(binaries[platform]), *args],
                        env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                             "UBSAN_OPTIONS": "halt_on_error=1"},
                        capture_output=True, text=True,
                    )
                    records.append({
                        "source": label, "case": case, "returncode": result.returncode,
                        "stdout": result.stdout, "stderr": result.stderr,
                    })
                    should_fail = label == "baseline" and baseline_failure(case)
                    with self.subTest(source=label, case=case):
                        if should_fail:
                            failed += 1
                            self.assertNotEqual(result.returncode, 0, "baseline defect not reproduced")
                            self.assertRegex(result.stderr, baseline_diagnostic(case))
                        else:
                            passed += 1
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                            self.assertIn("strings=0 pictures=0 handles=0", result.stdout)
                print(f"{label}: cases={passed+failed} passes={passed} "
                      f"expected_defects={failed}; full SDK + 13 wrappers; "
                      "Mac/Windows-conditional consumers; import gates passed", file=sys.stderr)
                self.assertEqual((passed, failed), (87, 0) if label == "fixed" else (50, 37))
        log = os.environ.get("OWNERSHIP_LOG")
        if log:
            Path(log).write_text(json.dumps(records, ensure_ascii=True, indent=2) + "\n")

    def test_preservation(self):
        baseline_paths = subprocess.check_output(
            ["git", "ls-tree", "-r", "--name-only", BASELINE], cwd=ROOT, text=True
        ).splitlines()
        allowed = {
            "4D Plugin API/Classes/" + name for name in (
                "ARRAY_BOOLEAN.h", "ARRAY_INTEGER.h", "ARRAY_LONGINT.h", "ARRAY_REAL.h",
                "ARRAY_TEXT.h", "C_BLOB.h", "C_PICTURE.h", "C_PICTURE.cpp",
            )
        } | {"README.md", ".github/tests/test_repository_contract.py"}
        changes = subprocess.check_output(
            ["git", "diff", "--name-only", BASELINE, "--", *baseline_paths],
            cwd=ROOT, text=True,
        ).splitlines()
        self.assertFalse(set(changes) - allowed, f"Protected paths changed: {changes}")
        workflow = (ROOT / ".github/workflows/ownership.yml").read_text()
        self.assertIn("branches: [miyako-windows-services-ownership, master]", workflow)
        self.assertIn("fetch-depth: 0", workflow)
        self.assertIn("runs-on: macos-latest", workflow)
        self.assertNotRegex(
            workflow, r"(?i)workflow_dispatch|upload|artifact|release\.py|"
            r"xcodebuild|msbuild|codesign|notarytool|pip install|permissions:\s*\n\s*contents: write"
        )

    def test_native_inventory_and_manifest(self):
        import xml.etree.ElementTree as ET

        paths = subprocess.check_output(
            ["git", "ls-files", "*.cpp", "*.c", "*.mm", "*.m"], cwd=ROOT, text=True
        ).splitlines()
        expected = {"4D Plugin API/Classes/" + name + ".cpp" for name in WRAPPERS}
        expected |= {"4D Plugin API/4DPluginAPI.cpp", "4DPlugin.cpp", "functions_wnds.cpp"}
        # This test's synthetic host is deliberately outside production project membership.
        self.assertEqual(set(paths) - {".github/tests/ownership_host.mm"}, expected)
        project = ET.fromstring((ROOT / "Windows Services.vcxproj").read_bytes())
        ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
        compiled = {
            entry.attrib["Include"].replace("\\", "/")
            for entry in project.findall(".//m:ClCompile[@Include]", ns)
        }
        self.assertEqual(compiled, expected)
        manifest = ROOT / (
            "Windows Services.4dbase/Plugins/Windows Services.bundle/Contents/"
            "Resources/documentation.xml"
        )
        ns = {"d": "http://www.4d.com/plugin"}
        commands = ET.fromstring(manifest.read_bytes()).findall(".//d:command", ns)
        self.assertEqual([int(c.attrib["command-id"]) for c in commands], list(range(1, 18)))
        for command in commands[:16]:
            self.assertEqual(command.find("d:return-value", ns).attrib["type"], "L")
            self.assertLessEqual(
                {a.attrib["type"] for a in command.findall("d:argument", ns)}, {"T", "L"}
            )
        self.assertEqual(
            [a.attrib["label"] for a in commands[16].findall("d:argument", ns)],
            ["serviceNames", "serviceDisplayNames", "accountNames", "states", "startTypes"],
        )


if __name__ == "__main__":
    unittest.main()
