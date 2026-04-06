#include "IfxStm.h"
#include "IfxCpu_Irq.h"
#include "Bsp.h"

typedef struct {
        Ifx_STM                 *stmSfr;
        IfxStm_CompareConfig    stmConfig;
        volatile uint8          LedBlink;
        volatile unsigned int   counter;
} App_stm;

typedef struct
{
        uint8 u8nuScheduling1msFlag;
        uint8 u8nuScheduling10msFlag;
        uint8 u8nuScheduling100msFlag;
        uint8 u8nuScheduling1000msFlag;
} SchedulingFlag;

typedef struct
{
        unsigned int u32nuCnt1ms;
        unsigned int u32nuCnt10ms;
        unsigned int u32nuCnt100ms;
        unsigned int u32nuCnt1000ms;
} stCnt;

extern volatile SchedulingFlag stSchedulingInfo;

void initSTM(void);
void Driver_Stm_Init(void);
