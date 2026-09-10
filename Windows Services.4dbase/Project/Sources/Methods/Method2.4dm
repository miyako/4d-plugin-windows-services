//%attributes = {}
//require no special access rights

$serviceName:="4DS sample"

$err:=SERVICE Get display name($serviceName; $displayName)
$err:=SERVICE Get type($serviceName; $type)
$err:=SERVICE Get start type($serviceName; $startType)
$err:=SERVICE Get path($serviceName; $pathName)
$err:=SERVICE Get state($serviceName; $state; $waitHint)

//require administrator access rights

$err:=SERVICE Set display name($serviceName; "uuuuu")
$err:=SERVICE Set type($serviceName; SERVICE_WIN32_OWN_PROCESS)
$err:=SERVICE Set start type($serviceName; SERVICE_AUTO_START)
$err:=SERVICE Set path($serviceName; Application file:C491+" -s \""+Structure file:C489+"\"")
