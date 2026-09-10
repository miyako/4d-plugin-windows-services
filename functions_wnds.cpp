#include "functions_wnds.h"

#if VERSIONWIN

#include <aclapi.h>

int _controlService(LPCTSTR lpServiceName, DWORD dwDesiredAccess, DWORD dwControl){
	
	int returnValue = 0;
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS status;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT);
	
	if(!schSCManager){
		returnValue = GetLastError();
	}else{
		schService = OpenService( 
								 schSCManager, 
								 lpServiceName,  
								 dwDesiredAccess);
		
		if(!schService){	
			returnValue = GetLastError();
		}else{	
			
			if(!ControlService(schService, dwControl, &status)){
				returnValue = GetLastError();
			}
			
			CloseServiceHandle(schService);	
			
		}
		
		CloseServiceHandle(schSCManager);	
		
	}
	
	return returnValue;
}
#endif

#if VERSIONWIN
int _getServiceConfig(LPCTSTR lpServiceName,
					  DWORD *dwServiceType,
					  DWORD *dwStartType,
					  C_TEXT *binaryPathName,
					  C_TEXT *serviceStartName,
					  C_TEXT *displayName){
	
	int returnValue = 0;
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT);
	
	if(!schSCManager){
		returnValue = GetLastError();
	}else{
		schService = OpenService( 
								 schSCManager, 
								 lpServiceName,  
								 SERVICE_QUERY_CONFIG);
		
		if(!schService){	
			returnValue = GetLastError();
		}else{
			
			DWORD dwBytesNeeded, cbBufSize;
			
			if(!QueryServiceConfig(schService,
								   NULL,
								   0,
								   &dwBytesNeeded)){
				
				cbBufSize = dwBytesNeeded;
				
				std::vector<unsigned int> buf(cbBufSize);
				
				LPQUERY_SERVICE_CONFIG serviceConfig = (LPQUERY_SERVICE_CONFIG)&buf[0];
				
				if(!QueryServiceConfig(schService,
									   serviceConfig,
									   cbBufSize,
									   &dwBytesNeeded)){
					returnValue = GetLastError();
				}else{
					if(dwServiceType)
						*dwServiceType = serviceConfig->dwServiceType;
					if(dwStartType)
						*dwStartType = serviceConfig->dwStartType;
					if(binaryPathName)
						binaryPathName->setUTF16String((const PA_Unichar *)serviceConfig->lpBinaryPathName, wcslen(serviceConfig->lpBinaryPathName));
					if(serviceStartName)
						serviceStartName->setUTF16String((const PA_Unichar *)serviceConfig->lpServiceStartName, wcslen(serviceConfig->lpServiceStartName));
					if(displayName)
						displayName->setUTF16String((const PA_Unichar *)serviceConfig->lpDisplayName, wcslen(serviceConfig->lpDisplayName));
				}				
			}
			
			CloseServiceHandle(schService);	
			
		}
		
		CloseServiceHandle(schSCManager);
		
	}
	
	return returnValue;
}
#endif

#if VERSIONWIN
int _setServiceConfig(LPCTSTR lpServiceName,
					  DWORD dwServiceType,
					  DWORD dwStartType,
					  LPCTSTR lpBinaryPathName,
					  LPCTSTR lpServiceStartName,
					  LPCTSTR lpPassword,
					  LPCTSTR lpDisplayName){
	
	int returnValue = 0;
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT);
	
	if(!schSCManager){
		returnValue = GetLastError();
	}else{
		schService = OpenService( 
								 schSCManager, 
								 lpServiceName,  
								 SERVICE_CHANGE_CONFIG);
		
		if(!schService){	
			returnValue = GetLastError();
		}else{
			if(!ChangeServiceConfig(schService,
									dwServiceType,
									dwStartType,
									SERVICE_NO_CHANGE,
									NULL,	//lpBinaryPathName
									NULL,	//lpLoadOrderGroup
									NULL,	//lpdwTagId
									NULL,	//lpDependencies
									lpServiceStartName,
									lpPassword,
									lpDisplayName)){
				returnValue = GetLastError();
			}
			
			CloseServiceHandle(schService);	
			
		}
		
		CloseServiceHandle(schSCManager);
		
	}
	
	return returnValue;
}
#endif

