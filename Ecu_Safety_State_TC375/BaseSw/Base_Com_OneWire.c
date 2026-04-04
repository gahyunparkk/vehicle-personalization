/**
 * |----------------------------------------------------------------------
 * | Copyright (C) Hyeongmin Kim, 2026
 * |
 * | This program is free software: you can redistribute it and/or modify
 * | it under the terms of the GNU General Public License as published by
 * | the Free Software Foundation, either version 3 of the License, or
 * | any later version.
 * |
 * | This program is distributed in the hope that it will be useful,
 * | but WITHOUT ANY WARRANTY; without even the implied warranty of
 * | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * | GNU General Public License for more details.
 * |
 * | You should have received a copy of the GNU General Public License
 * | along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * |----------------------------------------------------------------------
 */
#include "Platform_Types.h"
#include "IfxStm.h"
#include "IfxPort.h"
#include "Bsp.h"

#include "Base_Driver_Ds18b20_Config.h"
#include "Base_Com_OneWire.h"

static volatile uint8 m_busy_line = 0U;

#define BASE_COM_ONEWIRE_BUSY_ON()   \
    do                               \
    {                                \
        m_busy_line = 1U;            \
    } while (0)

#define BASE_COM_ONEWIRE_BUSY_OFF()  \
    do                               \
    {                                \
        m_busy_line = 0U;            \
    } while (0)

void Base_Com_OneWire_Delay(uint16 time_us)
{
    waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, time_us));
}

void Base_Com_OneWire_DriveLow(Base_Com_OneWire_t *oneWire)
{
    IfxPort_setPinLow(oneWire->dq.port, oneWire->dq.pinIndex);
}

void Base_Com_OneWire_Release(Base_Com_OneWire_t *oneWire)
{
    IfxPort_setPinHigh(oneWire->dq.port, oneWire->dq.pinIndex);
}

uint8 Base_Com_OneWire_IsBusyLine(void)
{
    return m_busy_line;
}

void Base_Com_OneWire_Init(Base_Com_OneWire_t *oneWire, Ifx_P *port, uint8 pinIndex)
{
    m_busy_line = 0U;

    oneWire->dq.port     = port;
    oneWire->dq.pinIndex = pinIndex;

    IfxPort_setPinMode(port, pinIndex, IfxPort_Mode_outputOpenDrainGeneral);

    Base_Com_OneWire_Release(oneWire);
}

uint8 Base_Com_OneWire_Reset(Base_Com_OneWire_t *oneWire)
{
    uint8 presence;

    Base_Com_OneWire_DriveLow(oneWire);
    Base_Com_OneWire_Delay(480U);

    Base_Com_OneWire_Release(oneWire);
    Base_Com_OneWire_Delay(70U);

    presence = IfxPort_getPinState(oneWire->dq.port, oneWire->dq.pinIndex);

    Base_Com_OneWire_Delay(410U);

    return presence; /* 0: presence detected, 1: no device */
}

void Base_Com_OneWire_WriteBit(Base_Com_OneWire_t *oneWire, uint8 bit)
{
    if (bit != 0U)
    {
        Base_Com_OneWire_DriveLow(oneWire);
        Base_Com_OneWire_Delay(10U);

        Base_Com_OneWire_Release(oneWire);
        Base_Com_OneWire_Delay(55U);
    }
    else
    {
        Base_Com_OneWire_DriveLow(oneWire);
        Base_Com_OneWire_Delay(65U);

        Base_Com_OneWire_Release(oneWire);
        Base_Com_OneWire_Delay(5U);
    }
}

uint8 Base_Com_OneWire_ReadBit(Base_Com_OneWire_t *oneWire)
{
    uint8 bit = 0U;

    Base_Com_OneWire_DriveLow(oneWire);
    Base_Com_OneWire_Delay(2U);

    Base_Com_OneWire_Release(oneWire);
    Base_Com_OneWire_Delay(10U);

    if (IfxPort_getPinState(oneWire->dq.port, oneWire->dq.pinIndex) != 0U)
    {
        bit = 1U;
    }

    Base_Com_OneWire_Delay(50U);

    return bit;
}

