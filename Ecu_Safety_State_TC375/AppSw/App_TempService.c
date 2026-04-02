#include "App_TempService.h"

typedef struct
{
    sint16  latest_temperature_x10;
    boolean is_data_valid;
    boolean is_update_requested;
    boolean is_error;
    boolean is_initialized;
} App_TempService_Data_t;

static Ds18b20_Context_t      g_ds18b20_context;
static App_TempService_Data_t g_temp_service;

static void App_TempService_ResetFsmToIdle(void);

void App_TempService_Init(void)
{
    Ds18b20_Fsm_Init(&g_ds18b20_context);

    g_temp_service.latest_temperature_x10 = 0;
    g_temp_service.is_data_valid          = FALSE;
    g_temp_service.is_update_requested    = FALSE;
    g_temp_service.is_error               = FALSE;
    g_temp_service.is_initialized         = TRUE;
}

void App_TempService_MainFunction(void)
{
    sint16 temperature_x10;

    if (g_temp_service.is_initialized != TRUE)
    {
        return;
    }

    /* 요청이 들어왔고 FSM이 쉬고 있으면 측정 시작 */
    if ((g_temp_service.is_update_requested == TRUE) &&
        (Ds18b20_Fsm_IsBusy(&g_ds18b20_context) != TRUE) &&
        (Ds18b20_Fsm_GetState(&g_ds18b20_context) == DS18B20_STATE_IDLE))
    {
        if (Ds18b20_Fsm_RequestStart(&g_ds18b20_context) != TRUE)
        {
            g_temp_service.is_error            = TRUE;
            g_temp_service.is_update_requested = FALSE;
            App_TempService_ResetFsmToIdle();
            return;
        }
    }

    /* 내부 FSM 1 step 진행 */
    Ds18b20_Fsm_MainFunction(&g_ds18b20_context);

    /* 완료 시 최신값 캐시 */
    if (Ds18b20_Fsm_IsComplete(&g_ds18b20_context) == TRUE)
    {
        if (Ds18b20_Fsm_GetTemperatureX10(&g_ds18b20_context, &temperature_x10) == TRUE)
        {
            g_temp_service.latest_temperature_x10 = temperature_x10;
            g_temp_service.is_data_valid          = TRUE;
            g_temp_service.is_error               = FALSE;
        }
        else
        {
            g_temp_service.is_error = TRUE;
        }

        g_temp_service.is_update_requested = FALSE;
        App_TempService_ResetFsmToIdle();
    }
    else if (Ds18b20_Fsm_IsError(&g_ds18b20_context) == TRUE)
    {
        g_temp_service.is_error            = TRUE;
        g_temp_service.is_update_requested = FALSE;
        App_TempService_ResetFsmToIdle();
    }
}

boolean App_TempService_RequestUpdate(void)
{
    if (g_temp_service.is_initialized != TRUE)
    {
        return FALSE;
    }

    if (Ds18b20_Fsm_IsBusy(&g_ds18b20_context) == TRUE)
    {
        return FALSE;
    }

    if (g_temp_service.is_update_requested == TRUE)
    {
        return FALSE;
    }

    g_temp_service.is_update_requested = TRUE;
    g_temp_service.is_error            = FALSE;

    return TRUE;
}

boolean App_TempService_IsBusy(void)
{
    if (g_temp_service.is_initialized != TRUE)
    {
        return FALSE;
    }

    return (boolean)((g_temp_service.is_update_requested == TRUE) ||
                     (Ds18b20_Fsm_IsBusy(&g_ds18b20_context) == TRUE));
}

boolean App_TempService_IsDataValid(void)
{
    if (g_temp_service.is_initialized != TRUE)
    {
        return FALSE;
    }

    return g_temp_service.is_data_valid;
}

boolean App_TempService_IsError(void)
{
    if (g_temp_service.is_initialized != TRUE)
    {
        return FALSE;
    }

    return g_temp_service.is_error;
}

boolean App_TempService_GetLatestTemperatureX10(sint16* temperature_x10)
{
    if ((g_temp_service.is_initialized != TRUE) || (temperature_x10 == NULL_PTR))
    {
        return FALSE;
    }

    if (g_temp_service.is_data_valid != TRUE)
    {
        return FALSE;
    }

    *temperature_x10 = g_temp_service.latest_temperature_x10;
    return TRUE;
}

Ds18b20_State_t App_TempService_GetFsmState(void)
{
    if (g_temp_service.is_initialized != TRUE)
    {
        return DS18B20_STATE_ERROR;
    }

    return Ds18b20_Fsm_GetState(&g_ds18b20_context);
}

static void App_TempService_ResetFsmToIdle(void)
{
    g_ds18b20_context.state         = DS18B20_STATE_IDLE;
    g_ds18b20_context.is_busy       = FALSE;
    g_ds18b20_context.is_data_ready = FALSE;
    g_ds18b20_context.is_error      = FALSE;
}
