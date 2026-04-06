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
#include "Shared_Profile.h"
#include "Shared_System_State.h"
#include "Shared_Util_Time.h"
#include "Shared_Can_Message.h"
#include "MCMCAN_FD.h"
#include "UART_Config.h"

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

static void    App_Scheduler_HandleRxFrame(const Shared_Can_Frame_t *rx_frame);
static void    App_Scheduler_HandleAccessIdx(uint8 uididx);

static void    App_Scheduler_BuildStateFrame(Shared_Can_Frame_t *tx_frame);
static void    App_Scheduler_BuildTempFrame(Shared_Can_Frame_t *tx_frame);
static void    App_Scheduler_BuildProfileTableFrame(Shared_Can_Frame_t *tx_frame);

static boolean App_Scheduler_IsValidUidIndex(uint8 uididx);

static boolean App_Scheduler_Can_ReadFrame(Shared_Can_Frame_t *rx_frame);
static boolean App_Scheduler_Can_WriteFrame(const Shared_Can_Frame_t *tx_frame);

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/
static uint32                      g_app_scheduler_now_ms = 0u;
static sint16                      g_app_scheduler_local_temperature_x10 = 250; /* default 25.0C */
static App_Manager_System_Input_t  g_app_scheduler_system_input;
static App_Manager_System_Output_t g_app_scheduler_system_output;

static uint32                      g_app_scheduler_last_tx_state_ms         = 0u;
static uint32                      g_app_scheduler_last_tx_temp_ms          = 0u;
static uint32                      g_app_scheduler_last_tx_profile_table_ms = 0u;

/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
void App_Scheduler_Init(void)
{
    Base_Driver_Stm_Init();

    App_Manager_System_Init();
    App_Manager_Temp_Init();

    initMcmcan();

    g_app_scheduler_last_tx_state_ms         = 0u;
    g_app_scheduler_last_tx_temp_ms          = 0u;
    g_app_scheduler_last_tx_profile_table_ms = 0u;

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
    /* watchdog / debounce 등 */
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

    (void)request_result;
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
    Shared_Can_Frame_t rx_frame;

    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

    while (App_Scheduler_Can_ReadFrame(&rx_frame) == TRUE)
    {
        App_Scheduler_HandleRxFrame(&rx_frame);
    }
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

    /* profile table은 별도 getter로 동기화 */
    App_Manager_System_GetProfileTable(&g_app_scheduler_system_output.profile_table);
}

