#include "Base_Fee.h"

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include <string.h>

#include "IfxFlash.h"
#include "IfxScuWdt.h"

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static uint8        Fee_CalcChecksum8(const uint8 *data, uint32 length);
static uint32       Fee_GetSlotAddress(uint32 slotIndex);
static boolean      Fee_IsSequenceNewer(uint32 candidate, uint32 current);

static Fee_Status_t Fee_ReadHeader(uint32 slotIndex, Fee_Profile_Header_t *outHeader);
static Fee_Status_t Fee_ValidateSlot(uint32 slotIndex,
                                     Fee_Profile_Header_t *outHeader,
                                     Shared_Profile_Table_t *outTable);
static Fee_Status_t Fee_FindLatestValidSlot(uint32 *outSlot,
                                            Fee_Profile_Header_t *outHeader,
                                            Shared_Profile_Table_t *outTable);
static Fee_Status_t Fee_FindFirstEmptySlot(uint32 *outSlot);
static Fee_Status_t Fee_EraseStorage(void);
static Fee_Status_t Fee_WriteSlot(uint32 slotIndex,
                                  const Shared_Profile_Table_t *inTable,
                                  uint32 sequence);

/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
Fee_Status_t Flash_LoadProfileTable(Shared_Profile_Table_t *outTable)
{
    uint32 latestSlot;
    Fee_Profile_Header_t latestHeader;

    if (outTable == NULL_PTR)
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    return Fee_FindLatestValidSlot(&latestSlot, &latestHeader, outTable);
}

Fee_Status_t Flash_SaveProfileTable(const Shared_Profile_Table_t *inTable)
{
    uint32 latestSlot;
    uint32 targetSlot;
    uint32 nextSequence = 0U;
    Fee_Profile_Header_t latestHeader;
    Shared_Profile_Table_t dummyTable;
    Fee_Status_t status;

    if (inTable == NULL_PTR)
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    status = Fee_FindLatestValidSlot(&latestSlot, &latestHeader, &dummyTable);
    if (status == FEE_STATUS_OK)
    {
        nextSequence = latestHeader.sequence + 1U;
    }
    else if ((status == FEE_STATUS_EMPTY) || (status == FEE_STATUS_NOT_FOUND))
    {
        nextSequence = 0U;
    }
    else
    {
        return status;
    }

    status = Fee_FindFirstEmptySlot(&targetSlot);
    if (status != FEE_STATUS_OK)
    {
        status = Fee_EraseStorage();
        if (status != FEE_STATUS_OK)
        {
            return status;
        }

        targetSlot = 0U;
    }

    return Fee_WriteSlot(targetSlot, inTable, nextSequence);
}

Fee_Status_t Flash_FormatStorage(void)
{
    return Fee_EraseStorage();
}

uint32 Flash_GetSlotCount(void)
{
    return FEE_SLOT_COUNT;
}

uint32 Flash_GetSlotSizeByte(void)
{
    return FEE_SLOT_SIZE_BYTE;
}

/*********************************************************************************************************************/
/*-----------------------------------------------Local Functions-----------------------------------------------------*/
/*********************************************************************************************************************/
static uint8 Fee_CalcChecksum8(const uint8 *data, uint32 length)
{
    uint32 i;
    uint8 sum = 0U;

    if (data == NULL_PTR)
    {
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        sum = (uint8)(sum + data[i]);
    }

    return (uint8)(~sum);
}

static uint32 Fee_GetSlotAddress(uint32 slotIndex)
{
    return (DFLASH_START_ADDR + (slotIndex * FEE_SLOT_SIZE_BYTE));
}

static boolean Fee_IsSequenceNewer(uint32 candidate, uint32 current)
{
    return (candidate > current) ? TRUE : FALSE;
}

