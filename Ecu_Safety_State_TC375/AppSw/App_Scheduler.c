#include "App_Scheduler.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define APP_SCHEDULER_DUMMY_TEST    (1u)

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Driver_Stm.h"

#include "App_Manager_System.h"
#include "App_Manager_Temp.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"
#include "Shared_Util_Time.h"

#if (APP_SCHEDULER_DUMMY_TEST == 0u)
#include "MCMCAN_FD.h"
#else
#include "UART_Config.h"
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Run_1ms(void);
static void App_Scheduler_Run_10ms(void);
static void App_Scheduler_Run_100ms(void);
static void App_Scheduler_Run_1s(void);
static void App_Scheduler_Run_10s(void);

static void App_Scheduler_Task_CanRx(void);
static void App_Scheduler_Task_Temp(void);
static void App_Scheduler_Task_System(void);
static void App_Scheduler_Task_CanTx(void);

static void App_Scheduler_BuildStateInfo(Shared_System_State_Info_t *state_info);

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
static const char *App_Scheduler_GetStateString(uint8 state);
static void        App_Scheduler_PrintStatus(uint32 elapsed_ms,
                                             const Shared_System_State_Info_t *state_info);
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/
static uint32                      g_app_scheduler_now_ms = 0u;
static sint16                      g_app_scheduler_local_temperature_x10 = 250; /* default 25.0C */
static App_Manager_System_Input_t  g_app_scheduler_system_input;
static App_Manager_System_Output_t g_app_scheduler_system_output;

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
static uint32  g_app_scheduler_test_start_ms = 0u;
static boolean g_app_scheduler_auth_sent = FALSE;
static boolean g_app_scheduler_shutdown_sent = FALSE;

static sint32  g_app_scheduler_prev_state = -1;
static sint32  g_app_scheduler_prev_profile_index = -1;
static sint32  g_app_scheduler_prev_out_temperature = -128;
static uint32  g_app_scheduler_prev_log_ms = 0u;
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
void App_Scheduler_Init(void)
{
    Base_Driver_Stm_Init();

    App_Manager_System_Init();
    App_Manager_Temp_Init();

#if (APP_SCHEDULER_DUMMY_TEST == 0u)
    initMcmcan();
#else
    g_app_scheduler_now_ms              = Shared_Util_Time_GetNowMs();
    g_app_scheduler_test_start_ms       = g_app_scheduler_now_ms;
    g_app_scheduler_auth_sent           = FALSE;
    g_app_scheduler_shutdown_sent       = FALSE;
    g_app_scheduler_prev_state          = -1;
    g_app_scheduler_prev_profile_index  = -1;
    g_app_scheduler_prev_out_temperature = -128;
    g_app_scheduler_prev_log_ms         = 0u;
#endif

    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

    g_app_scheduler_system_output.current_state        = (uint8)SHARED_SYSTEM_STATE_SLEEP;
    g_app_scheduler_system_output.temperature          = 25;
    g_app_scheduler_system_output.active_profile_index = SHARED_PROFILE_INDEX_INVALID;
    App_Manager_System_GetProfileTable(&g_app_scheduler_system_output.profile_table);
}

void App_Scheduler_Run(void)
{
    Base_Driver_Stm_SchedulingFlag_t flags;

    Base_Driver_Stm_GetAndClearSchedulingFlags(&flags);

    if (flags.scheduling_1ms_flag != 0u)
    {
        App_Scheduler_Run_1ms();
    }

    if (flags.scheduling_10ms_flag != 0u)
    {
        App_Scheduler_Run_10ms();
    }

    if (flags.scheduling_100ms_flag != 0u)
    {
        App_Scheduler_Run_100ms();
    }

    if (flags.scheduling_1s_flag != 0u)
    {
        App_Scheduler_Run_1s();
    }

    if (flags.scheduling_10s_flag != 0u)
    {
        App_Scheduler_Run_10s();
    }
}

static void App_Scheduler_Run_1ms(void)
{
    /* watchdog, debounce 등 아주 짧은 작업 */
}

static void App_Scheduler_Run_10ms(void)
{
    g_app_scheduler_now_ms = Shared_Util_Time_GetNowMs();

    App_Scheduler_Task_CanRx();
    App_Scheduler_Task_Temp();
    App_Scheduler_Task_System();
    App_Scheduler_Task_CanTx();
}

static void App_Scheduler_Run_100ms(void)
{
    /* diagnostic / health monitoring */
}

static void App_Scheduler_Run_1s(void)
{
    boolean request_result;

    request_result = App_Manager_Temp_RequestUpdate();

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
    UART_Printf("[SCH] temp request at %lu ms (%s)\r\n",
                g_app_scheduler_now_ms - g_app_scheduler_test_start_ms,
                (request_result == TRUE) ? "accepted" : "busy");
#endif
}

static void App_Scheduler_Run_10s(void)
{
    /* statistics / maintenance */
}

