#include "App_Scheduler.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define APP_SCHEDULER_DUMMY_TEST    (0u)

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include <string.h>

#include "Base_Driver_Stm.h"

#include "App_Manager_System.h"
#include "App_Manager_Temp.h"
#include "App_Can_Service.h"

#include "Shared_Profile.h"
#include "Shared_System_State.h"
#include "Shared_Util_Time.h"
#include "Shared_Can_Message.h"

#include "MCMCAN_FD.h"
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* scheduler run */
static void App_Scheduler_Run_1ms(void);
static void App_Scheduler_Run_10ms(void);
static void App_Scheduler_Run_100ms(void);
static void App_Scheduler_Run_1s(void);
static void App_Scheduler_Run_10s(void);

/* CAN */
static void    App_Scheduler_Task_CanRx(void);
static void    App_Scheduler_Task_CanTx(void);

/* TEMP */
static void    App_Scheduler_Task_Temp(void);

/* SYSTEM */
static void    App_Scheduler_Task_System(void);

/* HELPER */
static const char *App_Test_StateName(uint8 state);

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/
static uint32                      g_app_scheduler_now_ms = 0u;
static sint16                      g_app_scheduler_local_temperature_x10 = 250; /* default 25.0C */

static App_Manager_System_Input_t  g_app_scheduler_system_input;
static App_Manager_System_Output_t g_app_scheduler_system_output;

static uint32                      g_app_scheduler_last_tx_state_ms = 0u;
static uint32                      g_app_scheduler_last_tx_temp_ms  = 0u;

static boolean                     g_app_scheduler_profile_table_tx_requested = FALSE;
static boolean                     g_tx_state_requested = FALSE;
static boolean                     g_tx_temp_requested  = FALSE;


/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
void App_Scheduler_Init(void)
{
    Shared_Can_Frame_t tx_frame;

    initMcmcan();
    Base_Driver_Stm_Init();
    App_Manager_Temp_Init();
    App_Manager_System_Init();

    g_app_scheduler_last_tx_state_ms = 0u;
    g_app_scheduler_last_tx_temp_ms  = 0u;

    g_app_scheduler_profile_table_tx_requested = FALSE;

    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

    g_app_scheduler_system_output.current_state        = (uint8)SHARED_SYSTEM_STATE_SLEEP;
    g_app_scheduler_system_output.temperature          = 25;
    g_app_scheduler_system_output.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

    App_Manager_System_GetProfileTable(&g_app_scheduler_system_output.profile_table);

    /* 초기 프로필 테이블 1회 송신 */
    if (App_Can_Service_BuildProfileTableFrame(&g_app_scheduler_system_output.profile_table,
                                             &tx_frame) == TRUE)
    {
      if (App_Can_Service_WriteFrame(&tx_frame) == TRUE)
      {
          UART_Printf("[INIT][TX] SS_PROFILE_TABLE sent\r\n");
      }
    }
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
    App_Scheduler_Task_Temp();
    /* watchdog / debounce 등 */
}

static void App_Scheduler_Run_10ms(void)
{
    g_app_scheduler_now_ms = Shared_Util_Time_GetNowMs();

    App_Scheduler_Task_CanRx();
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

    (void)request_result;
}

static void App_Scheduler_Run_10s(void)
{
    /* statistics / maintenance */
}