static boolean Fee_IsBlankHeader(const Fee_Profile_Header_t *header)
{
    const uint8 *raw;
    uint32 i;
    boolean allFF = TRUE;
    boolean all00 = TRUE;

    if (header == NULL_PTR)
    {
        return FALSE;
    }

    raw = (const uint8 *)header;

    for (i = 0U; i < (uint32)sizeof(Fee_Profile_Header_t); i++)
    {
        if (raw[i] != 0xFFU)
        {
            allFF = FALSE;
        }

        if (raw[i] != 0x00U)
        {
            all00 = FALSE;
        }
    }

    return ((allFF == TRUE) || (all00 == TRUE)) ? TRUE : FALSE;
}

static Fee_Status_t Fee_ReadHeader(uint32 slotIndex, Fee_Profile_Header_t *outHeader)
{
    const Fee_Profile_Header_t *headerPtr;

    if ((slotIndex >= FEE_SLOT_COUNT) || (outHeader == NULL_PTR))
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    headerPtr   = (const Fee_Profile_Header_t *)Fee_GetSlotAddress(slotIndex);
    *outHeader  = *headerPtr;

    if (Fee_IsBlankHeader(outHeader) == TRUE)
    {
        return FEE_STATUS_EMPTY;
    }

    return FEE_STATUS_OK;
}

static Fee_Status_t Fee_ValidateSlot(uint32 slotIndex,
                                     Fee_Profile_Header_t *outHeader,
                                     Shared_Profile_Table_t *outTable)
{
    Fee_Profile_Header_t header;
    Fee_Status_t status;
    const uint8 *payloadPtr;
    uint8 checksum;

    if ((outHeader == NULL_PTR) || (outTable == NULL_PTR))
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    status = Fee_ReadHeader(slotIndex, &header);
    if (status != FEE_STATUS_OK)
    {
        return status;
    }

    if (header.magic != FEE_PROFILE_MAGIC)
    {
        return FEE_STATUS_HEADER_ERROR;
    }

    if (header.version != FEE_PROFILE_VERSION)
    {
        return FEE_STATUS_HEADER_ERROR;
    }

    payloadPtr = (const uint8 *)(Fee_GetSlotAddress(slotIndex) + sizeof(Fee_Profile_Header_t));
    (void)memcpy(outTable, payloadPtr, sizeof(Shared_Profile_Table_t));

    checksum = Fee_CalcChecksum8((const uint8 *)outTable, sizeof(Shared_Profile_Table_t));
    if (checksum != header.checksum)
    {
        return FEE_STATUS_CHECKSUM_ERROR;
    }

    *outHeader = header;
    return FEE_STATUS_OK;
}

static Fee_Status_t Fee_FindLatestValidSlot(uint32 *outSlot,
                                            Fee_Profile_Header_t *outHeader,
                                            Shared_Profile_Table_t *outTable)
{
    uint32 slot;
    boolean foundValid = FALSE;
    boolean sawEmpty = FALSE;
    uint32 bestSlot = 0U;
    Fee_Profile_Header_t bestHeader;
    Shared_Profile_Table_t bestTable;

    Fee_Profile_Header_t tempHeader;
    Shared_Profile_Table_t tempTable;
    Fee_Status_t status;

    if ((outSlot == NULL_PTR) || (outHeader == NULL_PTR) || (outTable == NULL_PTR))
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    for (slot = 0U; slot < FEE_SLOT_COUNT; slot++)
    {
        status = Fee_ValidateSlot(slot, &tempHeader, &tempTable);

        if (status == FEE_STATUS_OK)
        {
            if ((foundValid == FALSE) ||
                (Fee_IsSequenceNewer(tempHeader.sequence, bestHeader.sequence) == TRUE))
            {
                foundValid = TRUE;
                bestSlot   = slot;
                bestHeader = tempHeader;
                bestTable  = tempTable;
            }
        }
        else if (status == FEE_STATUS_EMPTY)
        {
            sawEmpty = TRUE;
        }
        else
        {
            /* invalid slot은 skip */
        }
    }

    if (foundValid == TRUE)
    {
        *outSlot   = bestSlot;
        *outHeader = bestHeader;
        *outTable  = bestTable;
        return FEE_STATUS_OK;
    }

    return (sawEmpty == TRUE) ? FEE_STATUS_EMPTY : FEE_STATUS_NOT_FOUND;
}

