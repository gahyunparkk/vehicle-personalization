#include "Base_Fee.h"
#include "IfxFlash.h"
#include "IfxScuWdt.h"

void Flash_LoadProfileTable(Shared_Profile_Table_t* outTable)
{
    const Shared_Profile_Table_t* flashData = (const Shared_Profile_Table_t*)DFLASH_START_ADDR;
    *outTable = *flashData;
}

void Flash_SaveProfileTable(const Shared_Profile_Table_t* inTable)
{
    uint16 endInitPassword;
    const uint32* dataPtr;
    uint32 pageAddr;
    uint32 page;
    uint32 totalPages;

    endInitPassword = IfxScuWdt_getSafetyWatchdogPassword();

    IfxScuWdt_clearSafetyEndinit(endInitPassword);
    IfxFlash_eraseMultipleSectors(DFLASH_START_ADDR, 1);
    IfxScuWdt_setSafetyEndinit(endInitPassword);

    IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);

    dataPtr    = (const uint32*)inTable;
    totalPages = FEE_PROFILE_TABLE_PAGE_COUNT;   /* 40B / 8B = 5 pages */

    for (page = 0U; page < totalPages; page++)
    {
        pageAddr = DFLASH_START_ADDR + (page * FEE_PAGE_SIZE_BYTE);

        IfxFlash_enterPageMode(pageAddr);
        IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);

        IfxFlash_loadPage2X32(pageAddr,
                              dataPtr[page * 2U],
                              dataPtr[(page * 2U) + 1U]);

        IfxScuWdt_clearSafetyEndinit(endInitPassword);
        IfxFlash_writePage(pageAddr);
        IfxScuWdt_setSafetyEndinit(endInitPassword);

        IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);
    }
}