void Base_Com_OneWire_WriteByte(Base_Com_OneWire_t *oneWire, uint8 byte)
{
    uint8 i = 8U;

    while (i-- != 0U)
    {
        Base_Com_OneWire_WriteBit(oneWire, (uint8)(byte & 0x01U));
        byte >>= 1;
    }
}

uint8 Base_Com_OneWire_ReadByte(Base_Com_OneWire_t *oneWire)
{
    uint8 i    = 8U;
    uint8 byte = 0U;

    while (i-- != 0U)
    {
        byte >>= 1;
        byte |= (uint8)(Base_Com_OneWire_ReadBit(oneWire) << 7);
    }

    return byte;
}

uint8 Base_Com_OneWire_First(Base_Com_OneWire_t *oneWire)
{
    Base_Com_OneWire_ResetSearch(oneWire);
    return Base_Com_OneWire_Search(oneWire, BASE_COM_ONEWIRE_CMD_SEARCHROM);
}

uint8 Base_Com_OneWire_Next(Base_Com_OneWire_t *oneWire)
{
    return Base_Com_OneWire_Search(oneWire, BASE_COM_ONEWIRE_CMD_SEARCHROM);
}

void Base_Com_OneWire_ResetSearch(Base_Com_OneWire_t *oneWire)
{
    oneWire->lastDiscrepancy       = 0U;
    oneWire->lastDeviceFlag        = 0U;
    oneWire->lastFamilyDiscrepancy = 0U;
}

int Base_Com_OneWire_Verify(Base_Com_OneWire_t *oneWire)
{
    uint8 rom_backup[8];
    uint8 i;
    uint8 result;
    uint8 ld_backup;
    uint8 ldf_backup;
    uint8 lfd_backup;

    for (i = 0U; i < 8U; i++)
    {
        rom_backup[i] = oneWire->romNo[i];
    }

    ld_backup  = oneWire->lastDiscrepancy;
    ldf_backup = oneWire->lastDeviceFlag;
    lfd_backup = oneWire->lastFamilyDiscrepancy;

    oneWire->lastDiscrepancy = 64U;
    oneWire->lastDeviceFlag  = 0U;

    if (Base_Com_OneWire_Search(oneWire, BASE_COM_ONEWIRE_CMD_SEARCHROM) != 0U)
    {
        result = 1U;

        for (i = 0U; i < 8U; i++)
        {
            if (rom_backup[i] != oneWire->romNo[i])
            {
                result = 0U;
                break;
            }
        }
    }
    else
    {
        result = 0U;
    }

    for (i = 0U; i < 8U; i++)
    {
        oneWire->romNo[i] = rom_backup[i];
    }

    oneWire->lastDiscrepancy       = ld_backup;
    oneWire->lastDeviceFlag        = ldf_backup;
    oneWire->lastFamilyDiscrepancy = lfd_backup;

    return result;
}

void Base_Com_OneWire_TargetSetup(Base_Com_OneWire_t *oneWire, uint8 familyCode)
{
    uint8 i;

    oneWire->romNo[0] = familyCode;

    for (i = 1U; i < 8U; i++)
    {
        oneWire->romNo[i] = 0U;
    }

    oneWire->lastDiscrepancy       = 64U;
    oneWire->lastFamilyDiscrepancy = 0U;
    oneWire->lastDeviceFlag        = 0U;
}

void Base_Com_OneWire_FamilySkipSetup(Base_Com_OneWire_t *oneWire)
{
    oneWire->lastDiscrepancy       = oneWire->lastFamilyDiscrepancy;
    oneWire->lastFamilyDiscrepancy = 0U;

    if (oneWire->lastDiscrepancy == 0U)
    {
        oneWire->lastDeviceFlag = 1U;
    }
}

uint8 Base_Com_OneWire_GetRom(Base_Com_OneWire_t *oneWire, uint8 index)
{
    return oneWire->romNo[index];
}

void Base_Com_OneWire_Select(Base_Com_OneWire_t *oneWire, uint8 *addr)
{
    uint8 i;

    Base_Com_OneWire_WriteByte(oneWire, BASE_COM_ONEWIRE_CMD_MATCHROM);

    for (i = 0U; i < 8U; i++)
    {
        Base_Com_OneWire_WriteByte(oneWire, *(addr + i));
    }
}