/*********************************************************************************************************************/
/*------------------------------------------------Task Functions-----------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Task_CanRx(void)
{
    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
    uint32 elapsed_ms = g_app_scheduler_now_ms - g_app_scheduler_test_start_ms;

    if ((g_app_scheduler_auth_sent == FALSE) && (elapsed_ms >= 3000u))
    {
        g_app_scheduler_system_input.auth_event_valid     = TRUE;
        g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_1;
        g_app_scheduler_auth_sent                         = TRUE;

        UART_Printf("[DUMMY RX] auth event at %lu ms, profile=%u\r\n",
                    elapsed_ms,
                    SHARED_PROFILE_INDEX_1);
    }

    if ((g_app_scheduler_shutdown_sent == FALSE) && (elapsed_ms >= 12000u))
    {
        g_app_scheduler_system_input.shutdown_request = TRUE;
        g_app_scheduler_shutdown_sent                 = TRUE;

        UART_Printf("[DUMMY RX] shutdown event at %lu ms\r\n", elapsed_ms);
    }
#else
    /*
     * TODO: 실제 CAN RX 처리
     *
     * 예:
     * if (receiveCanMessage(rx_data) != FALSE)
     * {
     *     switch (g_mcmcan.rxMsg.messageId)
     *     {
     *         case MSG_xxx_AUTH_RESULT:
     *             g_app_scheduler_system_input.auth_event_valid = TRUE;
     *             break;
     *
     *         case MSG_xxx_SHUTDOWN_REQ:
     *             g_app_scheduler_system_input.shutdown_request = TRUE;
     *             break;
     *
     *         case MSG_xxx_ACTIVE_PROFILE:
     *             g_app_scheduler_system_input.active_profile_index = (uint8)rx_data[0];
     *             break;
     *
     *         default:
     *             break;
     *     }
     * }
     */
#endif
}

static void App_Scheduler_Task_Temp(void)
{
    sint16 temperature_x10;

    App_Manager_Temp_Run();

    if (App_Manager_Temp_GetLatestTemp_X10(&temperature_x10) == TRUE)
    {
        g_app_scheduler_local_temperature_x10 = temperature_x10;
    }
}

static void App_Scheduler_Task_System(void)
{
    App_Manager_System_Run(g_app_scheduler_now_ms,
                           g_app_scheduler_local_temperature_x10,
                           &g_app_scheduler_system_input,
                           &g_app_scheduler_system_output);
}

static void App_Scheduler_Task_CanTx(void)
{
    Shared_System_State_Info_t state_info;

    App_Scheduler_BuildStateInfo(&state_info);

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
    uint32 elapsed_ms = g_app_scheduler_now_ms - g_app_scheduler_test_start_ms;

    if (((sint32)state_info.current_state != g_app_scheduler_prev_state) ||
        ((sint32)state_info.active_profile_index != g_app_scheduler_prev_profile_index) ||
        ((sint32)g_app_scheduler_system_output.temperature != g_app_scheduler_prev_out_temperature) ||
        ((elapsed_ms - g_app_scheduler_prev_log_ms) >= 1000u))
    {
        App_Scheduler_PrintStatus(elapsed_ms, &state_info);

        g_app_scheduler_prev_state           = (sint32)state_info.current_state;
        g_app_scheduler_prev_profile_index   = (sint32)state_info.active_profile_index;
        g_app_scheduler_prev_out_temperature = (sint32)g_app_scheduler_system_output.temperature;
        g_app_scheduler_prev_log_ms          = elapsed_ms;
    }
#else
    /*
     * TODO: 실제 CAN TX 처리
     *
     * 상태 정보 8 bytes
     * sendCanMessage(MSG_STATE_INFO, (uint32 *)&state_info, sizeof(Shared_System_State_Info_t));
     *
     * 온도는 별도 메시지
     * {
     *     sint8 temp_out = g_app_scheduler_system_output.temperature;
     *     sendCanMessage(MSG_TEMP_VALUE, (uint32 *)&temp_out, 1u);
     * }
     */

    (void)state_info;
#endif
}

static void App_Scheduler_BuildStateInfo(Shared_System_State_Info_t *state_info)
{
    if (state_info == NULL_PTR)
    {
        return;
    }

    state_info->current_state        = g_app_scheduler_system_output.current_state;
    state_info->active_profile_index = g_app_scheduler_system_output.active_profile_index;
    state_info->reserved0            = 0u;
    state_info->reserved1            = 0u;
    state_info->reserved2            = 0u;
    state_info->reserved3            = 0u;
    state_info->reserved4            = 0u;
    state_info->reserved5            = 0u;
}

#if (APP_SCHEDULER_DUMMY_TEST == 1u)
static const char *App_Scheduler_GetStateString(uint8 state)
{
    switch ((Shared_System_State_t)state)
    {
        case SHARED_SYSTEM_STATE_SLEEP:
            return "SLEEP";

        case SHARED_SYSTEM_STATE_SETUP:
            return "SETUP";

        case SHARED_SYSTEM_STATE_ACTIVATED:
            return "ACTIVATED";

        case SHARED_SYSTEM_STATE_SHUTDOWN:
            return "SHUTDOWN";

        case SHARED_SYSTEM_STATE_DENIED:
            return "DENIED";

        case SHARED_SYSTEM_STATE_EMERGENCY:
            return "EMERGENCY";

        default:
            return "UNKNOWN";
    }
}

static void App_Scheduler_PrintStatus(uint32 elapsed_ms,
                                      const Shared_System_State_Info_t *state_info)
{
    sint16 abs_temp_x10;

    if (state_info == NULL_PTR)
    {
        return;
    }

    abs_temp_x10 = g_app_scheduler_local_temperature_x10;

    if (abs_temp_x10 < 0)
    {
        abs_temp_x10 = (sint16)(-abs_temp_x10);
    }

    UART_Printf("[SCH] t=%lu ms, state=%u(%s), profile=%u, local_temp=%s%d.%d C, out_temp=%d C\r\n",
                elapsed_ms,
                state_info->current_state,
                App_Scheduler_GetStateString(state_info->current_state),
                state_info->active_profile_index,
                (g_app_scheduler_local_temperature_x10 < 0) ? "-" : "",
                (int)(abs_temp_x10 / 10),
                (int)(abs_temp_x10 % 10),
                (int)g_app_scheduler_system_output.temperature);
}
#endif
