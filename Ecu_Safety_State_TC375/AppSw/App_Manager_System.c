#include "App_Manager_System.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* 필요하면 여기 매크로 추가 */

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include <string.h>

#include "App_Can_Service.h"
#include "Platform_Types.h"
#include "Ifx_Types.h"
#include "Base_Fee.h"
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    Shared_System_State_t  current_state;
    uint32                 state_enter_time_ms;
    uint8                  active_profile_index;
    sint16                 last_temperature_x10;
    boolean                setup_profile_loaded;
    Shared_Profile_Table_t profile_table;
} App_Manager_System_Context_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* public-like local helpers */
static void    App_Manager_System_ResetOutput(App_Manager_System_Output_t *output);
static void    App_Manager_System_SetState(Shared_System_State_t next_state, uint32 now_ms);

/* state / condition check */
static boolean App_Manager_System_IsNormalProfileIndex(uint8 profile_index);
static boolean App_Manager_System_IsEmergencyRequired(sint16 temperature_x10);
static uint32  App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms);
static sint8   App_Manager_System_ConvertTemperatureToTxValue(sint16 temperature_x10);

/* profile table */
static void    App_Manager_System_CopyProfileTable(Shared_Profile_Table_t *dst,
                                                   const Shared_Profile_Table_t *src);
static void    App_Manager_System_ClearProfileTable(Shared_Profile_Table_t *table);
static void    App_Manager_System_LoadProfileTableFromDFlash(Shared_Profile_Table_t *profile_table);
static void    App_Manager_System_SaveProfileTableToDFlash(const Shared_Profile_Table_t *profile_table);
static void    App_Manager_System_LoadDummyProfileTable(Shared_Profile_Table_t *profile_table);

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/
static App_Manager_System_Context_t g_app_manager_system_context;

/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_System_UpdateProfileTable(const Shared_Profile_Table_t *profile_table)
{
    if (profile_table == NULL_PTR)
    {
        return;
    }

    App_Manager_System_CopyProfileTable(&g_app_manager_system_context.profile_table,
                                        profile_table);
}

void App_Manager_System_GetProfileTable(Shared_Profile_Table_t *profile_table)
{
    if (profile_table == NULL_PTR)
    {
        return;
    }

    App_Manager_System_CopyProfileTable(profile_table,
                                        &g_app_manager_system_context.profile_table);
}

void App_Manager_System_Init(void)
{
    g_app_manager_system_context.current_state         = SHARED_SYSTEM_STATE_SLEEP;
    g_app_manager_system_context.state_enter_time_ms   = 0U;
    g_app_manager_system_context.active_profile_index  = SHARED_PROFILE_INDEX_INVALID;
    g_app_manager_system_context.last_temperature_x10  = 0;
    g_app_manager_system_context.setup_profile_loaded  = FALSE;

    App_Manager_System_ClearProfileTable(&g_app_manager_system_context.profile_table);

    if (g_app_manager_system_context.setup_profile_loaded == FALSE)
    {
        App_Manager_System_LoadProfileTableFromDFlash(
            &g_app_manager_system_context.profile_table);

        g_app_manager_system_context.setup_profile_loaded = TRUE;
    }
}

