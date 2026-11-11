# Native string ownership audit

## Provenance and complete native inventory

Baseline: `89163513f0c523fa42981062113391d8ea35e4b6`. Guidance:
`miyako/skills/4d-skills/skills/4dplugin/SKILL.md` at immutable
`e6b492fcb81e5e1a7f7db723594b420e63bf6eb9`, read through the GitHub API.
Prior-art force-included synthetic platform technique was read through the API
from `miyako/4d-plugin-virtual-key` at
`255b85d1ff69657f432de80f3aa499527d53f077`; no checkout or results were copied.

The bundled API identifies itself as v12.0, with local modifications dated
2011-05-29 (implementation) and 2011-01-22 (header), not a verified upstream SDK
release. Protected Git blob identities are enforced for baseline and fixed builds:

| File under `4D Plugin API/` | Git blob |
|---|---|
| `4DPluginAPI.cpp` | `85fecf672a2bb58a6aae257a5ef943fc37f952c9` |
| `4DPluginAPI.h` | `16d1402f4c96891bdf75ecbc7c36a712ff00284e` |
| `PublicTypes.h` | `a22ac75de6d3a7159204c4e27523c00f78f9943b` |
| `PrivateTypes.h` | `347c45f30b1aedf7dff1f1008324ede025284f04` |
| `EntryPoints.h` | `524d7ea7b153b203629bd83b190ed582b4c449de` |
| `Flags.h` | `8c0aaaa655500fa478e47d9ddca81737dbebee0a` |
| `4DPluginAPI.def` | `996ecc4ab11ad165de9192eba2e7478db141c757` |

All tracked native production sources are the complete API implementation,
`4DPlugin.cpp`, `functions_wnds.cpp`, and these **13** wrapper `.cpp` files with
matching headers under `Classes/`:

| Wrappers/helpers | Ownership and consumers |
|---|---|
| `C_TEXT` | Owns a heap `CUTF16String`; copies parameter text into it; content-setter output, not descriptor transfer. Already noncopyable at baseline. Used by scalar commands 1-16 and config helpers. |
| `ARRAY_TEXT` | Owns a heap vector of value strings. Borrows array descriptors only while copying input; creates distinct strings for accepted output transfers, including element zero. Three instances in command 17. |
| `ARRAY_BOOLEAN`, `ARRAY_INTEGER`, `ARRAY_REAL` | Own heap vectors. No production command consumers here, but explicitly compiled by the Visual Studio project and harness. |
| `ARRAY_LONGINT` | Owns a heap vector. Two instances in command 17. |
| `C_BLOB` / `CBytes` | `C_BLOB` owns a heap `CBytes`; `CBytes` owns its vector by value. Byte helper used internally by `CPicture`; no exported blob command. |
| `C_PICTURE` / `CPicture` | Nested owning heap pointers and a retained picture. File, bytes, transform, size, thumbnail, representation, parameter, and return helpers audited despite no exported command consumers. |
| `C_DATE`, `C_INTEGER`, `C_LONGINT`, `C_REAL`, `C_TIME` | Scalar/value fields, no raw owned string pointers. `C_LONGINT` used by service commands. Date/time Objective-C helper has no command consumer. |

Other native interfaces: `Classes/C_TYPES.h`, the seven protected API files above,
`4DPlugin.h`, `functions_wnds.h`, and `4D Plugin_Prefix.pch`. There are no additional
tracked production C/C++/Objective-C translation units. `.def` exports
`FourDPackex`; it calls actual `PluginMain`/`CommandDispatcher`. The harness
executes `CommandDispatcher` directly and compiles `PluginMain` and the SDK entry
point unchanged. It does not claim to emulate plugin loading.

`Windows Services.sln` selects the four VC Debug/Release Win32/x64 configurations.
`Windows Services.vcxproj` explicitly compiles all 16 production translation
units, includes the matching interfaces, links Win32 service libraries, and writes
inside the prebuilt bundle tree. Xcode has `4DPlugin.cpp` in its explicit Sources
phase and a filesystem-synchronized API root, without a target
`fileSystemSynchronizedGroups` association; `functions_wnds.cpp` is not an explicit
member. This audit neither repairs nor claims to validate that native membership.
All dormant files are nevertheless compiled in isolation. Xcode's prefix header
imports Carbon, its framework phases link Carbon/Cocoa, and its Resources/Rez
phases produce plugin resources. No shell build phases occur in either project.
Neither project is executed or modified.

