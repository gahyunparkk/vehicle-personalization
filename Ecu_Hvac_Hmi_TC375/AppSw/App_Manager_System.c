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
#include "Ifx_Types.h"
#include "MCMCAN_FD.h"
#include "Platform_Types.h"
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
  Shared_System_State_t current_state;
  uint32 state_enter_time_ms;
  uint8 active_profile_index;
  boolean setup_profile_loaded;
  Shared_Profile_Table_t profile_table;
} App_Manager_System_Context_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* public-like local helpers */
static void App_Manager_System_SetState(Shared_System_State_t next_state, uint32 now_ms);

/* state / condition check */
static boolean App_Manager_System_IsNormalProfileIndex(uint8 profile_index);
static uint32 App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms);

/* profile table */
static void App_Manager_System_CopyProfileTable(Shared_Profile_Table_t *dst,
                                                const Shared_Profile_Table_t *src);
static void App_Manager_System_ReplaceProfileTable(Shared_Profile_Table_t *dst,
                                                   const Shared_Profile_Table_t *src);

// init sequence
static boolean App_PollProfileTableAtInit(uint32 timeout_ms);

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

  App_Manager_System_ReplaceProfileTable(&g_app_manager_system_context.profile_table,
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

void App_Manager_System_SetActiveProfileIndex(uint8 idx)
{
  g_app_manager_system_context.active_profile_index = idx;
}

void App_Manager_System_GetActiveProfileIndex(uint8 *idx)
{
  *idx = g_app_manager_system_context.active_profile_index;
}

void App_Manager_System_Init(void)
{
  g_app_manager_system_context.current_state = SHARED_SYSTEM_STATE_SLEEP;
  g_app_manager_system_context.state_enter_time_ms = 0U;
  g_app_manager_system_context.active_profile_index = SHARED_PROFILE_INDEX_INVALID;
  g_app_manager_system_context.setup_profile_loaded = FALSE;
  App_PollProfileTableAtInit(20000);
}

Shared_System_State_t App_Manager_System_GetState(void)
{
  return g_app_manager_system_context.current_state;
}

void App_Manager_System_GetProfile(uint8 idx, Shared_Profile_t *profile)
{
  *profile = g_app_manager_system_context.profile_table.profile[idx];
}
void App_Manager_System_SetProfile(uint8 idx, Shared_Profile_t *profile)
{
  g_app_manager_system_context.profile_table.profile[idx] = *profile;
}

/*********************************************************************************************************************/
/*---------------------------------------------State Helper Functions------------------------------------------------*/
/*********************************************************************************************************************/
// static void App_Manager_System_SetState(Shared_System_State_t next_state, uint32 now_ms)
//{
//   if (g_app_manager_system_context.current_state != next_state)
//   {
//     g_app_manager_system_context.current_state = next_state;
//     g_app_manager_system_context.state_enter_time_ms = now_ms;
//   }
// }
//
// static boolean App_Manager_System_IsNormalProfileIndex(uint8 profile_index)
//{
//   return (profile_index < SHARED_PROFILE_NORMAL_COUNT) ? TRUE : FALSE;
// }
//
// static uint32 App_Manager_System_GetElapsed(uint32 now_ms, uint32 base_ms)
//{
//   return (now_ms - base_ms);
// }

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

static void App_Manager_System_ReplaceProfileTable(Shared_Profile_Table_t *dst,
                                                   const Shared_Profile_Table_t *src)
{
  uint8 index;

  if ((dst == NULL_PTR) || (src == NULL_PTR))
  {
    return;
  }

  for (index = 0U; index < SHARED_PROFILE_TOTAL_COUNT; index++)
  {
    dst->profile[index].seat_motor_angle = src->profile[index].seat_motor_angle;
    dst->profile[index].side_motor_angle = src->profile[index].side_motor_angle;
  }
}

/* -------------------------------------------------------------------------------------------------
 * INIT CAN POLLING
 * - 초기화 시점에 profile table을 일정 시간 동안 폴링으로 수신
 * - 수신 성공 시 g_app.profileTable 갱신 후 TRUE 반환
 * ------------------------------------------------------------------------------------------------- */
static boolean App_PollProfileTableAtInit(uint32 timeout_ms)
{
  uint32 rx_data[MAXIMUM_CAN_DATA_PAYLOAD];
  uint32 rx_id;
  const uint8 *rx_bytes;
//  uint32 start_ms;

  // start_ms = App_GetNowMs();

  // while ((App_GetNowMs() - start_ms) < timeout_ms)
  while (1)
  {
    if (receiveCanMessage(rx_data) == FALSE)
    {
      continue;
    }

    rx_id = g_mcmcan.rxMsg.messageId;
    rx_bytes = (const uint8 *)rx_data;

    switch (rx_id)
    {
    case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
    {
      Shared_Profile_Table_t profile_table_msg;

      (void)memset(&profile_table_msg, 0, sizeof(profile_table_msg));
      (void)memcpy(&profile_table_msg,
                   rx_bytes,
                   sizeof(profile_table_msg));

      (void)memcpy(&g_app_manager_system_context.profile_table,
                   &profile_table_msg,
                   sizeof(Shared_Profile_Table_t));

      if (rx_id == SHARED_CAN_MSG_ID_SS_PROFILE_TABLE)
      {
        UART_Printf("[INIT][RX] SS_PROFILE_TABLE received\r\n");
      }
      else
      {
        UART_Printf("[INIT][RX] HH_PROFILE_TABLE received\r\n");
      }

      return TRUE;
    }

    case SHARED_CAN_MSG_ID_SS_STATE:
    {
      Shared_Can_State_t state_msg;

      (void)memset(&state_msg, 0, sizeof(state_msg));
      (void)memcpy(&state_msg,
                   rx_bytes,
                   sizeof(state_msg));

      g_app_manager_system_context.current_state = state_msg.current_state;

      UART_Printf("[INIT][RX] SS_STATE current_state=%u\r\n",
                  state_msg.current_state);
      break;
    }

    default:
    {
      /* init 단계에서는 profile table 외 메시지는 무시 */
      break;
    }
    }
  }

  UART_Printf("[INIT][RX] PROFILE_TABLE timeout -> use default\r\n");
  return FALSE;
}