uint8 Base_Com_OneWire_Crc8(uint8 *addr, uint8 len)
{
    uint8 crc    = 0U;
    uint8 inbyte;
    uint8 i;
    uint8 mix;

    while (len-- != 0U)
    {
        inbyte = *addr++;

        for (i = 8U; i > 0U; i--)
        {
            mix = (uint8)((crc ^ inbyte) & 0x01U);
            crc >>= 1;

            if (mix != 0U)
            {
                crc ^= 0x8CU;
            }

            inbyte >>= 1;
        }
    }

    return crc;
}

void Base_Com_OneWire_SelectWithPointer(Base_Com_OneWire_t *oneWire, uint8 *rom)
{
    uint8 i;

    Base_Com_OneWire_WriteByte(oneWire, BASE_COM_ONEWIRE_CMD_MATCHROM);

    for (i = 0U; i < 8U; i++)
    {
        Base_Com_OneWire_WriteByte(oneWire, *(rom + i));
    }
}

void Base_Com_OneWire_GetFullRom(Base_Com_OneWire_t *oneWire, uint8 *firstIndex)
{
    uint8 i;

    for (i = 0U; i < 8U; i++)
    {
        *(firstIndex + i) = oneWire->romNo[i];
    }
}

uint8 Base_Com_OneWire_Search(Base_Com_OneWire_t *oneWire, uint8 command)
{
    uint8 id_bit_number;
    uint8 last_zero;
    uint8 rom_byte_number;
    uint8 search_result;
    uint8 id_bit;
    uint8 cmp_id_bit;
    uint8 rom_byte_mask;
    uint8 search_direction;

    id_bit_number  = 1U;
    last_zero      = 0U;
    rom_byte_number = 0U;
    rom_byte_mask  = 1U;
    search_result  = 0U;

    if (oneWire->lastDeviceFlag == 0U)
    {
        if (Base_Com_OneWire_Reset(oneWire) != 0U)
        {
            oneWire->lastDiscrepancy       = 0U;
            oneWire->lastDeviceFlag        = 0U;
            oneWire->lastFamilyDiscrepancy = 0U;
            return 0U;
        }

        Base_Com_OneWire_WriteByte(oneWire, command);

        do
        {
            id_bit     = Base_Com_OneWire_ReadBit(oneWire);
            cmp_id_bit = Base_Com_OneWire_ReadBit(oneWire);

            if ((id_bit == 1U) && (cmp_id_bit == 1U))
            {
                break;
            }
            else
            {
                if (id_bit != cmp_id_bit)
                {
                    search_direction = id_bit;
                }
                else
                {
                    if (id_bit_number < oneWire->lastDiscrepancy)
                    {
                        search_direction = (uint8)((oneWire->romNo[rom_byte_number] & rom_byte_mask) > 0U);
                    }
                    else
                    {
                        search_direction = (uint8)(id_bit_number == oneWire->lastDiscrepancy);
                    }

                    if (search_direction == 0U)
                    {
                        last_zero = id_bit_number;

                        if (last_zero < 9U)
                        {
                            oneWire->lastFamilyDiscrepancy = last_zero;
                        }
                    }
                }

                if (search_direction == 1U)
                {
                    oneWire->romNo[rom_byte_number] |= rom_byte_mask;
                }
                else
                {
                    oneWire->romNo[rom_byte_number] &= (uint8)(~rom_byte_mask);
                }

                Base_Com_OneWire_WriteBit(oneWire, search_direction);

                id_bit_number++;
                rom_byte_mask <<= 1;

                if (rom_byte_mask == 0U)
                {
                    rom_byte_number++;
                    rom_byte_mask = 1U;
                }
            }
        } while (rom_byte_number < 8U);

        if (id_bit_number >= 65U)
        {
            oneWire->lastDiscrepancy = last_zero;

            if (oneWire->lastDiscrepancy == 0U)
            {
                oneWire->lastDeviceFlag = 1U;
            }

            search_result = 1U;
        }
    }

    if ((search_result == 0U) || (oneWire->romNo[0] == 0U))
    {
        oneWire->lastDiscrepancy       = 0U;
        oneWire->lastDeviceFlag        = 0U;
        oneWire->lastFamilyDiscrepancy = 0U;
        search_result                  = 0U;
    }

    return search_result;
}