void App_Manager_System_Run(uint32 now_ms,
                            sint16 local_temperature_x10,
                            const App_Manager_System_Input_t *input,
                            App_Manager_System_Output_t *output)
{
    if ((input == NULL_PTR) || (output == NULL_PTR))
    {
        return;
    }

    g_app_manager_system_context.last_temperature_x10 = local_temperature_x10;

    switch (g_app_manager_system_context.current_state)
    {
    case SHARED_SYSTEM_STATE_SLEEP:
        g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

        if (input->auth_event_valid == TRUE)
        {
            if (input->active_profile_index == SHARED_PROFILE_INDEX_INVALID)
            {
                App_Manager_System_SetState(SHARED_SYSTEM_STATE_DENIED, now_ms);
            }
            else if (App_Manager_System_IsNormalProfileIndex(input->active_profile_index) == TRUE)
            {
                g_app_manager_system_context.active_profile_index = input->active_profile_index;
                App_Manager_System_SetState(SHARED_SYSTEM_STATE_SETUP, now_ms);
            }
            else
            {
                /* ignore invalid value */
            }
        }
        break;

    case SHARED_SYSTEM_STATE_SETUP:
        if (App_Manager_System_IsEmergencyRequired(local_temperature_x10) == TRUE)
        {
            g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_EMERGENCY;
            App_Manager_System_SetState(SHARED_SYSTEM_STATE_EMERGENCY, now_ms);
        }
        else if (App_Manager_System_GetElapsed(now_ms,
                                               g_app_manager_system_context.state_enter_time_ms) >=
                 APP_MANAGER_SYSTEM_SETUP_HOLD_MS)
        {
            App_Manager_System_SetState(SHARED_SYSTEM_STATE_ACTIVATED, now_ms);
        }
        break;

    case SHARED_SYSTEM_STATE_ACTIVATED:
        if (input->shutdown_request == TRUE)
        {
            g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;
            App_Manager_System_SetState(SHARED_SYSTEM_STATE_SHUTDOWN, now_ms);
        }
        else if (App_Manager_System_IsEmergencyRequired(local_temperature_x10) == TRUE)
        {
            g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_EMERGENCY;
            App_Manager_System_SetState(SHARED_SYSTEM_STATE_EMERGENCY, now_ms);
        }
        break;

    case SHARED_SYSTEM_STATE_SHUTDOWN:
        g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

        App_Manager_System_SaveProfileTableToDFlash(
            &g_app_manager_system_context.profile_table);

        App_Manager_System_SetState(SHARED_SYSTEM_STATE_SLEEP, now_ms);
        break;

    case SHARED_SYSTEM_STATE_DENIED:
        g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

        if (App_Manager_System_GetElapsed(now_ms,
                                          g_app_manager_system_context.state_enter_time_ms) >=
            APP_MANAGER_SYSTEM_DENIED_TIMEOUT_MS)
        {
            App_Manager_System_SetState(SHARED_SYSTEM_STATE_SLEEP, now_ms);
        }
        break;

    case SHARED_SYSTEM_STATE_EMERGENCY:
        g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_EMERGENCY;
        break;

    default:
        g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;
        App_Manager_System_SetState(SHARED_SYSTEM_STATE_SLEEP, now_ms);
        break;
    }

    App_Manager_System_ResetOutput(output);
}

Shared_System_State_t App_Manager_System_GetState(void)
{
    return g_app_manager_system_context.current_state;
}

uint8 App_Manager_System_GetActiveProfileIndex(void)
{
    return g_app_manager_system_context.active_profile_index;
}

/*********************************************************************************************************************/
/*---------------------------------------------State Helper Functions------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Manager_System_ResetOutput(App_Manager_System_Output_t *output)
{
    output->current_state        = (uint8)g_app_manager_system_context.current_state;
    output->temperature          =
        App_Manager_System_ConvertTemperatureToTxValue(
            g_app_manager_system_context.last_temperature_x10);
    output->active_profile_index = g_app_manager_system_context.active_profile_index;
    output->profile_table        = g_app_manager_system_context.profile_table;
}

static void App_Manager_System_SetState(Shared_System_State_t next_state, uint32 now_ms)
{
    if (g_app_manager_system_context.current_state != next_state)
    {
        g_app_manager_system_context.current_state        = next_state;
        g_app_manager_system_context.state_enter_time_ms = now_ms;
    }
}

static boolean App_Manager_System_IsNormalProfileIndex(uint8 profile_index)
{
    return (profile_index < SHARED_PROFILE_NORMAL_COUNT) ? TRUE : FALSE;
}

static boolean App_Manager_System_IsEmergencyRequired(sint16 temperature_x10)
{
    if (temperature_x10 >= APP_MANAGER_SYSTEM_TEMP_EMERGENCY_HIGH_X10)
    {
        return TRUE;
    }


    return FALSE;
}

static uint32 App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms)
{
    return (now_ms - base_ms);
}

static sint8 App_Manager_System_ConvertTemperatureToTxValue(sint16 temperature_x10)
{
    sint16 temp_c = temperature_x10 / 10;

    if (temp_c > 127)
    {
        temp_c = 127;
    }
    else if (temp_c < -128)
    {
        temp_c = -128;
    }

    return (sint8)temp_c;
}

/*********************************************************************************************************************/
/*-------------------------------------------Profile Table Functions-------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Manager_System_CopyProfileTable(Shared_Profile_Table_t *dst,
                                                const Shared_Profile_Table_t *src)
{
    uint8 index;

    if ((dst == NULL_PTR) || (src == NULL_PTR))
    {
        return;
    }

    for (index = 0U; index < SHARED_PROFILE_TOTAL_COUNT; index++)
    {
        dst->profile[index] = src->profile[index];
    }
}

static void App_Manager_System_ClearProfileTable(Shared_Profile_Table_t *table)
{
    uint8 index;

    if (table == NULL_PTR)
    {
        return;
    }

    for (index = 0U; index < SHARED_PROFILE_TOTAL_COUNT; index++)
    {
        table->profile[index].profile_id          = 0U;
        table->profile[index].side_motor_angle    = 0U;
        table->profile[index].seat_motor_angle    = 0U;
        table->profile[index].ambient_light       = 0U;
        table->profile[index].ac_on_threshold     = 0;
        table->profile[index].heater_on_threshold = 0;
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------DFlash Functions-----------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Manager_System_LoadProfileTableFromDFlash(Shared_Profile_Table_t *profile_table)
{
    Fee_Status_t load_status;
    Fee_Status_t save_status;

    if (profile_table == NULL_PTR)
    {
        return;
    }

    load_status = Flash_LoadProfileTable(profile_table);

    if (load_status == FEE_STATUS_OK)
    {
        UART_Printf("[SYSTEM] profile table loaded from DFlash\r\n");
        return;
    }

    /*
     * DFlash가 비어 있거나, 기존 데이터가 손상/무효인 경우
     * fallback default profile을 RAM에 올린 뒤 1회 저장 시도
     */
    App_Manager_System_LoadDummyProfileTable(profile_table);

    UART_Printf("[SYSTEM] DFlash load failed (%d), default profile loaded\r\n",
                (sint32)load_status);

    if ((load_status == FEE_STATUS_EMPTY) ||
        (load_status == FEE_STATUS_NOT_FOUND) ||
        (load_status == FEE_STATUS_HEADER_ERROR) ||
        (load_status == FEE_STATUS_CHECKSUM_ERROR))
    {
        save_status = Flash_SaveProfileTable(profile_table);

        if (save_status == FEE_STATUS_OK)
        {
            UART_Printf("[SYSTEM] default profile saved to DFlash\r\n");
        }
        else
        {
            UART_Printf("[SYSTEM] default profile save failed (%d)\r\n",
                        (sint32)save_status);
        }
    }
}

