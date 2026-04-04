#ifndef APP_MANAGER_SYSTEM_H_
#define APP_MANAGER_SYSTEM_H_

#include "Platform_Types.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"

#define APP_MANAGER_SYSTEM_SETUP_HOLD_MS                  (100U)
#define APP_MANAGER_SYSTEM_DENIED_TIMEOUT_MS              (10000U)

#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_HIGH_X10        (500)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_CLEAR_HIGH_X10  (400)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_LOW_X10         (-200)
#define APP_MANAGER_SYSTEM_TEMP_EMERGENCY_CLEAR_LOW_X10   (-100)

typedef struct
{
    boolean                auth_event_valid;
    uint8                  active_profile_index;
    Shared_Profile_Table_t profile_table;
} App_Manager_System_Input_t;

typedef struct
{
    uint8                  current_state;
    sint8                  temperature;
    uint8                  active_profile_index;
    Shared_Profile_Table_t profile_table;

} App_Manager_System_Output_t;

void App_Manager_System_Init(void);

void App_Manager_System_Run(uint32 now_ms,
                            sint16 local_temperature_x10,
                            const App_Manager_System_Input_t *input,
                            App_Manager_System_Output_t *output);

Shared_System_State_t App_Manager_System_GetState(void);
uint8 App_Manager_System_GetActiveProfileIndex(void);

#endif /* APP_MANAGER_SYSTEM_H_ */