The binary `4DPlugin.rsrc` command signatures and bundled `documentation.xml`
declare 17 commands. Commands 1-16 have scalar text/longint arguments and longint
results. Command 17 declares five generic arrays: actual consumers are three
text arrays (name, display name, account) and two longint arrays (state, start
type). All service helpers (`_controlService`, `_getServiceConfig`,
`_setServiceConfig`, dormant `_changeAccess`) and all dispatcher arms were read,
not excluded by manifest type. The bundled resources, example databases,
prebuilt binaries, and metadata are not modified or used as runtime fixtures.

## Exact local contracts and corrections

`PA_SetStringInArray` (baseline 4349-4364) checks Unicode kind, non-null data,
`0 <= index <= count`, and non-null descriptor pointer, then disposes the old
element and shallow-assigns the incoming descriptor. Accepted input transfers
ownership, leaving the local alias unchanged. Rejection retains caller ownership.
`PA_GetStringInArray` (4196-4217) returns a borrowed alias. `ARRAY_TEXT` copies
borrowed characters into value strings before output mutation, and does **not**
dispose transferred descriptors. There is no immediate post-transfer disposal
bug in its normal accepted path; blanket disposal removal was not appropriate.

`PA_SetStringVariable` (3815-3820) is also shallow and does not clear an old owner.
`PA_CreateVariable(eVK_Unistring)` (3621-3623) creates an empty owned string;
`PA_SetUnistring` (323-330) is a content setter. `fromFileAtPath` previously
overwrote the empty owner with an independent path, explicitly disposed that
path, then cleared the aliased variable. It now fills the existing owned string
and clears the variable once. Scalar `C_TEXT` output continues to use the content
setter, with independent wrapper text valid until after consumption.

`PA_GetPictureVariable` (3721-3727) explicitly returns a variable-owned picture.
`PA_SetPictureVariable` (3846-3851) shallow-assigns; `PA_DuplicatePicture(...,1)`
(507-514) retains. File output is retained by `setPicture` **before** its argument
is cleared. `setBytes` releases the creation reference after successful retention.
It captures the creation error immediately, never adopts or disposes an
unspecified failed return, leaves the existing picture intact, and leaves
`PA_GetLastError()` available to its caller. The void wrapper gains no exception,
notification, or new error channel.

`PA_CreateVariable(eVK_Picture)` (3553-3555) creates an empty picture owner.
Transform, size, and thumbnail input arguments previously overwrote that owner.
They now start Undefined before the shallow setter. Grayscale still supplies its
independently retained reference and clears it after the synchronous command.
Size/thumbnail still detach borrowed inputs before clearing; thumbnail still
detaches and returns the owning output. Output variables that need an initial
picture owner retain their original initialization.

Five array wrappers, `C_BLOB`, `CPicture`, and `C_PICTURE` had compiler-generated
copies aliasing their owned pointer/reference. These eight classes now delete
copy construction and assignment, matching existing `C_TEXT` prior art.
The same harness demonstrates seven named baseline ASan heap-use-after-free
diagnostics and one `CPicture` reference-balance failure. No production copy call
was found; these are latent ownership defects, not evidence of service-command
reachability. Compiling all actual consumers proves the restrictions do not
break existing call sites. `CBytes` and scalar value wrappers remain copyable.

## Reproducible evidence

Run `PYTHONDONTWRITEBYTECODE=1 python3 .github/tests/test_ownership.py` on macOS.
It uses Python's standard library and the existing Apple compiler, with no
dependency installation. The **same final harness** compiles fixed working
sources and an immutable `git archive` of the baseline; no SDK fragments,
replacement SDK functions, project build, plugin installation, or binary fixtures
are used. Each case is a separate process, instrumented with ASan and UBSan.
Alignment checks are excluded because this SDK deliberately packs engine
structures to two bytes; leak detection is disabled on macOS, with explicit
string-allocation, picture-reference, and handle balances instead.

| Source | Cases | Passing controls | Expected named failures |
|---|---:|---:|---:|
| Fixed | 87 | 87 | 0 |
| Immutable baseline | 87 | 50 | 37 |

