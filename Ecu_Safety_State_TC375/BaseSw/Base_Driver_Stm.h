#ifndef BASE_DRIVER_STM_H_
#define BASE_DRIVER_STM_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    Ifx_STM               *stm_sfr;
    IfxStm_CompareConfig   stm_config;
} Base_Driver_Stm_t;

typedef struct
{
    uint8 scheduling_1ms_flag;
    uint8 scheduling_10ms_flag;
    uint8 scheduling_100ms_flag;
    uint8 scheduling_1s_flag;
    uint8 scheduling_10s_flag;
} Base_Driver_Stm_SchedulingFlag_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void   Base_Driver_Stm_Init(void);
void   Base_Driver_Stm_ClearSchedulingFlags(void);
void   Base_Driver_Stm_GetAndClearSchedulingFlags(Base_Driver_Stm_SchedulingFlag_t *flags);
uint32 Base_Driver_Stm_GetNowMs(void);

#endif /* BASE_DRIVER_STM_H_ */
