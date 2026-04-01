#include "Base_Ds18b20_Fsm.h"
#include "Base_Ds18b20.h"
#include "Base_OneWire.h"

/* 온도 값에 대한 일종의 Producer 모듈
 */
static void   Ds18b20_Fsm_SetError(Ds18b20_Context_t* context);
static sint16 Ds18b20_ConvertRawToDeciC(uint8 temp_lsb, uint8 temp_msb);


void Ds18b20_Fsm_Init(Ds18b20_Context_t* context)
{
    if (context == NULL_PTR)
    {
        return;
    }
    Ds18b20_InitSimple();
    context->state                  = DS18B20_STATE_IDLE;
    context->onewire                = &OneWire;
    context->temp_x10               = 0;
    context->is_busy                = FALSE;
    context->is_data_ready          = FALSE;
    context->is_error               = FALSE;
}


boolean Ds18b20_Fsm_RequestStart(Ds18b20_Context_t* context)
{
    if ((context == NULL_PTR) || (context->onewire == NULL_PTR))
    {
        return FALSE;
    }

    if (context->is_busy == TRUE)
    {
        return FALSE;
    }

    context->state                  = DS18B20_STATE_CONVERT_SEND_RESET;
    context->temp_x10               = 0;
    context->is_busy                = TRUE;
    context->is_data_ready          = FALSE;
    context->is_error               = FALSE;

    return TRUE;
}


void Ds18b20_Fsm_MainFunction(Ds18b20_Context_t* context)
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
        case DS18B20_STATE_IDLE:
        {
            break;
        }

        case DS18B20_STATE_CONVERT_SEND_RESET:
        {
            reset_result = OneWire_Reset(context->onewire);
            if (reset_result == DS18B20_RESET_OK)
            {
                context->state = DS18B20_STATE_CONVERT_SEND_SKIP_ROM;
            }
            else
            {
                Ds18b20_Fsm_SetError(context);
            }
            break;
        }

        case DS18B20_STATE_CONVERT_SEND_SKIP_ROM:
        {
            OneWire_WriteByte(context->onewire, ONEWIRE_CMD_SKIPROM);
            context->state = DS18B20_STATE_CONVERT_SEND_CONVERT_T;
            break;
        }

        case DS18B20_STATE_CONVERT_SEND_CONVERT_T:
        {
            OneWire_WriteByte(context->onewire, DS18B20_CMD_CONVERTTEMP);
            context->state = DS18B20_STATE_CONVERT_WAIT_CONVERSION;
            break;
        }

        case DS18B20_STATE_CONVERT_WAIT_CONVERSION:
        {
            if (Ds18b20_AllDone(context->onewire) == TRUE)
            {
                context->state = DS18B20_STATE_READ_SEND_RESET;
            }
            else
            {
                /* stay */
            }
            break;
        }

        case DS18B20_STATE_READ_SEND_RESET:
        {
            reset_result = OneWire_Reset(context->onewire);
            if (reset_result == DS18B20_RESET_OK)
            {
                context->state = DS18B20_STATE_READ_SKIP_ROM;
            }
            else
            {
                Ds18b20_Fsm_SetError(context);
            }
            break;
        }

        case DS18B20_STATE_READ_SKIP_ROM:
        {
            OneWire_WriteByte(context->onewire, ONEWIRE_CMD_SKIPROM);
            context->state = DS18B20_STATE_READ_SEND_READ_SCRATCHPAD;
            break;
        }

        case DS18B20_STATE_READ_SEND_READ_SCRATCHPAD:
        {
            OneWire_WriteByte(context->onewire, ONEWIRE_CMD_RSCRATCHPAD);
            context->state = DS18B20_STATE_READ_RECEIVE_SCRATCHPAD;
            break;
        }

        case DS18B20_STATE_READ_RECEIVE_SCRATCHPAD:
        {
            for (i = 0U; i < 9U; i++) // TODO: 추후 줄일 수 있음
            {
                scratchpad[i] = OneWire_ReadByte(context->onewire);
            }

            temp_x10          = Ds18b20_ConvertRawToDeciC(scratchpad[0], scratchpad[1]);
            context->temp_x10 = temp_x10;
            context->state    = DS18B20_STATE_POST_READ_RESET;
            break;
        }

        case DS18B20_STATE_POST_READ_RESET:
        {
            reset_result = OneWire_Reset(context->onewire);
            if (reset_result == DS18B20_RESET_OK)
            {
                context->is_busy       = FALSE;
                context->is_data_ready = TRUE;
                context->is_error      = FALSE;
                context->state         = DS18B20_STATE_COMPLETE;
            }
            else
            {
                Ds18b20_Fsm_SetError(context);
            }
            break;
        }

        case DS18B20_STATE_COMPLETE:
        {
            break;
        }

        case DS18B20_STATE_ERROR:
        {
            break;
        }

        default:
        {
            Ds18b20_Fsm_SetError(context);
            break;
        }
    }
}


boolean Ds18b20_Fsm_IsBusy(const Ds18b20_Context_t* context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return context->is_busy;
}


boolean Ds18b20_Fsm_IsComplete(const Ds18b20_Context_t* context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return (boolean)((context->state == DS18B20_STATE_COMPLETE) &&
                     (context->is_data_ready == TRUE));
}


boolean Ds18b20_Fsm_IsError(const Ds18b20_Context_t* context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return context->is_error;
}


Ds18b20_State_t Ds18b20_Fsm_GetState(const Ds18b20_Context_t* context)
{
    if (context == NULL_PTR)
    {
        return DS18B20_STATE_ERROR;
    }

    return context->state;
}


boolean Ds18b20_Fsm_GetTemperatureX10(
    const Ds18b20_Context_t* context,
    sint16*                  temp_x10)
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


static void Ds18b20_Fsm_SetError(Ds18b20_Context_t* context)
{
    context->state         = DS18B20_STATE_ERROR;
    context->is_busy       = FALSE;
    context->is_data_ready = FALSE;
    context->is_error      = TRUE;
}


static sint16 Ds18b20_ConvertRawToDeciC(uint8 temp_lsb, uint8 temp_msb)
{
    sint16 raw_temp;
    sint32 deci_c;

    raw_temp = (sint16)(((uint16)temp_msb << 8U) | (uint16)temp_lsb);
    deci_c   = ((sint32)raw_temp * 10) / 16;

    return (sint16)deci_c;
}