#if VERSIONWIN		   
int _changeAccess(LPCTSTR lpServiceName){
	
	int returnValue = 0;
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_ALL_ACCESS);
	
	if(!schSCManager){
		returnValue = GetLastError();
	}else{
		schService = OpenService(schSCManager, 
								 lpServiceName,  
								 READ_CONTROL|WRITE_DAC);
		
		if(!schService){	
			returnValue = GetLastError();
		}else{	
			
			PSECURITY_DESCRIPTOR psd = NULL;
			DWORD dwSize, dwBytesNeeded  = 0;
			
			if(!QueryServiceObjectSecurity(schService,
										   DACL_SECURITY_INFORMATION, 
										   &psd,           // using NULL does not work on all versions
										   0, 
										   &dwBytesNeeded)){
				
				dwSize = dwBytesNeeded;
				
				std::vector<unsigned int> buf(dwSize);
				
				psd = (PSECURITY_DESCRIPTOR)&buf[0];
				
				if(!QueryServiceObjectSecurity(schService,
											   DACL_SECURITY_INFORMATION, 
											   psd, 
											   dwSize, 
											   &dwBytesNeeded)){
					returnValue = GetLastError();
				}else{
					
					BOOL bDaclPresent = FALSE;
					PACL pacl = NULL;
					PACL pNewAcl = NULL;
					BOOL bDaclDefaulted = FALSE;
					
					if(!GetSecurityDescriptorDacl(psd, 
												  &bDaclPresent, 
												  &pacl,
												  &bDaclDefaulted)){
						returnValue = GetLastError();
					}else{
						EXPLICIT_ACCESS ea;
						
						BuildExplicitAccessWithName(&ea, 
													L"GUEST",
													SERVICE_START|SERVICE_STOP|
													SERVICE_CHANGE_CONFIG|SERVICE_QUERY_CONFIG|DELETE,
													SET_ACCESS, 
													NO_INHERITANCE);
						
						if(SetEntriesInAcl(1, &ea, pacl, &pNewAcl)!=ERROR_SUCCESS){
							returnValue = GetLastError();
						}else{
							SECURITY_DESCRIPTOR  sd;
							if(!InitializeSecurityDescriptor(&sd, 
															 SECURITY_DESCRIPTOR_REVISION)){
								returnValue = GetLastError();
							}else{
								if(SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE)){
									if(!SetServiceObjectSecurity(schService, 
																 DACL_SECURITY_INFORMATION, 
																 &sd)){
										returnValue = GetLastError();	
									}
								}
							}
							LocalFree((HLOCAL)pNewAcl);
						}
					}
				}					
			}
			
			CloseServiceHandle(schService);	
			
		}
		
		CloseServiceHandle(schSCManager);	
		
	}	
	
	return returnValue;
}
#endif

void SERVICE_Set_display_name(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_TEXT serviceDisplayName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	serviceDisplayName.fromParamAtIndex(pParams, 2);
	
#if VERSIONWIN	
	returnValue.setIntValue(_setServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  SERVICE_NO_CHANGE,
											  SERVICE_NO_CHANGE,
											  NULL,
											  NULL,
											  NULL,
											  (LPCTSTR)serviceDisplayName.getUTF16StringPtr()));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Get_display_name(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_TEXT serviceDisplayName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	
	returnValue.setIntValue(_getServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  NULL,
											  NULL,
											  NULL,
											  NULL,
											  &serviceDisplayName));	
#endif
	
	serviceDisplayName.toParamAtIndex(pParams, 2);
	returnValue.setReturn(pResult);	
}

