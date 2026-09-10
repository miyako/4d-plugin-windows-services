#include "4DPluginAPI.h"
#include "4DPlugin.h"

// --- Service Management
void SERVICE_Set_display_name(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Get_display_name(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Change_account(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Create(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Delete(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Get_state(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Set_path(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Get_path(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Set_start_type(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Get_start_type(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Set_type(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Get_type(sLONG_PTR *pResult, PackagePtr pParams);

// --- Service Control
void SERVICE_Resume(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Pause(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Stop(sLONG_PTR *pResult, PackagePtr pParams);
void SERVICE_Start(sLONG_PTR *pResult, PackagePtr pParams);

// --- Service Database
void SERVICE_GET_LIST(sLONG_PTR *pResult, PackagePtr pParams);
