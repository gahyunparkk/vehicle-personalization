#ifndef BASE_FEE_H_
#define BASE_FEE_H_

#include "Ifx_Types.h"
#include "Shared_Profile.h"

#define DFLASH_START_ADDR               (0xAF000000U)
#define FEE_PAGE_SIZE_BYTE              (8U)
#define FEE_PROFILE_TABLE_PAGE_COUNT    (SHARED_PROFILE_TABLE_SIZE_BYTE / FEE_PAGE_SIZE_BYTE)

void Flash_LoadProfileTable(Shared_Profile_Table_t* outTable);
void Flash_SaveProfileTable(const Shared_Profile_Table_t* inTable);

#endif /* BASE_FEE_H_ */