void SERVICE_Change_account(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_TEXT accountName;
	C_TEXT accountPassword;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	accountName.fromParamAtIndex(pParams, 2);
	accountPassword.fromParamAtIndex(pParams, 3);
	
#if VERSIONWIN
	
	LPCTSTR lpServiceStartName = NULL;
	if(accountName.getUTF16Length())
		lpServiceStartName = (LPCTSTR)accountName.getUTF16StringPtr();		
	
	LPCTSTR lpPassword = NULL;
	if(accountPassword.getUTF16Length())
		lpPassword = (LPCTSTR)accountPassword.getUTF16StringPtr();
	
	returnValue.setIntValue(_setServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  SERVICE_NO_CHANGE,
											  SERVICE_NO_CHANGE,
											  NULL,
											  lpServiceStartName,
											  lpPassword,
											  NULL));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Create(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT pathName;
	C_TEXT serviceName;
	C_TEXT serviceDisplayName;
	C_TEXT serviceAccout;
	C_TEXT serviceAccountPassword;
	C_LONGINT serviceStartType;
	C_LONGINT serviceType;
	C_LONGINT returnValue;
	
	pathName.fromParamAtIndex(pParams, 1);
	serviceName.fromParamAtIndex(pParams, 2);
	serviceDisplayName.fromParamAtIndex(pParams, 3);
	serviceAccout.fromParamAtIndex(pParams, 4);
	serviceAccountPassword.fromParamAtIndex(pParams, 5);
	serviceStartType.fromParamAtIndex(pParams, 6);
	serviceType.fromParamAtIndex(pParams, 7);
	
#if VERSIONWIN
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_ALL_ACCESS);
	
	if(!schSCManager){
		returnValue.setIntValue(GetLastError());
	}else{
		
		LPCTSTR lpServiceStartName = NULL;
		if(serviceAccout.getUTF16Length())
			lpServiceStartName = (LPCTSTR)serviceAccout.getUTF16StringPtr();		
		
		LPCTSTR lpPassword = NULL;
		if(serviceAccountPassword.getUTF16Length())
			lpPassword = (LPCTSTR)serviceAccountPassword.getUTF16StringPtr();
		
		schService = CreateService( 
								   schSCManager,
								   (LPCTSTR)serviceName.getUTF16StringPtr(), 
								   (LPCTSTR)serviceDisplayName.getUTF16StringPtr(), 
								   SERVICE_ALL_ACCESS,   
								   (DWORD)serviceType.getIntValue(), 
								   (DWORD)serviceStartType.getIntValue(),   
								   SERVICE_ERROR_NORMAL,
								   (LPCTSTR)pathName.getUTF16StringPtr(), 
								   NULL,                      // no load ordering group 
								   NULL,                      // no tag identifier 
								   NULL,                      // no dependencies 
								   lpServiceStartName,
								   lpPassword);
		
		if(!schService){	
			returnValue.setIntValue(GetLastError());
		}else{
			CloseServiceHandle(schService);				
		}
		
		//_changeAccess((LPCTSTR)serviceName.getUTF16StringPtr());
		
		CloseServiceHandle(schSCManager);
		
	}
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Delete(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_ALL_ACCESS);
	
	if(!schSCManager){
		returnValue.setIntValue(GetLastError());
	}else{
		schService = OpenService( 
								 schSCManager, 
								 (LPCTSTR)serviceName.getUTF16StringPtr(),  
								 DELETE);
		
		if(!schService){	
			returnValue.setIntValue(GetLastError());
		}else{
			if(!DeleteService(schService)){
				returnValue.setIntValue(GetLastError());
			}
			
			CloseServiceHandle(schService);	
			
		}
		
		CloseServiceHandle(schSCManager);
		
	}
#endif
	returnValue.setReturn(pResult);
}

