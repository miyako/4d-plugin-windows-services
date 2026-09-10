//%attributes = {}
//require administrator access rights

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