static Fee_Status_t Fee_FindFirstEmptySlot(uint32 *outSlot)
{
    uint32 slot;
    Fee_Profile_Header_t header;
    Fee_Status_t status;

    if (outSlot == NULL_PTR)
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    for (slot = 0U; slot < FEE_SLOT_COUNT; slot++)
    {
        status = Fee_ReadHeader(slot, &header);
        if (status == FEE_STATUS_EMPTY)
        {
            *outSlot = slot;
            return FEE_STATUS_OK;
        }
    }

    return FEE_STATUS_NOT_FOUND;
}

static Fee_Status_t Fee_EraseStorage(void)
{
    uint16 endInitPassword;

    endInitPassword = IfxScuWdt_getSafetyWatchdogPassword();

    IfxScuWdt_clearSafetyEndinit(endInitPassword);
    IfxFlash_eraseMultipleSectors(DFLASH_START_ADDR, FEE_ERASE_SECTOR_COUNT);
    IfxScuWdt_setSafetyEndinit(endInitPassword);

    IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0);

    return FEE_STATUS_OK;
}

static Fee_Status_t Fee_WriteSlot(uint32 slotIndex,
                                  const Shared_Profile_Table_t *inTable,
                                  uint32 sequence)
{
    uint8 buffer[FEE_SLOT_SIZE_BYTE];
    const uint32 *wordPtr;
    uint32 page;
    uint32 pageAddr;
    uint16 endInitPassword;

    Fee_Profile_Header_t header;
    Fee_Profile_Header_t verifyHeader;
    Shared_Profile_Table_t verifyTable;
    Fee_Status_t status;

    if ((slotIndex >= FEE_SLOT_COUNT) || (inTable == NULL_PTR))
    {
        return FEE_STATUS_INVALID_PARAM;
    }

    (void)memset(buffer, 0xFF, sizeof(buffer));

    header.magic    = FEE_PROFILE_MAGIC;
    header.version  = FEE_PROFILE_VERSION;
    header.checksum = Fee_CalcChecksum8((const uint8 *)inTable, sizeof(Shared_Profile_Table_t));
    header.sequence = sequence;

    (void)memcpy(&buffer[0], &header, sizeof(Fee_Profile_Header_t));
    (void)memcpy(&buffer[sizeof(Fee_Profile_Header_t)],
                 inTable,
                 sizeof(Shared_Profile_Table_t));

    wordPtr = (const uint32 *)buffer;
    endInitPassword = IfxScuWdt_getSafetyWatchdogPassword();

    for (page = 0U; page < FEE_SLOT_PAGE_COUNT; page++)
    {
        pageAddr = Fee_GetSlotAddress(slotIndex) + (page * FEE_PAGE_SIZE_BYTE);

        IfxFlash_enterPageMode(pageAddr);
        IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0);

        IfxFlash_loadPage2X32(pageAddr,
                              wordPtr[page * 2U],
                              wordPtr[(page * 2U) + 1U]);

        IfxScuWdt_clearSafetyEndinit(endInitPassword);
        IfxFlash_writePage(pageAddr);
        IfxScuWdt_setSafetyEndinit(endInitPassword);

        IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0);
    }

    status = Fee_ValidateSlot(slotIndex, &verifyHeader, &verifyTable);
    if (status != FEE_STATUS_OK)
    {
        return FEE_STATUS_VERIFY_ERROR;
    }

    if (memcmp(&verifyTable, inTable, sizeof(Shared_Profile_Table_t)) != 0)
    {
        return FEE_STATUS_VERIFY_ERROR;
    }

    return FEE_STATUS_OK;
}