static void App_Scheduler_Task_CanTx(void)
{
    Shared_Can_Frame_t tx_frame;

    if ((g_app_scheduler_now_ms - g_app_scheduler_last_tx_state_ms) >=
        SHARED_CAN_CYCLE_MS_SS_STATE)
    {
        App_Scheduler_BuildStateFrame(&tx_frame);
        (void)App_Scheduler_Can_WriteFrame(&tx_frame);
        g_app_scheduler_last_tx_state_ms = g_app_scheduler_now_ms;
    }

    if ((g_app_scheduler_now_ms - g_app_scheduler_last_tx_temp_ms) >=
        SHARED_CAN_CYCLE_MS_SS_TEMP)
    {
        App_Scheduler_BuildTempFrame(&tx_frame);
        (void)App_Scheduler_Can_WriteFrame(&tx_frame);
        g_app_scheduler_last_tx_temp_ms = g_app_scheduler_now_ms;
    }

    if ((g_app_scheduler_now_ms - g_app_scheduler_last_tx_profile_table_ms) >=
        SHARED_CAN_CYCLE_MS_PROFILE_TABLE)
    {
        App_Scheduler_BuildProfileTableFrame(&tx_frame);
        (void)App_Scheduler_Can_WriteFrame(&tx_frame);
        g_app_scheduler_last_tx_profile_table_ms = g_app_scheduler_now_ms;
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------RX Helpers---------------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_HandleRxFrame(const Shared_Can_Frame_t *rx_frame)
{
    if (rx_frame == NULL_PTR)
    {
        return;
    }

    switch (rx_frame->message_id)
    {
        case SHARED_CAN_MSG_ID_AB_ACCESS_IDX:
        case SHARED_CAN_MSG_ID_HH_ACCESS_IDX:
        {
            if (rx_frame->payload_size >= SHARED_CAN_MSG_SIZE_ACCESS_IDX)
            {
                App_Scheduler_HandleAccessIdx(rx_frame->payload[0]);
            }
            break;
        }

        case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
        case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        {
            if (rx_frame->payload_size >= sizeof(Shared_Profile_Table_t))
            {
                Shared_Profile_Table_t profile_table;

                (void)memcpy(&profile_table,
                             rx_frame->payload,
                             sizeof(Shared_Profile_Table_t));

                App_Manager_System_UpdateProfileTable(&profile_table);
            }
            break;
        }

        default:
        {
            /* ignore */
            break;
        }
    }
}

static void App_Scheduler_HandleAccessIdx(uint8 uididx)
{
    Shared_System_State_t current_state;

    if (App_Scheduler_IsValidUidIndex(uididx) == FALSE)
    {
        return;
    }

    current_state = App_Manager_System_GetState();

    switch (current_state)
    {
        case SHARED_SYSTEM_STATE_SLEEP:
        {
            g_app_scheduler_system_input.auth_event_valid     = TRUE;
            g_app_scheduler_system_input.active_profile_index = uididx;
            break;
        }

        case SHARED_SYSTEM_STATE_ACTIVATED:
        {
            g_app_scheduler_system_input.shutdown_request = TRUE;
            break;
        }

        default:
        {
            /* SETUP / SHUTDOWN / DENIED / EMERGENCY에서는 무시 */
            break;
        }
    }
}

static boolean App_Scheduler_IsValidUidIndex(uint8 uididx)
{
    if (uididx == SHARED_PROFILE_INDEX_INVALID)
    {
        return FALSE;
    }

    if (uididx >= SHARED_PROFILE_TOTAL_COUNT)
    {
        return FALSE;
    }

    return TRUE;
}

/*********************************************************************************************************************/
/*------------------------------------------------TX Builders--------------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_BuildStateFrame(Shared_Can_Frame_t *tx_frame)
{
    Shared_Can_State_t state_msg;

    if (tx_frame == NULL_PTR)
    {
        return;
    }

    (void)memset(tx_frame, 0, sizeof(Shared_Can_Frame_t));

    state_msg.current_state = g_app_scheduler_system_output.current_state;

    tx_frame->message_id   = SHARED_CAN_MSG_ID_SS_STATE;
    tx_frame->dlc          = Shared_Can_GetDlc(tx_frame->message_id);
    tx_frame->frame_size   = Shared_Can_GetFrameSize(tx_frame->message_id);
    tx_frame->payload_size = Shared_Can_GetPayloadSize(tx_frame->message_id);

    (void)memcpy(tx_frame->payload,
                 &state_msg,
                 sizeof(Shared_Can_State_t));
}

static void App_Scheduler_BuildTempFrame(Shared_Can_Frame_t *tx_frame)
{
    Shared_Can_Temp_t temp_msg;

    if (tx_frame == NULL_PTR)
    {
        return;
    }

    (void)memset(tx_frame, 0, sizeof(Shared_Can_Frame_t));

    temp_msg.temperature = g_app_scheduler_system_output.temperature;

    tx_frame->message_id   = SHARED_CAN_MSG_ID_SS_TEMP;
    tx_frame->dlc          = Shared_Can_GetDlc(tx_frame->message_id);
    tx_frame->frame_size   = Shared_Can_GetFrameSize(tx_frame->message_id);
    tx_frame->payload_size = Shared_Can_GetPayloadSize(tx_frame->message_id);

    (void)memcpy(tx_frame->payload,
                 &temp_msg,
                 sizeof(Shared_Can_Temp_t));
}

static void App_Scheduler_BuildProfileTableFrame(Shared_Can_Frame_t *tx_frame)
{
    if (tx_frame == NULL_PTR)
    {
        return;
    }

    (void)memset(tx_frame, 0, sizeof(Shared_Can_Frame_t));

    tx_frame->message_id   = SHARED_CAN_MSG_ID_SS_PROFILE_TABLE;
    tx_frame->dlc          = Shared_Can_GetDlc(tx_frame->message_id);
    tx_frame->frame_size   = Shared_Can_GetFrameSize(tx_frame->message_id);
    tx_frame->payload_size = Shared_Can_GetPayloadSize(tx_frame->message_id);

    (void)memcpy(tx_frame->payload,
                 &g_app_scheduler_system_output.profile_table,
                 sizeof(Shared_Profile_Table_t));
}

/*********************************************************************************************************************/
/*------------------------------------------------CAN Adapter--------------------------------------------------------*/
/*********************************************************************************************************************/
static boolean App_Scheduler_Can_ReadFrame(Shared_Can_Frame_t *rx_frame)
{
    uint32 rx_data[SHARED_CAN_MAX_DATA_WORD_SIZE];
    uint8  copy_size;

    if (rx_frame == NULL_PTR)
    {
        return FALSE;
    }

    (void)memset(rx_frame, 0, sizeof(Shared_Can_Frame_t));
    (void)memset(rx_data, 0, sizeof(rx_data));

    if (receiveCanMessage(rx_data) == FALSE)
    {
        return FALSE;
    }

    rx_frame->message_id   = g_mcmcan.rxMsg.messageId;
    rx_frame->dlc          = (uint8)g_mcmcan.rxMsg.dataLengthCode;
    rx_frame->frame_size   = Shared_Can_GetFrameSize(rx_frame->message_id);
    rx_frame->payload_size = Shared_Can_GetPayloadSize(rx_frame->message_id);

    if ((rx_frame->frame_size == 0U) || (rx_frame->payload_size == 0U))
    {
        return FALSE;
    }

    copy_size = rx_frame->frame_size;
    if (copy_size > (uint8)sizeof(rx_frame->payload))
    {
        copy_size = (uint8)sizeof(rx_frame->payload);
    }

    (void)memcpy(rx_frame->payload,
                 (const uint8 *)rx_data,
                 copy_size);

    return TRUE;
}


static boolean App_Scheduler_Can_WriteFrame(const Shared_Can_Frame_t *tx_frame)
{
    uint32 tx_data[SHARED_CAN_MAX_DATA_WORD_SIZE];
    uint8  copy_size;

    if (tx_frame == NULL_PTR)
    {
        return FALSE;
    }

    (void)memset(tx_data, 0, sizeof(tx_data));

    copy_size = tx_frame->frame_size;
    if (copy_size > (uint8)sizeof(tx_data))
    {
        copy_size = (uint8)sizeof(tx_data);
    }

    (void)memcpy((uint8 *)tx_data,
                 tx_frame->payload,
                 copy_size);

    transmitCanMessage(tx_frame->message_id, tx_data);

    return TRUE;
}