void SERVICE_Get_state(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT serviceState;
	C_LONGINT waitHint;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT);
	
	
	
	if(!schSCManager){
		returnValue.setIntValue(GetLastError());
	}else{
		schService = OpenService( 
								 schSCManager, 
								 (LPCTSTR)serviceName.getUTF16StringPtr(),  
								 SERVICE_QUERY_STATUS);
		
		if(!schService){	
			returnValue.setIntValue(GetLastError());
		}else{
			
			SERVICE_STATUS serviceStatus;
			
			if(!QueryServiceStatus(schService,
								   &serviceStatus)){
				returnValue.setIntValue(GetLastError());
			}else{
				serviceState.setIntValue(serviceStatus.dwCurrentState);				
				waitHint.setIntValue(serviceStatus.dwWaitHint);
			}
			
			CloseServiceHandle(schService);	
			
		}	
		
		CloseServiceHandle(schSCManager);
		
	}
#endif
	
	serviceState.toParamAtIndex(pParams, 2);
	waitHint.toParamAtIndex(pParams, 3);
	returnValue.setReturn(pResult);
}

void SERVICE_Set_path(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_TEXT pathName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	pathName.fromParamAtIndex(pParams, 2);
	
#if VERSIONWIN		
	returnValue.setIntValue(_setServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  SERVICE_NO_CHANGE,
											  SERVICE_NO_CHANGE,
											  (LPCTSTR)pathName.getUTF16StringPtr(),
											  NULL,
											  NULL,
											  NULL));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Get_path(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_TEXT pathName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN	
	returnValue.setIntValue(_getServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  NULL,
											  NULL,
											  &pathName,
											  NULL,
											  NULL));	
#endif
	
	pathName.toParamAtIndex(pParams, 2);
	returnValue.setReturn(pResult);
}

void SERVICE_Set_start_type(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT serviceStartType;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	serviceStartType.fromParamAtIndex(pParams, 2);
	
#if VERSIONWIN	
	returnValue.setIntValue(_setServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  SERVICE_NO_CHANGE,
											  (DWORD)serviceStartType.getIntValue(),
											  NULL,
											  NULL,
											  NULL,
											  NULL));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Get_start_type(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT startType;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	DWORD dwStartType;
	
	returnValue.setIntValue(_getServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  NULL,
											  &dwStartType,
											  NULL,
											  NULL,
											  NULL));	
	
	startType.setIntValue(dwStartType);
	startType.toParamAtIndex(pParams, 2);
#endif	
	
	returnValue.setReturn(pResult);
}

void SERVICE_Set_type(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT serviceType;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	serviceType.fromParamAtIndex(pParams, 2);
	
#if VERSIONWIN
	returnValue.setIntValue(_setServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  (DWORD)serviceType.getIntValue(),
											  SERVICE_NO_CHANGE,
											  NULL,
											  NULL,
											  NULL,
											  NULL));
#endif	
	
	returnValue.setReturn(pResult);
}

void SERVICE_Get_type(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT serviceType;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	DWORD dwServiceType;
	
	returnValue.setIntValue(_getServiceConfig((LPCTSTR)serviceName.getUTF16StringPtr(), 
											  &dwServiceType,
											  NULL,
											  NULL,
											  NULL,
											  NULL));	
	serviceType.setIntValue(dwServiceType);	
	
#endif
	
	serviceType.toParamAtIndex(pParams, 2);
	
	returnValue.setReturn(pResult);
}

// -------------------------------- Service Control -------------------------------


