#ifndef BASE_FEE_H_
#define BASE_FEE_H_

#include "Ifx_Types.h"
#include "Shared_Profile.h"

#define DFLASH_START_ADDR            (0xAF000000U)
#define FEE_PAGE_SIZE_BYTE           (8U)
#define FEE_ERASE_SECTOR_COUNT       (1U)

#define FEE_SLOT_COUNT               (8U)

#define FEE_PROFILE_MAGIC            (0x55AAU)
#define FEE_PROFILE_VERSION          (0x01U)

typedef enum
{
    FEE_STATUS_OK = 0,
    FEE_STATUS_INVALID_PARAM,
    FEE_STATUS_EMPTY,
    FEE_STATUS_NOT_FOUND,
    FEE_STATUS_HEADER_ERROR,
    FEE_STATUS_CHECKSUM_ERROR,
    FEE_STATUS_WRITE_ERROR,
    FEE_STATUS_VERIFY_ERROR
} Fee_Status_t;

/*
 * 8B header
 * - magic   : 빈 flash / garbage 판별
 * - version : 구조 버전
 * - checksum: payload checksum8
 * - sequence: 최신 slot 판별용
 */
typedef struct
{
    uint16 magic;
    uint8  version;
    uint8  checksum;
    uint32 sequence;
} Fee_Profile_Header_t;


#define FEE_ALIGN_UP(x, a) \
    ((((uint32)(x)) + ((uint32)(a) - 1U)) / (uint32)(a) * (uint32)(a))

#define FEE_RECORD_RAW_SIZE_BYTE \
    ((uint32)(sizeof(Fee_Profile_Header_t) + sizeof(Shared_Profile_Table_t)))

#define FEE_SLOT_SIZE_BYTE \
    (FEE_ALIGN_UP(FEE_RECORD_RAW_SIZE_BYTE, FEE_PAGE_SIZE_BYTE))

#define FEE_SLOT_PAGE_COUNT \
    (FEE_SLOT_SIZE_BYTE / FEE_PAGE_SIZE_BYTE)


Fee_Status_t Flash_LoadProfileTable(Shared_Profile_Table_t *outTable);
Fee_Status_t Flash_SaveProfileTable(const Shared_Profile_Table_t *inTable);
Fee_Status_t Flash_FormatStorage(void);

uint32       Flash_GetSlotCount(void);
uint32       Flash_GetSlotSizeByte(void);

#endif /* BASE_FEE_H_ */
