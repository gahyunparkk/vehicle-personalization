#include "Base_Driver_Ds18b20.h"
#include "Base_Driver_Ds18b20_Legacy.h"
#include "Base_Com_OneWire.h"
#include "Platform_Types.h"

/* 온도 값에 대한 일종의 Producer 모듈 */
static void   Base_Driver_Ds18b20_SetError(Base_Driver_Ds18b20_Context_t *context);
static sint16 Base_Driver_Ds18b20_ConvertRawToDeciC(uint8 temp_lsb, uint8 temp_msb);

void Base_Driver_Ds18b20_Init(Base_Driver_Ds18b20_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return;
    }

    Base_Driver_Ds18b20_InitSimple();
    context->state         = BASE_DRIVER_DS18B20_STATE_IDLE;
    context->onewire       = &g_base_com_onewire;
    context->temp_x10      = 0;
    context->is_busy       = FALSE;
    context->is_data_ready = FALSE;
    context->is_error      = FALSE;
}

boolean Base_Driver_Ds18b20_RequestStart(Base_Driver_Ds18b20_Context_t *context)
{
    if ((context == NULL_PTR) || (context->onewire == NULL_PTR))
    {
        return FALSE;
    }

    if (context->is_busy == TRUE)
    {
        return FALSE;
    }

    context->state         = BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_RESET;
    context->temp_x10      = 0;
    context->is_busy       = TRUE;
    context->is_data_ready = FALSE;
    context->is_error      = FALSE;

    return TRUE;
}

void Base_Driver_Ds18b20_MainFunction(Base_Driver_Ds18b20_Context_t *context)
{
    uint8  scratchpad[9];
    uint8  i;
    uint8  reset_result;
    sint16 temp_x10;

    if ((context == NULL_PTR) || (context->onewire == NULL_PTR))
    {
        return;
    }

    switch (context->state)
    {
        case BASE_DRIVER_DS18B20_STATE_IDLE:
        {
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_RESET:
        {
            reset_result = Base_Com_OneWire_Reset(context->onewire);
            if (reset_result == BASE_DRIVER_DS18B20_RESET_OK)
            {
                context->state = BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_SKIP_ROM;
            }
            else
            {
                Base_Driver_Ds18b20_SetError(context);
            }
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_SKIP_ROM:
        {
            Base_Com_OneWire_WriteByte(context->onewire, BASE_COM_ONEWIRE_CMD_SKIPROM);
            context->state = BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_CONVERT_T;
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_CONVERT_SEND_CONVERT_T:
        {
            Base_Com_OneWire_WriteByte(context->onewire, BASE_DRIVER_DS18B20_CMD_CONVERTTEMP);
            context->state = BASE_DRIVER_DS18B20_STATE_CONVERT_WAIT_CONVERSION;
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_CONVERT_WAIT_CONVERSION:
        {
            if (Base_Driver_Ds18b20_AllDone(context->onewire) == TRUE)
            {
                context->state = BASE_DRIVER_DS18B20_STATE_READ_SEND_RESET;
            }
            else
            {
                /* stay */
            }
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_READ_SEND_RESET:
        {
            reset_result = Base_Com_OneWire_Reset(context->onewire);
            if (reset_result == BASE_DRIVER_DS18B20_RESET_OK)
            {
                context->state = BASE_DRIVER_DS18B20_STATE_READ_SKIP_ROM;
            }
            else
            {
                Base_Driver_Ds18b20_SetError(context);
            }
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_READ_SKIP_ROM:
        {
            Base_Com_OneWire_WriteByte(context->onewire, BASE_COM_ONEWIRE_CMD_SKIPROM);
            context->state = BASE_DRIVER_DS18B20_STATE_READ_SEND_READ_SCRATCHPAD;
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_READ_SEND_READ_SCRATCHPAD:
        {
            Base_Com_OneWire_WriteByte(context->onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);
            context->state = BASE_DRIVER_DS18B20_STATE_READ_RECEIVE_SCRATCHPAD;
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_READ_RECEIVE_SCRATCHPAD:
        {
            for (i = 0U; i < 9U; i++)
            {
                scratchpad[i] = Base_Com_OneWire_ReadByte(context->onewire);
            }

            temp_x10          = Base_Driver_Ds18b20_ConvertRawToDeciC(scratchpad[0], scratchpad[1]);
            context->temp_x10 = temp_x10;
            context->state    = BASE_DRIVER_DS18B20_STATE_POST_READ_RESET;
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_POST_READ_RESET:
        {
            reset_result = Base_Com_OneWire_Reset(context->onewire);
            if (reset_result == BASE_DRIVER_DS18B20_RESET_OK)
            {
                context->is_busy       = FALSE;
                context->is_data_ready = TRUE;
                context->is_error      = FALSE;
                context->state         = BASE_DRIVER_DS18B20_STATE_COMPLETE;
            }
            else
            {
                Base_Driver_Ds18b20_SetError(context);
            }
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_COMPLETE:
        {
            break;
        }

        case BASE_DRIVER_DS18B20_STATE_ERROR:
        {
            break;
        }

        default:
        {
            Base_Driver_Ds18b20_SetError(context);
            break;
        }
    }
}

boolean Base_Driver_Ds18b20_IsBusy(const Base_Driver_Ds18b20_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return context->is_busy;
}

boolean Base_Driver_Ds18b20_IsComplete(const Base_Driver_Ds18b20_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return (boolean)((context->state == BASE_DRIVER_DS18B20_STATE_COMPLETE) &&
                     (context->is_data_ready == TRUE));
}

boolean Base_Driver_Ds18b20_IsError(const Base_Driver_Ds18b20_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return context->is_error;
}

Base_Driver_Ds18b20_State_t Base_Driver_Ds18b20_GetState(
    const Base_Driver_Ds18b20_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return BASE_DRIVER_DS18B20_STATE_ERROR;
    }

    return context->state;
}

boolean Base_Driver_Ds18b20_GetTemperatureX10(
    const Base_Driver_Ds18b20_Context_t *context,
    sint16                              *temp_x10)
{
    if ((context == NULL_PTR) || (temp_x10 == NULL_PTR))
    {
        return FALSE;
    }

    if (context->is_data_ready != TRUE)
    {
        return FALSE;
    }

    *temp_x10 = context->temp_x10;
    return TRUE;
}

static void Base_Driver_Ds18b20_SetError(Base_Driver_Ds18b20_Context_t *context)
{
    context->state         = BASE_DRIVER_DS18B20_STATE_ERROR;
    context->is_busy       = FALSE;
    context->is_data_ready = FALSE;
    context->is_error      = TRUE;
}

static sint16 Base_Driver_Ds18b20_ConvertRawToDeciC(uint8 temp_lsb, uint8 temp_msb)
{
    sint16 raw_temp;
    sint32 deci_c;

    raw_temp = (sint16)(((uint16)temp_msb << 8U) | (uint16)temp_lsb);
    deci_c   = ((sint32)raw_temp * 10) / 16;

    return (sint16)deci_c;
}
