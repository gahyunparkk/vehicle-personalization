#ifndef BASE_DRIVER_STM_H_
#define BASE_DRIVER_STM_H_

#include "Ifx_Types.h"
#include "IfxStm.h"

typedef struct
{
    Ifx_STM              *stm_sfr;
    IfxStm_CompareConfig  stm_config;
} Base_Driver_Stm_t;

typedef struct
{
    volatile uint8 scheduling_1ms_flag;
    volatile uint8 scheduling_10ms_flag;
    volatile uint8 scheduling_100ms_flag;
    volatile uint8 scheduling_1s_flag;
    volatile uint8 scheduling_10s_flag;
} Base_Driver_Stm_SchedulingFlag_t;

extern Base_Driver_Stm_t                g_base_driver_stm;
extern volatile uint32                  g_base_driver_stm_counter_1ms;
extern Base_Driver_Stm_SchedulingFlag_t g_base_driver_stm_scheduling_flag;

void Base_Driver_Stm_Init(void);
void Base_Driver_Stm_ClearSchedulingFlags(void);

#endif /* BASE_DRIVER_STM_H_ */
