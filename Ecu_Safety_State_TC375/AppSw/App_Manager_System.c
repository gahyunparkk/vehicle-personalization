/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Manager_System.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define APP_MANAGER_SYSTEM_DENIED_TIMEOUT_MS      (10000U)
#define APP_MANAGER_SYSTEM_SETUP_TIMEOUT_MS       (3000U)

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    App_Manager_System_State_t  current_state;
    App_Manager_System_Reason_t last_reason;
    uint32                      state_enter_time_ms;
} App_Manager_System_Context_t;

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
static App_Manager_System_Context_t g_app_manager_system_context;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Manager_System_ResetOutput(App_Manager_System_Output_t *output);
static void App_Manager_System_SetState(App_Manager_System_State_t next_state,
                                        App_Manager_System_Reason_t reason,
                                        uint32 now_ms);
static uint32 App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_System_Init(void)
{
    g_app_manager_system_context.current_state       = APP_MANAGER_SYSTEM_STATE_SLEEP;
    g_app_manager_system_context.last_reason         = APP_MANAGER_SYSTEM_REASON_NONE;
    g_app_manager_system_context.state_enter_time_ms = 0U;
}

App_Manager_System_State_t App_Manager_System_GetState(void)
{
    return g_app_manager_system_context.current_state;
}

App_Manager_System_Reason_t App_Manager_System_GetReason(void)
{
    return g_app_manager_system_context.last_reason;
}

void App_Manager_System_Run(uint32 now_ms,
                            const App_Manager_System_Input_t *input,
                            App_Manager_System_Output_t *output)
{
    App_Manager_System_State_t prev_state;

    if ((input == NULL_PTR) || (output == NULL_PTR))
    {
        return;
    }

    prev_state = g_app_manager_system_context.current_state;

    App_Manager_System_ResetOutput(output);

    switch (g_app_manager_system_context.current_state)
    {
    case APP_MANAGER_SYSTEM_STATE_SLEEP:
        if (input->hazard_detected == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_EMERGENCY,
                                        APP_MANAGER_SYSTEM_REASON_HAZARD_DETECTED,
                                        now_ms);
        }
        else if (input->rfid_fail_3times == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_DENIED,
                                        APP_MANAGER_SYSTEM_REASON_INVALID_RFID_3TIMES,
                                        now_ms);
        }
        else if (input->rfid_auth_success == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SETUP,
                                        APP_MANAGER_SYSTEM_REASON_VALID_RFID,
                                        now_ms);
        }
        else
        {
            /* keep state */
        }
        break;

    case APP_MANAGER_SYSTEM_STATE_SETUP:
        output->cmd_door_unlock  = TRUE;
        output->cmd_profile_load = TRUE;

        if (input->hazard_detected == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_EMERGENCY,
                                        APP_MANAGER_SYSTEM_REASON_HAZARD_DETECTED,
                                        now_ms);
        }
        else if (input->profile_load_done == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_ACTIVATED,
                                        APP_MANAGER_SYSTEM_REASON_PROFILE_LOAD_DONE,
                                        now_ms);
        }
        else if (App_Manager_System_GetElapsed(now_ms,
                                               g_app_manager_system_context.state_enter_time_ms) >=
                 APP_MANAGER_SYSTEM_SETUP_TIMEOUT_MS)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SLEEP,
                                        APP_MANAGER_SYSTEM_REASON_SETUP_TIMEOUT,
                                        now_ms);
        }
        else
        {
            /* keep state */
        }
        break;

    case APP_MANAGER_SYSTEM_STATE_ACTIVATED:
        output->cmd_apply_profile = TRUE;

        if (input->hazard_detected == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_EMERGENCY,
                                        APP_MANAGER_SYSTEM_REASON_HAZARD_DETECTED,
                                        now_ms);
        }
        else if (input->deactivate_request == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SLEEP,
                                        APP_MANAGER_SYSTEM_REASON_DEACTIVATE_REQUEST,
                                        now_ms);
        }
        else
        {
            /* keep state */
        }
        break;

    case APP_MANAGER_SYSTEM_STATE_DENIED:
        output->cmd_alert = TRUE;

        if (input->hazard_detected == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_EMERGENCY,
                                        APP_MANAGER_SYSTEM_REASON_HAZARD_DETECTED,
                                        now_ms);
        }
        else if (App_Manager_System_GetElapsed(now_ms,
                                               g_app_manager_system_context.state_enter_time_ms) >=
                 APP_MANAGER_SYSTEM_DENIED_TIMEOUT_MS)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SLEEP,
                                        APP_MANAGER_SYSTEM_REASON_DENIED_TIMEOUT,
                                        now_ms);
        }
        else
        {
            /* keep state */
        }
        break;

    case APP_MANAGER_SYSTEM_STATE_EMERGENCY:
        output->cmd_emergency_profile = TRUE;
        output->cmd_door_unlock       = TRUE;
        output->cmd_alert             = TRUE;

        if (input->emergency_clear == TRUE)
        {
            App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SLEEP,
                                        APP_MANAGER_SYSTEM_REASON_EMERGENCY_CLEAR,
                                        now_ms);
        }
        else
        {
            /* keep state */
        }
        break;

    default:
        App_Manager_System_SetState(APP_MANAGER_SYSTEM_STATE_SLEEP,
                                    APP_MANAGER_SYSTEM_REASON_NONE,
                                    now_ms);
        break;
    }

    output->current_state      = g_app_manager_system_context.current_state;
    output->transition_reason  = g_app_manager_system_context.last_reason;

    if (prev_state != g_app_manager_system_context.current_state)
    {
        output->state_changed      = TRUE;
        output->tx_state_broadcast = TRUE;
    }
}

static void App_Manager_System_ResetOutput(App_Manager_System_Output_t *output)
{
    output->current_state          = g_app_manager_system_context.current_state;
    output->transition_reason      = g_app_manager_system_context.last_reason;
    output->state_changed          = FALSE;
    output->tx_state_broadcast     = FALSE;
    output->cmd_door_unlock        = FALSE;
    output->cmd_profile_load       = FALSE;
    output->cmd_apply_profile      = FALSE;
    output->cmd_emergency_profile  = FALSE;
    output->cmd_alert              = FALSE;
}

static void App_Manager_System_SetState(App_Manager_System_State_t next_state,
                                        App_Manager_System_Reason_t reason,
                                        uint32 now_ms)
{
    if (g_app_manager_system_context.current_state != next_state)
    {
        g_app_manager_system_context.current_state       = next_state;
        g_app_manager_system_context.last_reason         = reason;
        g_app_manager_system_context.state_enter_time_ms = now_ms;
    }
}

static uint32 App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms)
{
    return (now_ms - base_ms);
}
