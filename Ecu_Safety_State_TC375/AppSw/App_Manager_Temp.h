#ifndef APPSW_APP_MANAGER_TEMP_H_
#define APPSW_APP_MANAGER_TEMP_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"
#include "Base_Driver_Ds18b20.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_Temp_Init(void);
void App_Manager_Temp_Run(void);

boolean App_Manager_Temp_RequestUpdate(void);

boolean App_Manager_Temp_IsBusy(void);
boolean App_Manager_Temp_IsDataValid(void);
boolean App_Manager_Temp_IsError(void);

boolean App_Manager_Temp_GetLatestTemp_X10(sint16 *temp_x10);
Base_Driver_Ds18b20_State_t App_Manager_Temp_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* APPSW_APP_MANAGER_TEMP_H_ */
