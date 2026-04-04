#ifndef SHARED_SYSTEM_STATE_H_
#define SHARED_SYSTEM_STATE_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum
{
    SHARED_SYSTEM_STATE_SLEEP = 0,
    SHARED_SYSTEM_STATE_SETUP,
    SHARED_SYSTEM_STATE_ACTIVATED,
    SHARED_SYSTEM_STATE_DENIED,
    SHARED_SYSTEM_STATE_EMERGENCY
} Shared_System_State_t;

/*
 * 모든 ECU가 알아야 하는 최소 상태 정보만 포함
 *
 * total = 8 bytes
 */
typedef struct
{
    uint8 current_state;
    uint8 previous_state;
    uint8 active_profile_index;
    uint8 reserved0;
    uint8 reserved1;
    uint8 reserved2;
    uint8 reserved3;
    uint8 reserved4;
} Shared_System_State_Info_t;

/*********************************************************************************************************************/
/*-----------------------------------------------Compile-Time Checks-------------------------------------------------*/
/*********************************************************************************************************************/
typedef char Shared_System_State_Info_t_Size_Check[
    (sizeof(Shared_System_State_Info_t) == 8U) ? 1 : -1];

#endif /* SHARED_SYSTEM_STATE_H_ */
