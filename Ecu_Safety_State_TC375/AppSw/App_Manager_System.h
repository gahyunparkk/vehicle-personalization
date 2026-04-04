#ifndef APP_MANAGER_SYSTEM_H_
#define APP_MANAGER_SYSTEM_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum
{
    APP_MANAGER_SYSTEM_STATE_SLEEP = 0,
    APP_MANAGER_SYSTEM_STATE_SETUP,
    APP_MANAGER_SYSTEM_STATE_ACTIVATED,
    APP_MANAGER_SYSTEM_STATE_DENIED,
    APP_MANAGER_SYSTEM_STATE_EMERGENCY
} App_Manager_System_State_t;

typedef enum
{
    APP_MANAGER_SYSTEM_REASON_NONE = 0,
    APP_MANAGER_SYSTEM_REASON_VALID_RFID,
    APP_MANAGER_SYSTEM_REASON_INVALID_RFID_3TIMES,
    APP_MANAGER_SYSTEM_REASON_PROFILE_LOAD_DONE,
    APP_MANAGER_SYSTEM_REASON_SETUP_TIMEOUT,
    APP_MANAGER_SYSTEM_REASON_DEACTIVATE_REQUEST,
    APP_MANAGER_SYSTEM_REASON_HAZARD_DETECTED,
    APP_MANAGER_SYSTEM_REASON_DENIED_TIMEOUT,
    APP_MANAGER_SYSTEM_REASON_EMERGENCY_CLEAR
} App_Manager_System_Reason_t;

/*
 * input은 각 ECU에서 수신한 CAN 정보를 상위 FSM이 해석하기 쉬운 형태로 정리한 값이다.
 * event성 입력은 1주기 펄스처럼 넣는 것을 가정한다.
 */

typedef struct
{
    boolean rfid_auth_success;
    boolean rfid_fail_3times;

    boolean profile_load_done;

    boolean deactivate_request;

    boolean hazard_detected;
    boolean emergency_clear;
} App_Manager_System_Input_t;

typedef struct
{
    App_Manager_System_State_t  current_state;
    App_Manager_System_Reason_t transition_reason;

    boolean state_changed;
    boolean tx_state_broadcast;

    boolean cmd_door_unlock;
    boolean cmd_profile_load;
    boolean cmd_apply_profile;
    boolean cmd_emergency_profile;
    boolean cmd_alert;
} App_Manager_System_Output_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_System_Init(void);

void App_Manager_System_Run(uint32 now_ms,
                            const App_Manager_System_Input_t *input,
                            App_Manager_System_Output_t *output);

App_Manager_System_State_t App_Manager_System_GetState(void);
App_Manager_System_Reason_t App_Manager_System_GetReason(void);

#endif /* APP_MANAGER_SYSTEM_H_ */
