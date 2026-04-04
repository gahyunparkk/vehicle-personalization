#ifndef SHARED_SYSTEM_STATE_H_
#define SHARED_SYSTEM_STATE_H_

#include "Platform_Types.h"

typedef enum
{
    SHARED_SYSTEM_STATE_SLEEP = 0,
    SHARED_SYSTEM_STATE_SETUP,
    SHARED_SYSTEM_STATE_ACTIVATED,
    SHARED_SYSTEM_STATE_SHUTDOWN,
    SHARED_SYSTEM_STATE_DENIED,
    SHARED_SYSTEM_STATE_EMERGENCY
} Shared_System_State_t;

/* total = 8 bytes */
typedef struct
{
    uint8 current_state;
    uint8 active_profile_index;
    uint8 reserved0;
    uint8 reserved1;
    uint8 reserved2;
    uint8 reserved3;
    uint8 reserved4;
    uint8 reserved5;
} Shared_System_State_Info_t;

typedef char Shared_System_State_Info_t_Size_Check[
    (sizeof(Shared_System_State_Info_t) == 8U) ? 1 : -1];

#endif /* SHARED_SYSTEM_STATE_H_ */