/*********************************************************************************************************************/
/*------------------------------------------------CAN Task Functions-------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Task_CanRx(void)
{
    Shared_Can_Frame_t rx_frame;

    App_Can_Service_ResetSystemInput(&g_app_scheduler_system_input);

    while (App_Can_Service_ReadFrame(&rx_frame) == TRUE)
    {
        App_Can_Service_HandleRxFrame(&rx_frame,
                                      &g_app_scheduler_system_input);
    }
}

static void App_Scheduler_Task_CanTx(void)
{
    Shared_Can_Frame_t tx_frame;

    if (g_app_scheduler_profile_table_tx_requested == TRUE)
    {
        if (App_Can_Service_BuildProfileTableFrame(&g_app_scheduler_system_output.profile_table,
                                                   &tx_frame) == TRUE)
        {
            (void)App_Can_Service_WriteFrame(&tx_frame);
            UART_Printf("[TX] SS_PROFILE_TABLE sent\r\n");
        }

        g_app_scheduler_profile_table_tx_requested = FALSE;
    }

    if (g_tx_state_requested == TRUE)
    {
        if (App_Can_Service_BuildStateFrame(
                (Shared_System_State_t)g_app_scheduler_system_output.current_state,
                &tx_frame) == TRUE)
        {
            (void)App_Can_Service_WriteFrame(&tx_frame);
            UART_Printf("[TX] SS_STATE sent\r\n");
        }

        g_tx_state_requested = FALSE;
    }

    if (g_tx_temp_requested == TRUE)
    {
        if (App_Can_Service_BuildTempFrame(g_app_scheduler_system_output.temperature,
                                           &tx_frame) == TRUE)
        {
            (void)App_Can_Service_WriteFrame(&tx_frame);
            UART_Printf("[TX] SS_TEMP sent\r\n");
        }

        g_tx_temp_requested = FALSE;
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------TEMP Task Functions------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Task_Temp(void)
{
    sint16 temperature_x10;

    App_Manager_Temp_Run();

    if (App_Manager_Temp_GetLatestTemp_X10(&temperature_x10) == TRUE)
    {
        if (g_app_scheduler_local_temperature_x10 != temperature_x10)
        {
            g_app_scheduler_local_temperature_x10 = temperature_x10;
            g_tx_temp_requested = TRUE;
        }
    }
}

/*********************************************************************************************************************/
/*-----------------------------------------------SYSTEM Task Functions-----------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Task_System(void)
{
    uint8 prev_state;

    prev_state = g_app_scheduler_system_output.current_state;

    /* CAN으로 유효한 프로필 인덱스가 들어온 경우 로그 */
    if (g_app_scheduler_system_input.auth_event_valid == TRUE)
    {
        UART_Printf("[RX] AB_PROFILE_IDX valid idx=%u\r\n",
                    g_app_scheduler_system_input.active_profile_index);

        /* 테스트 시나리오:
         * ACTIVATED 상태에서 다시 유효 태그가 들어오면 shutdown 요청
         */
        if ((g_app_scheduler_system_output.current_state == (uint8)SHARED_SYSTEM_STATE_ACTIVATED) &&
            (g_app_scheduler_system_input.active_profile_index < SHARED_PROFILE_NORMAL_COUNT))
        {
            g_app_scheduler_system_input.shutdown_request = TRUE;
            UART_Printf("[REQ] shutdown_request = TRUE\r\n");
        }
    }

    App_Manager_System_Run(g_app_scheduler_now_ms,
                           g_app_scheduler_local_temperature_x10,
                           &g_app_scheduler_system_input,
                           &g_app_scheduler_system_output);

    App_Manager_System_GetProfileTable(&g_app_scheduler_system_output.profile_table);

    if (prev_state != g_app_scheduler_system_output.current_state)
    {
        UART_Printf("[STATE] %s -> %s\r\n",
                    App_Test_StateName(prev_state),
                    App_Test_StateName(g_app_scheduler_system_output.current_state));

        g_tx_state_requested = TRUE;
    }

    if ((prev_state != g_app_scheduler_system_output.current_state) &&
        (g_app_scheduler_system_output.current_state == (uint8)SHARED_SYSTEM_STATE_SETUP))
    {
        g_app_scheduler_profile_table_tx_requested = TRUE;
    }
}


/*********************************************************************************************************************/
/*-----------------------------------------------Helper Functions-----------------------------------------------*/
/*********************************************************************************************************************/
static const char *App_Test_StateName(uint8 state)
{
    switch (state)
    {
    case SHARED_SYSTEM_STATE_SLEEP:     return "SLEEP";
    case SHARED_SYSTEM_STATE_SETUP:     return "SETUP";
    case SHARED_SYSTEM_STATE_ACTIVATED: return "ACTIVATED";
    case SHARED_SYSTEM_STATE_SHUTDOWN:  return "SHUTDOWN";
    case SHARED_SYSTEM_STATE_DENIED:    return "DENIED";
    case SHARED_SYSTEM_STATE_EMERGENCY: return "EMERGENCY";
    default:                            return "UNKNOWN";
    }
}