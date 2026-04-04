#include "Shared_Util_Time.h"
#include "IfxStm.h"

uint32 Shared_Util_Time_GetNowMs(void)
{
    uint64 ticks = IfxStm_get(&MODULE_STM0);
    uint64 freq  = (uint64)IfxStm_getFrequency(&MODULE_STM0);

    return (uint32)(ticks / (freq / 1000ULL));
}