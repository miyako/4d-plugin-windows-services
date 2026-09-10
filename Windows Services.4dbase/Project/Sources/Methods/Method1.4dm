//%attributes = {}
//require administrator access rights

$serviceName:="4DS sample"

$err:=SERVICE Start($serviceName)

If (False:C215)
	//not applicable to 4D Server
	$err:=SERVICE Pause($serviceName)
	$err:=SERVICE Resume($serviceName)
End if 

$err:=SERVICE Stop($serviceName)