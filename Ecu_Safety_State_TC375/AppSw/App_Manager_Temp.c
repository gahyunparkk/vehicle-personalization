#include "App_Manager_Temp.h"

typedef struct
{
    sint16  latest_temperature_x10;
    boolean is_data_valid;
    boolean is_update_requested;
    boolean is_error;
    boolean is_initialized;
} App_Manager_Temp_Data_t;

static Base_Driver_Ds18b20_Context_t g_ds18b20_context;
static App_Manager_Temp_Data_t       g_app_manager_temp;

static void App_Manager_Temp_ResetDriverToIdle(void);

void App_Manager_Temp_Init(void)
{
    Base_Driver_Ds18b20_Init(&g_ds18b20_context);

    g_app_manager_temp.latest_temperature_x10 = 0;
    g_app_manager_temp.is_data_valid          = FALSE;
    g_app_manager_temp.is_update_requested    = FALSE;
    g_app_manager_temp.is_error               = FALSE;
    g_app_manager_temp.is_initialized         = TRUE;
}

void App_Manager_Temp_Run(void)
{
    sint16 temperature_x10;

    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return;
    }

    /* 요청이 들어왔고 드라이버가 쉬고 있으면 측정 시작 */
    if ((g_app_manager_temp.is_update_requested == TRUE) &&
        (Base_Driver_Ds18b20_IsBusy(&g_ds18b20_context) != TRUE) &&
        (Base_Driver_Ds18b20_GetState(&g_ds18b20_context) == BASE_DRIVER_DS18B20_STATE_IDLE))
    {
        if (Base_Driver_Ds18b20_RequestStart(&g_ds18b20_context) != TRUE)
        {
            g_app_manager_temp.is_error            = TRUE;
            g_app_manager_temp.is_update_requested = FALSE;
            App_Manager_Temp_ResetDriverToIdle();
            return;
        }
    }

    /* 내부 드라이버 1 step 진행 */
    Base_Driver_Ds18b20_MainFunction(&g_ds18b20_context);

    /* 완료 시 최신값 캐시 */
    if (Base_Driver_Ds18b20_IsComplete(&g_ds18b20_context) == TRUE)
    {
        if (Base_Driver_Ds18b20_GetTemperatureX10(&g_ds18b20_context, &temperature_x10) == TRUE)
        {
            g_app_manager_temp.latest_temperature_x10 = temperature_x10;
            g_app_manager_temp.is_data_valid          = TRUE;
            g_app_manager_temp.is_error               = FALSE;
        }
        else
        {
            g_app_manager_temp.is_error = TRUE;
        }

        g_app_manager_temp.is_update_requested = FALSE;
        App_Manager_Temp_ResetDriverToIdle();
    }
    else if (Base_Driver_Ds18b20_IsError(&g_ds18b20_context) == TRUE)
    {
        g_app_manager_temp.is_error            = TRUE;
        g_app_manager_temp.is_update_requested = FALSE;
        App_Manager_Temp_ResetDriverToIdle();
    }
}

boolean App_Manager_Temp_RequestUpdate(void)
{
    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return FALSE;
    }

    if (Base_Driver_Ds18b20_IsBusy(&g_ds18b20_context) == TRUE)
    {
        return FALSE;
    }

    if (g_app_manager_temp.is_update_requested == TRUE)
    {
        return FALSE;
    }

    g_app_manager_temp.is_update_requested = TRUE;
    g_app_manager_temp.is_error            = FALSE;

    return TRUE;
}

boolean App_Manager_Temp_IsBusy(void)
{
    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return FALSE;
    }

    return (boolean)((g_app_manager_temp.is_update_requested == TRUE) ||
                     (Base_Driver_Ds18b20_IsBusy(&g_ds18b20_context) == TRUE));
}

boolean App_Manager_Temp_IsDataValid(void)
{
    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return FALSE;
    }

    return g_app_manager_temp.is_data_valid;
}

boolean App_Manager_Temp_IsError(void)
{
    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return FALSE;
    }

    return g_app_manager_temp.is_error;
}

boolean App_Manager_Temp_GetLatestTemp_X10(sint16 *temperature_x10)
{
    if ((g_app_manager_temp.is_initialized != TRUE) || (temperature_x10 == NULL_PTR))
    {
        return FALSE;
    }

    if (g_app_manager_temp.is_data_valid != TRUE)
    {
        return FALSE;
    }

    *temperature_x10 = g_app_manager_temp.latest_temperature_x10;
    return TRUE;
}

Base_Driver_Ds18b20_State_t App_Manager_Temp_GetState(void)
{
    if (g_app_manager_temp.is_initialized != TRUE)
    {
        return BASE_DRIVER_DS18B20_STATE_ERROR;
    }

    return Base_Driver_Ds18b20_GetState(&g_ds18b20_context);
}

static void App_Manager_Temp_ResetDriverToIdle(void)
{
    g_ds18b20_context.state         = BASE_DRIVER_DS18B20_STATE_IDLE;
    g_ds18b20_context.is_busy       = FALSE;
    g_ds18b20_context.is_data_ready = FALSE;
    g_ds18b20_context.is_error      = FALSE;
}
