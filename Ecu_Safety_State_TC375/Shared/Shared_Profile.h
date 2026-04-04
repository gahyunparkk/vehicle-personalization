#ifndef SHARED_PROFILE_H_
#define SHARED_PROFILE_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define SHARED_PROFILE_NORMAL_COUNT         (3U)
#define SHARED_PROFILE_DEFAULT_COUNT        (1U)
#define SHARED_PROFILE_EMERGENCY_COUNT      (1U)
#define SHARED_PROFILE_TOTAL_COUNT          (5U)

#define SHARED_PROFILE_INDEX_0              (0U)
#define SHARED_PROFILE_INDEX_1              (1U)
#define SHARED_PROFILE_INDEX_2              (2U)
#define SHARED_PROFILE_INDEX_DEFAULT        (3U)
#define SHARED_PROFILE_INDEX_EMERGENCY      (4U)
#define SHARED_PROFILE_INDEX_INVALID        (0xFFU)

#define SHARED_PROFILE_SIZE_BYTE            (8U)
#define SHARED_PROFILE_TABLE_SIZE_BYTE      (40U)

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 1 profile = 8 bytes
 *
 * profile_id          : 2 bytes
 * side_motor_angle    : 1 byte
 * seat_motor_angle    : 1 byte
 * ambient_light       : 2 bytes
 * ac_on_threshold     : 1 byte
 * heater_on_threshold : 1 byte
 */
typedef struct
{
    uint16 profile_id;
    uint8  side_motor_angle;
    uint8  seat_motor_angle;
    uint16 ambient_light;
    sint8  ac_on_threshold;
    sint8  heater_on_threshold;
} Shared_Profile_t;

/*
 * normal profile 3개 + default profile 1개 + emergency profile 1개
 * total = 40 bytes
 */
typedef struct
{
    Shared_Profile_t profile[SHARED_PROFILE_TOTAL_COUNT];
} Shared_Profile_Table_t;

/*********************************************************************************************************************/
/*-----------------------------------------------Compile-Time Checks-------------------------------------------------*/
/*********************************************************************************************************************/
typedef char Shared_Profile_t_Size_Check[
    (sizeof(Shared_Profile_t) == SHARED_PROFILE_SIZE_BYTE) ? 1 : -1];

typedef char Shared_Profile_Table_t_Size_Check[
    (sizeof(Shared_Profile_Table_t) == SHARED_PROFILE_TABLE_SIZE_BYTE) ? 1 : -1];

#endif /* SHARED_PROFILE_H_ */
