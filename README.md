# 4d-plugin-windows-services

This plugin lets a 4D application manage Windows Services directly from 4D code, driving the same Win32 Service Control Manager (SCM) API (`OpenSCManager`, `CreateService`, `ChangeServiceConfig`, `ControlService`, `EnumServicesStatus`, and related calls) that tools like the Services MMC snap-in and `sc.exe` use. It can create, configure, query, start/stop/pause/resume, delete, and enumerate services and drivers registered on the local machine.

Command | Returns | Purpose
---|---|---
[SERVICE Set display name](#service-set-display-name) | Longint | Change a service's display name
[SERVICE Get display name](#service-get-display-name) | Longint | Read a service's display name
[SERVICE Change account](#service-change-account) | Longint | Change the logon account/password a service runs under
[SERVICE Create](#service-create) | Longint | Register a new service
[SERVICE Delete](#service-delete) | Longint | Remove a service registration
[SERVICE Get state](#service-get-state) | Longint | Read a service's current run state
[SERVICE Set path](#service-set-path) | Longint | Change a service's executable path/command line
[SERVICE Get path](#service-get-path) | Longint | Read a service's executable path/command line
[SERVICE Set start type](#service-set-start-type) | Longint | Change how a service starts (auto/manual/disabled/...)
[SERVICE Get start type](#service-get-start-type) | Longint | Read a service's start type
[SERVICE Set type](#service-set-type) | Longint | Change a service's type flags
[SERVICE Get type](#service-get-type) | Longint | Read a service's type flags
[SERVICE Resume](#service-resume) | Longint | Resume a paused service
[SERVICE Pause](#service-pause) | Longint | Pause a running service
[SERVICE Stop](#service-stop) | Longint | Stop a running service
[SERVICE Start](#service-start) | Longint | Start a service
[SERVICE GET LIST](#service-get-list) | *(none)* | Enumerate every service and driver on the machine

**Platforms:** Windows only. The plugin has no macOS implementation — every command is a no-op on macOS (it simply returns without calling any API).

---

## Requirements & platform notes

- **Administrator rights.** Any command that changes a service's configuration or its running state (create, delete, set-anything, start/stop/pause/resume, change account) requires the host 4D process to be running with administrator rights. Commands that only read configuration (the `Get`-prefixed commands and `SERVICE GET LIST`) do not.
- **Return-value convention.** Every command except `SERVICE GET LIST` returns a `Longint`: `0` means success; any other value is a raw Windows system error code (the same numbering `GetLastError` returns — look them up under "System Error Codes" in the Windows SDK documentation, e.g. `5` = access denied, `1060` = service does not exist).
- **`SERVICE GET LIST` has no return value.** Call it without assigning a result; it always fills its five output arrays (empty if enumeration failed or no services matched).
- **Empty account name/password is normal, not an error.** For a driver, or any service running under a default/system account, the account name reported by `SERVICE Get display name`'s siblings or `SERVICE GET LIST` can legitimately come back as an empty string — this reflects how the service is actually registered, not a lookup failure.
- **Pause/Resume aren't universally supported.** Whether `SERVICE Pause`/`SERVICE Resume` do anything depends on the target service itself accepting pause/continue controls — many services, including 4D Server's own Windows service, don't. Calling them on a service that doesn't support it returns an error rather than silently succeeding.
- **Service type and start type are Win32 constants**, passed/returned as `Longint` bit values:

  Service type (`SERVICE_Set_type`/`SERVICE_Get_type`, can be combined with `|`) | Value
  ---|---
  `SERVICE_KERNEL_DRIVER` | 1
  `SERVICE_FILE_SYSTEM_DRIVER` | 2
  `SERVICE_WIN32_OWN_PROCESS` | 16
  `SERVICE_WIN32_SHARE_PROCESS` | 32
  `SERVICE_INTERACTIVE_PROCESS` | 256 (combine with one of the two above)

  Start type (`SERVICE_Set_start_type`/`SERVICE_Get_start_type`) | Value
  ---|---
  `SERVICE_BOOT_START` | 0
  `SERVICE_SYSTEM_START` | 1
  `SERVICE_AUTO_START` | 2
  `SERVICE_DEMAND_START` | 3
  `SERVICE_DISABLED` | 4

  Current state (`SERVICE Get state`) | Value
  ---|---
  `SERVICE_STOPPED` | 1
  `SERVICE_START_PENDING` | 2
  `SERVICE_STOP_PENDING` | 3
  `SERVICE_RUNNING` | 4
  `SERVICE_CONTINUE_PENDING` | 5
  `SERVICE_PAUSE_PENDING` | 6
  `SERVICE_PAUSED` | 7

  If the plugin ships its own named constants for these (check your Explorer's constant list), prefer those over the raw numbers above for readability.

---

## SERVICE Set display name

### Syntax
```4d
SERVICE Set display name ( serviceName ; displayName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name (as registered with the SCM), not the display name
`displayName` | Text | New display name to assign
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Renames the display name shown for the service in the Services console. Requires administrator rights. Does not affect the internal service name, path, type, or start type.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Set display name($serviceName; "uuuuu")
```

---

## SERVICE Get display name

### Syntax
```4d
SERVICE Get display name ( serviceName ; displayName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`displayName` | Text | Receives the service's current display name
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires no special access rights. Returns an empty string in `displayName` if the lookup fails (check the result code, don't infer failure from an empty string alone in edge cases).

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Get display name($serviceName; $displayName)
```

---

## SERVICE Change account

### Syntax
```4d
SERVICE Change account ( serviceName ; accountName ; accountPassword ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`accountName` | Text | Account to run the service as (e.g. `.\username`, `NT AUTHORITY\LocalService`). Pass an empty string to leave the current account unchanged
`accountPassword` | Text | Password for `accountName`. Pass an empty string to leave the current password unchanged (ignored if `accountName` is also empty)
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights (this isn't demonstrated in a sample method, but it shares the same underlying configuration path as the other `Set`-prefixed commands below, all of which require administrator rights). Leave both `accountName` and `accountPassword` empty to make no change to the account at all.

### Example
```4d
$serviceName:="4DS sample"
$err:=SERVICE Change account($serviceName; ".\\ServiceUser"; "MyP@ssw0rd")
```

---

## SERVICE Create

### Syntax
```4d
SERVICE Create ( pathName ; serviceName ; displayName ; accountName ; accountPassword ; startType ; serviceType ) -> Longint
```

Parameter | Type | Description
---|---|---
`pathName` | Text | Full path to the executable (plus any command-line arguments) the service runs
`serviceName` | Text | Internal service name to register
`displayName` | Text | Display name shown in the Services console
`accountName` | Text | Account to run the service as; empty string runs it as `LocalSystem`
`accountPassword` | Text | Password for `accountName`; empty string if not applicable
`startType` | Longint | One of the start-type constants listed above
`serviceType` | Longint | One or more of the service-type constants listed above, combined with `|`
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Registers a new service with the SCM; the service isn't started automatically — call `SERVICE Start` afterward if you need it running immediately. The error handling always uses `SERVICE_ERROR_NORMAL` internally (not exposed as a parameter): a failure to start this service at boot is logged but doesn't stop the rest of the boot sequence.

### Example
From the plugin's own test method (`Method4.4dm`):
```4d
$pathName:=Application file:C491+" -s \""+Structure file:C489+"\""
$serviceName:="4DS sample"
$displayName:="My Super Service"
$accountName:=""
$accountPassword:=""
$serviceStartType:=SERVICE_AUTO_START
$serviceType:=SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS

$err:=SERVICE Create(\
$pathName; \
$serviceName; \
$displayName; \
$accountName; \
$accountPassword; \
$serviceStartType; \
$serviceType)
```
This is the standard pattern for registering a 4D Server instance as a Windows Service: `Application file` plus `Structure file` (quoted, with `-s`) becomes the service's command line.

---

## SERVICE Delete

### Syntax
```4d
SERVICE Delete ( serviceName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name to remove
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Marks the service for deletion; Windows removes it once no handles to it remain open and, if it's running, once it's stopped. Stop the service first if you need the deletion to take effect immediately.

### Example
From the plugin's own test method (`Method3.4dm`):
```4d
$serviceName:="4DS sample"
$err:=SERVICE Delete($serviceName)
```

---

## SERVICE Get state

### Syntax
```4d
SERVICE Get state ( serviceName ; serviceState ; waitHint ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`serviceState` | Longint | Receives the current state (see the state table above)
`waitHint` | Longint | Receives the estimated milliseconds remaining for a pending start/stop/pause/continue; only meaningful while `serviceState` is one of the `*_PENDING` values
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires no special access rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Get state($serviceName; $state; $waitHint)
```

---

## SERVICE Set path

### Syntax
```4d
SERVICE Set path ( serviceName ; pathName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`pathName` | Text | New full path (and arguments) to the executable
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Only takes effect the next time the service starts.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Set path($serviceName; Application file:C491+" -s \""+Structure file:C489+"\"")
```

---

## SERVICE Get path

### Syntax
```4d
SERVICE Get path ( serviceName ; pathName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`pathName` | Text | Receives the service's current executable path/command line
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires no special access rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Get path($serviceName; $pathName)
```

---

## SERVICE Set start type

### Syntax
```4d
SERVICE Set start type ( serviceName ; startType ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`startType` | Longint | One of the start-type constants listed above
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Set start type($serviceName; SERVICE_AUTO_START)
```

---

## SERVICE Get start type

### Syntax
```4d
SERVICE Get start type ( serviceName ; startType ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`startType` | Longint | Receives the current start type
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires no special access rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Get start type($serviceName; $startType)
```

---

## SERVICE Set type

### Syntax
```4d
SERVICE Set type ( serviceName ; serviceType ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`serviceType` | Longint | One or more of the service-type constants listed above, combined with `|`
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Set type($serviceName; SERVICE_WIN32_OWN_PROCESS)
```

---

## SERVICE Get type

### Syntax
```4d
SERVICE Get type ( serviceName ; serviceType ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
`serviceType` | Longint | Receives the current type flags
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires no special access rights.

### Example
From the plugin's own test method (`Method2.4dm`):
```4d
$err:=SERVICE Get type($serviceName; $type)
```

---

## SERVICE Resume

### Syntax
```4d
SERVICE Resume ( serviceName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. **Not applicable to 4D Server** — 4D Server's own service does not accept pause/continue controls, per the plugin's own test method. Only meaningful for target services that declare `SERVICE_ACCEPT_PAUSE_CONTINUE`.

### Example
From the plugin's own test method (`Method1.4dm`), which explicitly notes this isn't applicable to 4D Server:
```4d
//not applicable to 4D Server
$err:=SERVICE Pause($serviceName)
$err:=SERVICE Resume($serviceName)
```

---

## SERVICE Pause

### Syntax
```4d
SERVICE Pause ( serviceName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Same caveat as `SERVICE Resume` above: not every service supports being paused.

### Example
See `SERVICE Resume` above — both come from the same guarded block in `Method1.4dm`.

---

## SERVICE Stop

### Syntax
```4d
SERVICE Stop ( serviceName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Requests a stop and returns; it does not itself wait for the service to finish stopping. Poll `SERVICE Get state` if you need to confirm the service has actually reached `SERVICE_STOPPED`.

### Example
From the plugin's own test method (`Method1.4dm`):
```4d
$err:=SERVICE Stop($serviceName)
```

---

## SERVICE Start

### Syntax
```4d
SERVICE Start ( serviceName ) -> Longint
```

Parameter | Type | Description
---|---|---
`serviceName` | Text | Internal service name
Result | Longint | `0` on success, otherwise a Windows error code

### Description
Requires administrator rights. Same asynchronous-start caveat as `SERVICE Stop`: poll `SERVICE Get state` if you need to know when it's actually running rather than just pending.

### Example
From the plugin's own test method (`Method1.4dm`):
```4d
$serviceName:="4DS sample"
$err:=SERVICE Start($serviceName)
```

---

## SERVICE GET LIST

### Syntax
```4d
SERVICE GET LIST ( names ; displayNames ; accountNames ; states ; startTypes )
```

Parameter | Type | Description
---|---|---
`names` | Array Text | Receives every service/driver's internal name
`displayNames` | Array Text | Receives each entry's display name (empty if it couldn't be looked up)
`accountNames` | Array Text | Receives each entry's logon account name (empty for drivers or services with no explicit account)
`states` | Array Longint | Receives each entry's current state (see the state table above)
`startTypes` | Array Longint | Receives each entry's start type (see the start-type table above); `0` (`SERVICE_BOOT_START`) if the entry's configuration couldn't be looked up
Result | *(none)* | This command has no return value — don't assign it to a variable

### Description
Requires no special access rights. Enumerates every Win32 service **and** driver registered on the machine, in every state (running, stopped, paused, etc.) — this is a full system enumeration, not just "your" services, so expect a few hundred entries on a typical machine. All five arrays are always the same length, index-for-index. Two arrays share a slower, per-entry configuration query internally, so this command scales with the total number of services/drivers on the box rather than being instant.

### Example
From the plugin's own test method (`Method5.4dm`):
```4d
SERVICE GET LIST($names; $displayNames; $accountNames; $states; $startTypes)
```
A generic dispatcher over the result, since the arrays are parallel:
```4d
For ($i; 1; Size of array($names))
	ALERT($names{$i}+" — "+$displayNames{$i}+" ("+String($states{$i})+")")
End for 
```

---

## Error handling & troubleshooting

- **Non-zero result ≠ a 4D error, it's a raw Windows error code.** Every command except `SERVICE GET LIST` returns the value straight from `GetLastError`. `0` is success; look up any other value under "System Error Codes" in Microsoft's documentation (common ones: `5` access denied, `1060` service does not exist, `1056` service already running, `1062` service not started).
- **Access denied (`5`) almost always means the host 4D process isn't elevated.** Every command except the `Get`-prefixed ones and `SERVICE GET LIST` needs administrator rights; run 4D "as Administrator" if you're scripting configuration changes.
- **An empty `displayName`/`accountName` result isn't necessarily a failure.** Always check the `Longint` result code rather than treating an empty string as an error — for drivers and default-account services, an empty account name is the correct, expected value.
- **`SERVICE Start`/`SERVICE Stop` return before the transition finishes.** They only *request* the state change. If your logic depends on the service actually being fully started or stopped, poll `SERVICE Get state` (and optionally sleep for `waitHint` milliseconds between polls) rather than assuming the state has changed the instant the call returns.
- **`SERVICE Pause`/`SERVICE Resume` fail on services that don't support pausing** — this is expected for many services (4D Server included), not a bug in the call.
- **`SERVICE GET LIST` has no result — don't write `$err:=SERVICE GET LIST(...)`.** It's the one command in this plugin with no return value.
- **This plugin is Windows-only.** Calling any of these commands on macOS returns without doing anything (a `0`/success-looking result with no actual side effect for the `Set`/control commands, and empty arrays for `SERVICE GET LIST`) — guard for platform in cross-platform code rather than relying on an error to detect macOS.

---

## Quick reference

```4d
// Register, start, inspect, and tear down a service
$pathName:=Application file:C491+" -s \""+Structure file:C489+"\""
$serviceName:="4DS sample"

$err:=SERVICE Create($pathName; $serviceName; "My Super Service"; ""; ""; SERVICE_AUTO_START; SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS)
$err:=SERVICE Start($serviceName)
$err:=SERVICE Get state($serviceName; $state; $waitHint)
$err:=SERVICE Stop($serviceName)
$err:=SERVICE Delete($serviceName)

// Read-only inspection, no admin rights needed
$err:=SERVICE Get display name($serviceName; $displayName)
$err:=SERVICE Get path($serviceName; $pathName)
$err:=SERVICE Get start type($serviceName; $startType)
$err:=SERVICE Get type($serviceName; $type)

// Full machine-wide enumeration
SERVICE GET LIST($names; $displayNames; $accountNames; $states; $startTypes)
```
