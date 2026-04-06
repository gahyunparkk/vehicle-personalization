/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Can_Service.h"

#include "MCMCAN_FD.h"
#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------Static Functions---------------------------------------------------*/
/*********************************************************************************************************************/
static boolean App_Can_Service_IsValidProfileIndex(uint8 profile_idx)
{
    if (profile_idx == SHARED_PROFILE_INDEX_INVALID)
    {
        return FALSE;
    }

    if (profile_idx >= SHARED_PROFILE_TOTAL_COUNT)
    {
        return FALSE;
    }

    return TRUE;
}

static boolean App_Can_Service_InitFrame(uint32 message_id,
                                         Shared_Can_Frame_t *tx_frame)
{
    if (tx_frame == NULL_PTR)
    {
        return FALSE;
    }

    (void)memset(tx_frame, 0, sizeof(Shared_Can_Frame_t));

    tx_frame->message_id   = message_id;
    tx_frame->dlc          = Shared_Can_GetDlc(message_id);
    tx_frame->frame_size   = Shared_Can_GetFrameSize(message_id);
    tx_frame->payload_size = Shared_Can_GetPayloadSize(message_id);

    if ((tx_frame->frame_size == 0U) || (tx_frame->payload_size == 0U))
    {
        return FALSE;
    }

    return TRUE;
}

static void App_Can_Service_HandleProfileIdx(uint8                       profile_idx,
                                             App_Manager_System_Input_t *system_input)
{
    Shared_System_State_t current_state;

    if (system_input == NULL_PTR)
    {
        return;
    }

    if (App_Can_Service_IsValidProfileIndex(profile_idx) == FALSE)
    {
        return;
    }

    current_state = App_Manager_System_GetState();

    switch (current_state)
    {
        case SHARED_SYSTEM_STATE_SLEEP:
        {
            system_input->auth_event_valid     = TRUE;
            system_input->active_profile_index = profile_idx;
            break;
        }

        case SHARED_SYSTEM_STATE_ACTIVATED:
        {
            system_input->shutdown_request = TRUE;
            break;
        }

        default:
        {
            /* SETUP / SHUTDOWN / DENIED / EMERGENCY에서는 무시 */
            break;
        }
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------Global Functions---------------------------------------------------*/
/*********************************************************************************************************************/
void App_Can_Service_ResetSystemInput(App_Manager_System_Input_t *system_input)
{
    if (system_input == NULL_PTR)
    {
        return;
    }

    system_input->auth_event_valid     = FALSE;
    system_input->shutdown_request     = FALSE;
    system_input->active_profile_index = SHARED_PROFILE_INDEX_INVALID;
}

boolean App_Can_Service_ReadFrame(Shared_Can_Frame_t *rx_frame)
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

boolean App_Can_Service_WriteFrame(const Shared_Can_Frame_t *tx_frame)
{
    uint32 tx_data[SHARED_CAN_MAX_DATA_WORD_SIZE];
    uint8  copy_size;

    if (tx_frame == NULL_PTR)
    {
        return FALSE;
    }

    if ((tx_frame->frame_size == 0U) || (tx_frame->payload_size == 0U))
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

void App_Can_Service_HandleRxFrame(const Shared_Can_Frame_t   *rx_frame,
                                   App_Manager_System_Input_t *system_input)
{
    if ((rx_frame == NULL_PTR) || (system_input == NULL_PTR))
    {
        return;
    }

    switch (rx_frame->message_id)
    {
        case SHARED_CAN_MSG_ID_AB_PROFILE_IDX:
        case SHARED_CAN_MSG_ID_HH_PROFILE_IDX:
        {
            if (rx_frame->payload_size >= SHARED_CAN_MSG_SIZE_PROFILE_IDX)
            {
                App_Can_Service_HandleProfileIdx(rx_frame->payload[0], system_input);
            }
            break;
        }

        case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
        case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        {
            if (rx_frame->payload_size >= sizeof(Shared_Profile_Table_t))
            {
                Shared_Profile_Table_t profile_table;

                (void)memset(&profile_table, 0, sizeof(Shared_Profile_Table_t));
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

boolean App_Can_Service_BuildStateFrame(Shared_System_State_t current_state,
                                        Shared_Can_Frame_t    *tx_frame)
{
    Shared_Can_State_t state_msg;

    if (App_Can_Service_InitFrame(SHARED_CAN_MSG_ID_SS_STATE, tx_frame) == FALSE)
    {
        return FALSE;
    }

    (void)memset(&state_msg, 0, sizeof(Shared_Can_State_t));
    state_msg.current_state = (uint8)current_state;

    (void)memcpy(tx_frame->payload,
                 &state_msg,
                 sizeof(Shared_Can_State_t));

    return TRUE;
}

boolean App_Can_Service_BuildTempFrame(sint8               temperature,
                                       Shared_Can_Frame_t *tx_frame)
{
    Shared_Can_Temp_t temp_msg;

    if (App_Can_Service_InitFrame(SHARED_CAN_MSG_ID_SS_TEMP, tx_frame) == FALSE)
    {
        return FALSE;
    }

    (void)memset(&temp_msg, 0, sizeof(Shared_Can_Temp_t));
    temp_msg.temperature = temperature;

    (void)memcpy(tx_frame->payload,
                 &temp_msg,
                 sizeof(Shared_Can_Temp_t));

    return TRUE;
}

boolean App_Can_Service_BuildProfileTableFrame(const Shared_Profile_Table_t *profile_table,
                                               Shared_Can_Frame_t           *tx_frame)
{
    if ((profile_table == NULL_PTR) || (tx_frame == NULL_PTR))
    {
        return FALSE;
    }

    if (App_Can_Service_InitFrame(SHARED_CAN_MSG_ID_SS_PROFILE_TABLE, tx_frame) == FALSE)
    {
        return FALSE;
    }

    (void)memcpy(tx_frame->payload,
                 profile_table,
                 sizeof(Shared_Profile_Table_t));

    return TRUE;
}