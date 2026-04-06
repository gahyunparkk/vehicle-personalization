#ifndef SHARED_CAN_MESSAGE_H_
#define SHARED_CAN_MESSAGE_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * CAN bit rate
 */
#define SHARED_CAN_NOMINAL_BAUDRATE            (500000U)
#define SHARED_CAN_DATA_BAUDRATE               (2000000U)

/*
 * CAN Message ID
 */
#define SHARED_CAN_MSG_ID_SS_STATE             (0x100U)
#define SHARED_CAN_MSG_ID_AB_ACCESS_IDX        (0x200U)
#define SHARED_CAN_MSG_ID_HH_ACCESS_IDX        (0x201U)
#define SHARED_CAN_MSG_ID_SS_TEMP              (0x300U)
#define SHARED_CAN_MSG_ID_SS_PROFILE_TABLE     (0x400U)
#define SHARED_CAN_MSG_ID_AB_PROFILE_TABLE     (0x401U)
#define SHARED_CAN_MSG_ID_HH_PROFILE_TABLE     (0x402U)

/*
 * CAN FD DLC code
 *
 * 0~8  : 0~8 bytes
 * 9    : 12 bytes
 * 10   : 16 bytes
 * 11   : 20 bytes
 * 12   : 24 bytes
 * 13   : 32 bytes
 * 14   : 48 bytes
 * 15   : 64 bytes
 */
#define SHARED_CAN_DLC_0                       (0U)
#define SHARED_CAN_DLC_1                       (1U)
#define SHARED_CAN_DLC_2                       (2U)
#define SHARED_CAN_DLC_3                       (3U)
#define SHARED_CAN_DLC_4                       (4U)
#define SHARED_CAN_DLC_5                       (5U)
#define SHARED_CAN_DLC_6                       (6U)
#define SHARED_CAN_DLC_7                       (7U)
#define SHARED_CAN_DLC_8                       (8U)
#define SHARED_CAN_DLC_12                      (9U)
#define SHARED_CAN_DLC_16                      (10U)
#define SHARED_CAN_DLC_20                      (11U)
#define SHARED_CAN_DLC_24                      (12U)
#define SHARED_CAN_DLC_32                      (13U)
#define SHARED_CAN_DLC_48                      (14U)
#define SHARED_CAN_DLC_64                      (15U)

/*
 * Message payload size [byte]
 * - payload size: 실제 유효 데이터 크기
 * - frame size  : CAN/CAN FD 프레임 상의 실제 전송 바이트 수
 */
#define SHARED_CAN_MSG_SIZE_SS_STATE           (1U)
#define SHARED_CAN_MSG_SIZE_ACCESS_IDX         (1U)
#define SHARED_CAN_MSG_SIZE_SS_TEMP            (1U)
#define SHARED_CAN_MSG_SIZE_PROFILE_TABLE      (40U)

#define SHARED_CAN_FRAME_SIZE_SS_STATE         (1U)
#define SHARED_CAN_FRAME_SIZE_ACCESS_IDX       (1U)
#define SHARED_CAN_FRAME_SIZE_SS_TEMP          (1U)
#define SHARED_CAN_FRAME_SIZE_PROFILE_TABLE    (48U)

#define SHARED_CAN_MAX_FRAME_SIZE_BYTE         (48U)
#define SHARED_CAN_MAX_DATA_WORD_SIZE          (16U)   /* 64 byte work buffer */

/*
 * Message DLC
 */
#define SHARED_CAN_MSG_DLC_SS_STATE            (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_ACCESS_IDX          (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_SS_TEMP             (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_PROFILE_TABLE       (SHARED_CAN_DLC_48)

/*
 * Message cycle [ms]
 */
#define SHARED_CAN_CYCLE_MS_SS_STATE           (10U)
#define SHARED_CAN_CYCLE_MS_SS_TEMP            (1000U)
#define SHARED_CAN_CYCLE_MS_PROFILE_TABLE      (1000U)
#define SHARED_CAN_CYCLE_MS_EVENT              (0U)

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    uint8 current_state;
} Shared_Can_State_t;

typedef struct
{
    uint8 active_profile_index;
} Shared_Can_Access_Idx_t;

typedef struct
{
    sint8 temperature;
} Shared_Can_Temp_t;

typedef struct
{
    uint32 message_id;
    uint8  dlc;
    uint8  frame_size;
    uint8  payload_size;
    uint8  payload[SHARED_CAN_MAX_FRAME_SIZE_BYTE];
} Shared_Can_Frame_t;

