#ifndef APPSW_APP_TEMPSERVICE_H_
#define APPSW_APP_TEMPSERVICE_H_

#include "Platform_Types.h"
#include "Base_OneWire.h"
#include "Base_Ds18b20_Fsm.h"

void            App_TempService_Init(void);
void            App_TempService_MainFunction(void);

boolean         App_TempService_RequestUpdate(void);

boolean         App_TempService_IsBusy(void);
boolean         App_TempService_IsDataValid(void);
boolean         App_TempService_IsError(void);

boolean         App_TempService_GetLatestTemperatureX10(sint16* temp_x10);
Ds18b20_State_t App_TempService_GetFsmState(void);

#endif /* APPSW_APP_TEMPSERVICE_H_ */
