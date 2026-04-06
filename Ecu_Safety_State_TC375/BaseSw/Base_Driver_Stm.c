#include "Base_Driver_Stm.h"

#include "Bsp.h"
#include "IfxCpu.h"
#include "IfxStm.h"

#define BASE_DRIVER_STM_ISR_PRIORITY    (0x64u)

Base_Driver_Stm_t                g_base_driver_stm;
volatile uint32                  g_base_driver_stm_counter_1ms = 0u;
Base_Driver_Stm_SchedulingFlag_t g_base_driver_stm_scheduling_flag = {0};

static Ifx_TickTime g_base_driver_stm_ticks_1ms;

IFX_INTERRUPT(Base_Driver_Stm_Isr, 0, BASE_DRIVER_STM_ISR_PRIORITY);

void Base_Driver_Stm_Init(void)
{
    boolean interrupt_state = IfxCpu_disableInterrupts();

    IfxStm_enableOcdsSuspend(&MODULE_STM0);

    g_base_driver_stm_ticks_1ms = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1u);

    g_base_driver_stm.stm_sfr = &MODULE_STM0;
    IfxStm_initCompareConfig(&g_base_driver_stm.stm_config);

    g_base_driver_stm.stm_config.triggerPriority = BASE_DRIVER_STM_ISR_PRIORITY;
    g_base_driver_stm.stm_config.typeOfService   = IfxSrc_Tos_cpu0;
    g_base_driver_stm.stm_config.ticks           = (uint32)g_base_driver_stm_ticks_1ms;

    IfxStm_initCompare(g_base_driver_stm.stm_sfr, &g_base_driver_stm.stm_config);

    Base_Driver_Stm_ClearSchedulingFlags();
    g_base_driver_stm_counter_1ms = 0u;

    IfxCpu_restoreInterrupts(interrupt_state);
}

void Base_Driver_Stm_ClearSchedulingFlags(void)
{
    g_base_driver_stm_scheduling_flag.scheduling_1ms_flag   = 0u;
    g_base_driver_stm_scheduling_flag.scheduling_10ms_flag  = 0u;
    g_base_driver_stm_scheduling_flag.scheduling_100ms_flag = 0u;
    g_base_driver_stm_scheduling_flag.scheduling_1s_flag    = 0u;
    g_base_driver_stm_scheduling_flag.scheduling_10s_flag   = 0u;
}

void Base_Driver_Stm_Isr(void)
{
    IfxStm_clearCompareFlag(g_base_driver_stm.stm_sfr,
                            g_base_driver_stm.stm_config.comparator);

    IfxStm_increaseCompare(g_base_driver_stm.stm_sfr,
                           g_base_driver_stm.stm_config.comparator,
                           (uint32)g_base_driver_stm_ticks_1ms);

    g_base_driver_stm_counter_1ms++;

    g_base_driver_stm_scheduling_flag.scheduling_1ms_flag = 1u;

    if ((g_base_driver_stm_counter_1ms % 10u) == 0u)
    {
        g_base_driver_stm_scheduling_flag.scheduling_10ms_flag = 1u;
    }

    if ((g_base_driver_stm_counter_1ms % 100u) == 0u)
    {
        g_base_driver_stm_scheduling_flag.scheduling_100ms_flag = 1u;
    }

    if ((g_base_driver_stm_counter_1ms % 1000u) == 0u)
    {
        g_base_driver_stm_scheduling_flag.scheduling_1s_flag = 1u;
    }

    if ((g_base_driver_stm_counter_1ms % 10000u) == 0u)
    {
        g_base_driver_stm_scheduling_flag.scheduling_10s_flag = 1u;
    }
}

void Base_Driver_Stm_GetAndClearSchedulingFlags(Base_Driver_Stm_SchedulingFlag_t *flags)
{
    boolean interrupt_state;

    if (flags == NULL_PTR)
    {
        return;
    }

    interrupt_state = IfxCpu_disableInterrupts();

    *flags = g_base_driver_stm_scheduling_flag;
    Base_Driver_Stm_ClearSchedulingFlags();

    IfxCpu_restoreInterrupts(interrupt_state);
}

uint32 Base_Driver_Stm_GetNowMs(void)
{
    uint32  now_ms;
    boolean interrupt_state;

    interrupt_state = IfxCpu_disableInterrupts();
    now_ms = g_base_driver_stm_counter_1ms;
    IfxCpu_restoreInterrupts(interrupt_state);

    return now_ms;
}