/*********************************************************************************************************************/
/*-----------------------------------------------Inline Helper Functions---------------------------------------------*/
/*********************************************************************************************************************/
static inline uint8 Shared_Can_GetFrameSizeFromDlc(uint8 dlc)
{
    switch (dlc)
    {
    case SHARED_CAN_DLC_0:  return 0U;
    case SHARED_CAN_DLC_1:  return 1U;
    case SHARED_CAN_DLC_2:  return 2U;
    case SHARED_CAN_DLC_3:  return 3U;
    case SHARED_CAN_DLC_4:  return 4U;
    case SHARED_CAN_DLC_5:  return 5U;
    case SHARED_CAN_DLC_6:  return 6U;
    case SHARED_CAN_DLC_7:  return 7U;
    case SHARED_CAN_DLC_8:  return 8U;
    case SHARED_CAN_DLC_12: return 12U;
    case SHARED_CAN_DLC_16: return 16U;
    case SHARED_CAN_DLC_20: return 20U;
    case SHARED_CAN_DLC_24: return 24U;
    case SHARED_CAN_DLC_32: return 32U;
    case SHARED_CAN_DLC_48: return 48U;
    case SHARED_CAN_DLC_64: return 64U;
    default:                return 0U;
    }
}

static inline uint8 Shared_Can_GetPayloadSize(uint32 message_id)
{
    switch (message_id)
    {
    case SHARED_CAN_MSG_ID_SS_STATE:
        return SHARED_CAN_MSG_SIZE_SS_STATE;

    case SHARED_CAN_MSG_ID_AB_ACCESS_IDX:
    case SHARED_CAN_MSG_ID_HH_ACCESS_IDX:
        return SHARED_CAN_MSG_SIZE_ACCESS_IDX;

    case SHARED_CAN_MSG_ID_SS_TEMP:
        return SHARED_CAN_MSG_SIZE_SS_TEMP;

    case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        return SHARED_CAN_MSG_SIZE_PROFILE_TABLE;

    default:
        return 0U;
    }
}

static inline uint8 Shared_Can_GetDlc(uint32 message_id)
{
    switch (message_id)
    {
    case SHARED_CAN_MSG_ID_SS_STATE:
        return SHARED_CAN_MSG_DLC_SS_STATE;

    case SHARED_CAN_MSG_ID_AB_ACCESS_IDX:
    case SHARED_CAN_MSG_ID_HH_ACCESS_IDX:
        return SHARED_CAN_MSG_DLC_ACCESS_IDX;

    case SHARED_CAN_MSG_ID_SS_TEMP:
        return SHARED_CAN_MSG_DLC_SS_TEMP;

    case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        return SHARED_CAN_MSG_DLC_PROFILE_TABLE;

    default:
        return SHARED_CAN_DLC_0;
    }
}

static inline uint8 Shared_Can_GetFrameSize(uint32 message_id)
{
    return Shared_Can_GetFrameSizeFromDlc(Shared_Can_GetDlc(message_id));
}

static inline boolean Shared_Can_IsFdMessage(uint32 message_id)
{
    switch (message_id)
    {
    case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
    case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        return TRUE;

    default:
        return FALSE;
    }
}

static inline boolean Shared_Can_IsValidMessageId(uint32 message_id)
{
    return (Shared_Can_GetPayloadSize(message_id) > 0U) ? TRUE : FALSE;
}

/*********************************************************************************************************************/
/*-----------------------------------------------Compile-Time Checks-------------------------------------------------*/
/*********************************************************************************************************************/
typedef char Shared_Can_State_t_Size_Check[
    (sizeof(Shared_Can_State_t) == SHARED_CAN_MSG_SIZE_SS_STATE) ? 1 : -1];

typedef char Shared_Can_Access_Idx_t_Size_Check[
    (sizeof(Shared_Can_Access_Idx_t) == SHARED_CAN_MSG_SIZE_ACCESS_IDX) ? 1 : -1];

typedef char Shared_Can_Temp_t_Size_Check[
    (sizeof(Shared_Can_Temp_t) == SHARED_CAN_MSG_SIZE_SS_TEMP) ? 1 : -1];

typedef char Shared_Can_Profile_Table_t_Size_Check[
    (sizeof(Shared_Profile_Table_t) == SHARED_CAN_MSG_SIZE_PROFILE_TABLE) ? 1 : -1];

#endif /* SHARED_CAN_MESSAGE_H_ */