The 37 baseline failures comprise nine duplicate/unowned string disposals,
19 picture-reference leaks, seven ASan wrapper-copy heap-use-after-free reports,
one `CPicture` unowned-picture disposal, and one attempted retain of a failed
picture creation's unspecified sentinel. Counts are enforced, not inferred from
a successful exit. The three Python tests additionally check protected-path
preservation, exact SDK blobs, complete native inventory/project membership,
command manifest, and narrowly scoped CI. Optional `OWNERSHIP_LOG` writes all
case arguments, return codes, stdout, and stderr to a caller-selected artifact.

Fixtures cover ASCII, BMP U+65E5, supplementary U+1F600, empty text, allocated and
null engine empties; path round trips and explicit scalar/array lengths; array
replacement, allocator churn, independent borrowed-input copying before mutation,
resize/clear, Undefined adoption, index-zero acceptance, and kind/data/index/null
descriptor rejection. Callback argument-handle and command failures exercise
existing nonthrowing behavior; failed picture creation uses an invalid sentinel
that must not be inspected/adopted/disposed. Object/collection owners are N/A in
this legacy target's wrapper surface, not certified by zero search matches.

Both binaries link the complete unchanged SDK and all wrappers. The Mac binary
compiles both actual consumers and confirms its conditional dispatcher is a
no-op. A second build compiles the **actual** `functions_wnds.cpp` and
`4DPlugin.cpp` with Windows conditionals enabled after importing the unchanged
SDK types. It runs commands 2, 8, and 17 against synthetic in-memory SCM/query/
enumeration responses, including access denial. All other control/configuration/
security/install/delete/start entry points are fail-closed stubs. Unknown engine
selectors and commands abort, as do filesystem/URL helpers. A linked-import gate
rejects real SCM/service, registry, process-launch, filesystem, and network entry
points. Cocoa/CoreFoundation are used only for in-memory Unicode/value support.

`bump-version.yml`, `release.yml`, all scripts/hooks, solution, and project phases
were read before validation or push. Existing workflows are dispatch-only and
remain unchanged. The new read-only `ownership.yml` runs only on relevant pushes
to the exact audit branch and `master`, checks out full history for the immutable
baseline, and has no publishing/build/signing/manual-dispatch stages.

## Concrete limits and deferrals

- This is not a Windows compiler/ABI build or a reproduction in any version of
  the 4D host. Windows conditionals use macOS SDK types and synthetic boundary
  types; native `wchar_t`, Win32 calling conventions, and Windows conversion
  functions are not verified. All 17 commands compile, but only 2/8/17 execute
  with synthetic service responses. No real service enumeration/access,
  configuration, control, install/uninstall, registry, process, network, private
  userdata, or system-state operation occurs.
- Protected `4DPluginAPI.cpp` helpers `PA_ConvertFromUTF8` (71-85) pass the
  converter's documented **byte** count directly to a UTF-16 string constructor
  expecting **code units**. This is a concrete protected-SDK bounds defect,
  not exercised or fixed by the wrapper-only change. The target has no
  `CPicture::createCopy` or pointer-reference wrapper; no metadata was invented.
- Protected `PA_ResizeArray` (3964-4095) can update count after resize failure and
  disposes shrinking elements before knowing resize succeeded. Reliable OOM
  preservation cannot be inferred. `ARRAY_TEXT` also lacks an error channel for
  malformed/null-data destinations or allocation/resize failure. Valid-destination
  success and direct setter rejection controls do **not** certify these paths.
  SDK changes and an agreed error/preservation contract are required separately.
- Successful picture-representation metadata and SDK folder/path getters return
  engine descriptors without a sufficiently explicit local ownership/error
  contract. `_GetCount`, `loadImageOfType`, and protected
  `PA_GetDatabaseFolderPath` require further host-contract evidence before
  adding cleanup; the harness deliberately supplies no picture representations.
  It does not certify their successful-string cleanup or conversion bounds.
- Existing void helpers do not report all command failures. Returned command
  variables are unused for these procedure selectors; the shim models successful
  results as Undefined, not as owning object/string returns. It makes no claim
  that arbitrary successful return values or unspecified failed values can be
  cleared. Constructor/retain OOM contracts and retained engine-side callback
  arguments are not certified. No new throws or success-shaped fallback outputs
  were added to hide these gaps.
