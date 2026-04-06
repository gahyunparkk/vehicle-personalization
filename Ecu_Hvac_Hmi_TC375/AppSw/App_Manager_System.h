#ifndef APP_MANAGER_SYSTEM_H_
#define APP_MANAGER_SYSTEM_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define APP_MANAGER_SYSTEM_SETUP_HOLD_MS     (100U)
#define APP_MANAGER_SYSTEM_DENIED_TIMEOUT_MS (5000U)

#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_HIGH_X10       (500)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_CLEAR_HIGH_X10 (400)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_LOW_X10        (-200)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_CLEAR_LOW_X10  (-100)

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
  boolean auth_event_valid;
  boolean shutdown_request;
  uint8 active_profile_index;
} App_Manager_System_Input_t;

typedef struct
{
  uint8 current_state;
  sint8 temperature;
  uint8 active_profile_index;
  Shared_Profile_Table_t profile_table;
} App_Manager_System_Output_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_System_Init(void);

void App_Manager_System_Run(uint32 now_ms,
                            sint16 local_temperature_x10,
                            const App_Manager_System_Input_t *input,
                            App_Manager_System_Output_t *output);

Shared_System_State_t App_Manager_System_GetState(void);
uint8 App_Manager_System_GetActiveProfileIndex(void);
void App_Manager_System_SetActiveProfileIndex(uint8 idx);

/* 외부에서 CAN 수신 등의 결과로 RAM 프로필 테이블을 갱신할 때 사용 */
void App_Manager_System_UpdateProfileTable(const Shared_Profile_Table_t *profile_table);

/* 현재 RAM에 유지 중인 프로필 테이블 조회 */
void App_Manager_System_GetProfileTable(Shared_Profile_Table_t *profile_table);

#endif /* APP_MANAGER_SYSTEM_H_ */