static void App_Manager_System_SaveProfileTableToDFlash(const Shared_Profile_Table_t *profile_table)
{
    Fee_Status_t save_status;

    if (profile_table == NULL_PTR)
    {
        return;
    }

    save_status = Flash_SaveProfileTable(profile_table);

    if (save_status == FEE_STATUS_OK)
    {
        UART_Printf("[SYSTEM] profile table saved to DFlash\r\n");
    }
    else
    {
        UART_Printf("[SYSTEM] profile table save failed (%d)\r\n",
                    (sint32)save_status);
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------Dummy Functions------------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Manager_System_LoadDummyProfileTable(Shared_Profile_Table_t *profile_table)
{
    if (profile_table == NULL_PTR)
    {
        return;
    }

    (void)memset(profile_table, 0, sizeof(Shared_Profile_Table_t));

    profile_table->profile[SHARED_PROFILE_INDEX_0].profile_id          = 0x86b8;
    profile_table->profile[SHARED_PROFILE_INDEX_0].side_motor_angle    = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_0].seat_motor_angle    = 128U;
    profile_table->profile[SHARED_PROFILE_INDEX_0].ambient_light       = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_0].ac_on_threshold     = 30;
    profile_table->profile[SHARED_PROFILE_INDEX_0].heater_on_threshold = 15;

    profile_table->profile[SHARED_PROFILE_INDEX_1].profile_id          = 0x839c;
    profile_table->profile[SHARED_PROFILE_INDEX_1].side_motor_angle    = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_1].seat_motor_angle    = 128U;
    profile_table->profile[SHARED_PROFILE_INDEX_1].ambient_light       = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_1].ac_on_threshold     = 30;
    profile_table->profile[SHARED_PROFILE_INDEX_1].heater_on_threshold = 15;

    profile_table->profile[SHARED_PROFILE_INDEX_2].profile_id          = 0x1234;
    profile_table->profile[SHARED_PROFILE_INDEX_2].side_motor_angle    = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_2].seat_motor_angle    = 128U;
    profile_table->profile[SHARED_PROFILE_INDEX_2].ambient_light       = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_2].ac_on_threshold     = 30;
    profile_table->profile[SHARED_PROFILE_INDEX_2].heater_on_threshold = 15;

    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].profile_id          = 0xeeee;
    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].side_motor_angle    = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].seat_motor_angle    = 128U;
    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].ambient_light       = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].ac_on_threshold     = 30;
    profile_table->profile[SHARED_PROFILE_INDEX_DEFAULT].heater_on_threshold = 15;

    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].profile_id          = 0xffff;
    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].side_motor_angle    = 0xffU;
    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].seat_motor_angle    = 0xffU;
    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].ambient_light       = 0U;
    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].ac_on_threshold     = 30;
    profile_table->profile[SHARED_PROFILE_INDEX_EMERGENCY].heater_on_threshold = 15;

    UART_Printf("[SYSTEM] default profile table loaded\r\n");
}