void SERVICE_Resume(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	returnValue.setIntValue(_controlService((LPCTSTR)serviceName.getUTF16StringPtr(),
											SERVICE_PAUSE_CONTINUE, 
											SERVICE_CONTROL_CONTINUE));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Pause(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	returnValue.setIntValue(_controlService((LPCTSTR)serviceName.getUTF16StringPtr(),
											SERVICE_PAUSE_CONTINUE, 
											SERVICE_CONTROL_PAUSE));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Stop(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	returnValue.setIntValue(_controlService((LPCTSTR)serviceName.getUTF16StringPtr(),
											SERVICE_STOP, 
											SERVICE_CONTROL_STOP));
#endif
	
	returnValue.setReturn(pResult);
}

void SERVICE_Start(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT serviceName;
	C_LONGINT returnValue;
	
	serviceName.fromParamAtIndex(pParams, 1);
	
#if VERSIONWIN
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT);
	
	if(!schSCManager){
		returnValue.setIntValue(GetLastError());
	}else{
		schService = OpenService( 
								 schSCManager, 
								 (LPCTSTR)serviceName.getUTF16StringPtr(),  
								 SERVICE_START);
		
		if(!schService){	
			returnValue.setIntValue(GetLastError());
		}else{
			
			if(!StartService(schService, 0, NULL)){
				returnValue.setIntValue(GetLastError());
			}
			
			CloseServiceHandle(schService);			
			
		}
		
		CloseServiceHandle(schSCManager);
		
	}
#endif
	returnValue.setReturn(pResult);	
}

// ------------------------------- Service Database -------------------------------


void SERVICE_GET_LIST(sLONG_PTR *pResult, PackagePtr pParams)
{
	ARRAY_TEXT names;
	ARRAY_TEXT displayNames;
	ARRAY_TEXT accountNames;
	ARRAY_LONGINT states;
	ARRAY_LONGINT startTypes;
	
	names.setSize(1);
	displayNames.setSize(1);
	accountNames.setSize(1);
	states.setSize(1);
	startTypes.setSize(1);
	
#if VERSIONWIN
	SC_HANDLE schSCManager;
	
	schSCManager = OpenSCManager(NULL,
								 SERVICES_ACTIVE_DATABASE, 
								 SC_MANAGER_CONNECT|SC_MANAGER_ENUMERATE_SERVICE);
	
	if(schSCManager){
		
		DWORD cbBufSize, cbBytesNeeded, servicesReturned, resumeHandle = 0;
		
		if(!EnumServicesStatus(schSCManager,
							   SERVICE_WIN32|SERVICE_DRIVER,
							   SERVICE_STATE_ALL,
							   NULL,
							   0,
							   &cbBytesNeeded,
							   &servicesReturned,
							   &resumeHandle)){
			
			
			cbBufSize = cbBytesNeeded;
			
			std::vector<unsigned int> buf(cbBufSize);			
			
			LPENUM_SERVICE_STATUS lpServices = (LPENUM_SERVICE_STATUS)&buf[0];
			
			if(EnumServicesStatus(schSCManager,
								  SERVICE_WIN32|SERVICE_DRIVER,
								  SERVICE_STATE_ALL,
								  lpServices,
								  cbBufSize,
								  &cbBytesNeeded,
								  &servicesReturned,
								  &resumeHandle)){
				
				C_TEXT serviceStartName, serviceDisplayName;
				
				for(unsigned int i = 0; i < servicesReturned; ++i){
					
					DWORD dwStartType;
					
					_getServiceConfig(lpServices[i].lpServiceName, 
									  NULL,
									  &dwStartType,
									  NULL,
									  &serviceStartName,
									  &serviceDisplayName);	
					
					names.appendUTF16String((const PA_Unichar *)lpServices[i].lpServiceName);	
					displayNames.appendUTF16String((const PA_Unichar *)serviceDisplayName.getUTF16StringPtr());				
					accountNames.appendUTF16String((const PA_Unichar *)serviceStartName.getUTF16StringPtr());
					states.appendIntValue(lpServices[i].ServiceStatus.dwCurrentState);
					startTypes.appendIntValue(dwStartType);
					
				}
			}			
		}		
	}
#endif	
	
	names.toParamAtIndex(pParams, 1);
	displayNames.toParamAtIndex(pParams, 2);
	accountNames.toParamAtIndex(pParams, 3);
	states.toParamAtIndex(pParams, 4);
	startTypes.toParamAtIndex(pParams, 5);
}
