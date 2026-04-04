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
 * CAN Message ID
 */
#define SHARED_CAN_MSG_ID_SS_STATE              (0x100U)
#define SHARED_CAN_MSG_ID_AB_ACCESS_IDX         (0x200U)
#define SHARED_CAN_MSG_ID_HH_ACCESS_IDX         (0x201U)
#define SHARED_CAN_MSG_ID_SS_TEMP               (0x300U)
#define SHARED_CAN_MSG_ID_SS_PROFILE_TABLE      (0x400U)
#define SHARED_CAN_MSG_ID_AB_PROFILE_TABLE      (0x401U)
#define SHARED_CAN_MSG_ID_HH_PROFILE_TABLE      (0x402U)

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
#define SHARED_CAN_DLC_0                        (0U)
#define SHARED_CAN_DLC_1                        (1U)
#define SHARED_CAN_DLC_2                        (2U)
#define SHARED_CAN_DLC_3                        (3U)
#define SHARED_CAN_DLC_4                        (4U)
#define SHARED_CAN_DLC_5                        (5U)
#define SHARED_CAN_DLC_6                        (6U)
#define SHARED_CAN_DLC_7                        (7U)
#define SHARED_CAN_DLC_8                        (8U)
#define SHARED_CAN_DLC_12                       (9U)
#define SHARED_CAN_DLC_16                       (10U)
#define SHARED_CAN_DLC_20                       (11U)
#define SHARED_CAN_DLC_24                       (12U)
#define SHARED_CAN_DLC_32                       (13U)
#define SHARED_CAN_DLC_48                       (14U)
#define SHARED_CAN_DLC_64                       (15U)

/*
 * Message payload size [byte]
 */
#define SHARED_CAN_MSG_SIZE_SS_STATE            (1U)
#define SHARED_CAN_MSG_SIZE_ACCESS_IDX          (1U)
#define SHARED_CAN_MSG_SIZE_SS_TEMP             (1U)
#define SHARED_CAN_MSG_SIZE_PROFILE_TABLE       (40U)

/*
 * Message DLC
 */
#define SHARED_CAN_MSG_DLC_SS_STATE             (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_ACCESS_IDX           (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_SS_TEMP              (SHARED_CAN_DLC_1)
#define SHARED_CAN_MSG_DLC_PROFILE_TABLE        (SHARED_CAN_DLC_48)

/*
 * Message cycle [ms]
 */
#define SHARED_CAN_CYCLE_MS_SS_STATE            (10U)
#define SHARED_CAN_CYCLE_MS_SS_TEMP             (1000U)
#define SHARED_CAN_CYCLE_MS_PROFILE_TABLE       (1000U)
#define SHARED_CAN_CYCLE_MS_EVENT               (0U)

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
