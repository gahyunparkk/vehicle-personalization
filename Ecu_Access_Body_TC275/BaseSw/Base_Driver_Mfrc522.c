/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Com_Qspi1.h"
#include "Base_Driver_Mfrc522.h"
#include "Bsp.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* ---------- MFRC522 register map ---------- */
#define MFRC522_REG_COMMAND          0x01U
#define MFRC522_REG_COMIEN           0x02U
#define MFRC522_REG_DIVIEN           0x03U
#define MFRC522_REG_COMIRQ           0x04U
#define MFRC522_REG_DIVIRQ           0x05U
#define MFRC522_REG_ERROR            0x06U
#define MFRC522_REG_STATUS1          0x07U
#define MFRC522_REG_STATUS2          0x08U
#define MFRC522_REG_FIFODATA         0x09U
#define MFRC522_REG_FIFOLEVEL        0x0AU
#define MFRC522_REG_CONTROL          0x0CU
#define MFRC522_REG_BITFRAMING       0x0DU
#define MFRC522_REG_COLL             0x0EU

#define MFRC522_REG_MODE             0x11U
#define MFRC522_REG_TXMODE           0x12U
#define MFRC522_REG_RXMODE           0x13U
#define MFRC522_REG_TXCONTROL        0x14U
#define MFRC522_REG_TXASK            0x15U

#define MFRC522_REG_CRCRESULT_H      0x21U
#define MFRC522_REG_CRCRESULT_L      0x22U

#define MFRC522_REG_TMODE            0x2AU
#define MFRC522_REG_TPRESCALER       0x2BU
#define MFRC522_REG_TRELOAD_H        0x2CU
#define MFRC522_REG_TRELOAD_L        0x2DU

#define MFRC522_REG_VERSION          0x37U

/* ---------- MFRC522 command set ---------- */
#define MFRC522_CMD_IDLE             0x00U
#define MFRC522_CMD_CALCCRC          0x03U
#define MFRC522_CMD_TRANSCEIVE       0x0CU
#define MFRC522_CMD_MFAUTHENT        0x0EU
#define MFRC522_CMD_SOFTRESET        0x0FU

/* ---------- PICC commands ---------- */
#define MFRC522_PICC_CMD_REQA        0x26U
#define MFRC522_PICC_CMD_WUPA        0x52U

/* ---------- Cascade / UID ---------- */
#define MFRC522_SEL_CL1              0x93U
#define MFRC522_SEL_CL2              0x95U
#define MFRC522_SEL_CL3              0x97U
#define MFRC522_CT                   0x88U

/* ---------- Limits ---------- */
#define MFRC522_FIFO_XFER_MAX        64U

#ifndef MFRC522_WAIT_US
#define MFRC522_WAIT_US(us)          waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, (us)))
#endif

#ifndef MFRC522_WAIT_MS
#define MFRC522_WAIT_MS(ms)          waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, (ms)))
#endif

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
volatile uint32 g_mfrc522_dbg       = 0U;
volatile uint8  g_mfrc522_dbg_irq   = 0U;
volatile uint8  g_mfrc522_dbg_error = 0U;
volatile uint8  g_mfrc522_dbg_fifo  = 0U;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static uint8          Base_Driver_Mfrc522_BuildReadAddr(uint8 reg);
static uint8          Base_Driver_Mfrc522_BuildWriteAddr(uint8 reg);

static Mfrc522_Status Base_Driver_Mfrc522_Transceive(const uint8 *txData,
                                                     uint8         txLen,
                                                     uint8        *rxData,
                                                     uint8        *rxLen,
                                                     uint8        *rxLastBits,
                                                     uint8         txLastBits,
                                                     uint32        timeoutLoops);

static void           Base_Driver_Mfrc522_WriteFifo(const uint8 *data, uint8 len);
static void           Base_Driver_Mfrc522_ReadFifo(uint8 *data, uint8 len);
static void           Base_Driver_Mfrc522_SoftReset(void);
static void           Base_Driver_Mfrc522_AntennaOn(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
static uint8 Base_Driver_Mfrc522_BuildReadAddr(uint8 reg)
{
    return (uint8)(0x80U | ((reg << 1U) & 0x7EU));
}

static uint8 Base_Driver_Mfrc522_BuildWriteAddr(uint8 reg)
{
    return (uint8)((reg << 1U) & 0x7EU);
}

Mfrc522_Status Base_Driver_Mfrc522_ReadUid(Mfrc522_Uid *outUid)
{
    Mfrc522_Status st;
    uint8          txBuf[2];
    uint8          rxBuf[5];
    uint8          rxLen;
    uint8          rxLastBits;
    uint8          bcc;
    uint8          atqa[2];

    if (outUid == NULL_PTR)
    {
        return MFRC522_STATUS_ERROR;
    }

    outUid->size   = 0U;
    outUid->uid[0] = 0U;
    outUid->uid[1] = 0U;
    outUid->uid[2] = 0U;
    outUid->uid[3] = 0U;

    st = Base_Driver_Mfrc522_RequestA(atqa);
    if (st != MFRC522_STATUS_OK)
    {
        return st;
    }

    Base_Driver_Mfrc522_ClearBitMask(MFRC522_REG_COLL, 0x80U);

    txBuf[0] = MFRC522_SEL_CL1;
    txBuf[1] = 0x20U;

    rxLen      = 0U;
    rxLastBits = 0U;

    st = Base_Driver_Mfrc522_Transceive(txBuf, 2U, rxBuf, &rxLen, &rxLastBits, 0U, 5000U);
    if (st != MFRC522_STATUS_OK)
    {
        return st;
    }

    if ((rxLen != 5U) || (rxLastBits != 0U))
    {
        return MFRC522_STATUS_ERROR;
    }

    bcc = (uint8)(rxBuf[0] ^ rxBuf[1] ^ rxBuf[2] ^ rxBuf[3]);
    if (bcc != rxBuf[4])
    {
        return MFRC522_STATUS_ERROR;
    }

    if (rxBuf[0] == MFRC522_CT)
    {
        return MFRC522_STATUS_ERROR;
    }

    outUid->uid[0] = rxBuf[0];
    outUid->uid[1] = rxBuf[1];
    outUid->uid[2] = rxBuf[2];
    outUid->uid[3] = rxBuf[3];
    outUid->size   = 4U;

    (void)Base_Driver_Mfrc522_RequestA(atqa);

    return MFRC522_STATUS_OK;
}

uint8 Base_Driver_Mfrc522_ReadReg(uint8 reg)
{
    uint8 tx[2];
    uint8 rx[2];

    tx[0] = Base_Driver_Mfrc522_BuildReadAddr(reg);
    tx[1] = 0x00U;

    rx[0] = 0U;
    rx[1] = 0U;

    Base_Com_Qspi1_Transfer(tx, rx, 2U);

    return rx[1];
}

void Base_Driver_Mfrc522_WriteReg(uint8 reg, uint8 value)
{
    uint8 tx[2];
    uint8 rx[2];

    tx[0] = Base_Driver_Mfrc522_BuildWriteAddr(reg);
    tx[1] = value;

    rx[0] = 0U;
    rx[1] = 0U;

    Base_Com_Qspi1_Transfer(tx, rx, 2U);
}

void Base_Driver_Mfrc522_SetBitMask(uint8 reg, uint8 mask)
{
    uint8 value;

    value = Base_Driver_Mfrc522_ReadReg(reg);
    Base_Driver_Mfrc522_WriteReg(reg, (uint8)(value | mask));
}

void Base_Driver_Mfrc522_ClearBitMask(uint8 reg, uint8 mask)
{
    uint8 value;

    value = Base_Driver_Mfrc522_ReadReg(reg);
    Base_Driver_Mfrc522_WriteReg(reg, (uint8)(value & (uint8)(~mask)));
}

static void Base_Driver_Mfrc522_WriteFifo(const uint8 *data, uint8 len)
{
    uint8 tx[1U + MFRC522_FIFO_XFER_MAX];
    uint8 rx[1U + MFRC522_FIFO_XFER_MAX];
    uint8 i;

    if ((data == NULL_PTR) || (len == 0U) || (len > MFRC522_FIFO_XFER_MAX))
    {
        return;
    }

    tx[0] = Base_Driver_Mfrc522_BuildWriteAddr(MFRC522_REG_FIFODATA);
    rx[0] = 0U;

    for (i = 0U; i < len; ++i)
    {
        tx[1U + i] = data[i];
        rx[1U + i] = 0U;
    }

    Base_Com_Qspi1_Transfer(tx, rx, (uint32)len + 1U);
}

static void Base_Driver_Mfrc522_ReadFifo(uint8 *data, uint8 len)
{
    uint8 i;

    if ((data == NULL_PTR) || (len == 0U) || (len > MFRC522_FIFO_XFER_MAX))
    {
        return;
    }

    for (i = 0U; i < len; ++i)
    {
        data[i] = Base_Driver_Mfrc522_ReadReg(MFRC522_REG_FIFODATA);
    }
}

static void Base_Driver_Mfrc522_SoftReset(void)
{
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_SOFTRESET);
    MFRC522_WAIT_MS(2);
}

static void Base_Driver_Mfrc522_AntennaOn(void)
{
    uint8 value;

    value = Base_Driver_Mfrc522_ReadReg(MFRC522_REG_TXCONTROL);

    if ((value & 0x03U) != 0x03U)
    {
        Base_Driver_Mfrc522_WriteReg(MFRC522_REG_TXCONTROL, (uint8)(value | 0x03U));
    }
}

static Mfrc522_Status Base_Driver_Mfrc522_Transceive(const uint8 *txData,
                                                     uint8         txLen,
                                                     uint8        *rxData,
                                                     uint8        *rxLen,
                                                     uint8        *rxLastBits,
                                                     uint8         txLastBits,
                                                     uint32        timeoutLoops)
{
    uint32 loop;
    uint8  irq;
    uint8  error;
    uint8  fifoLevel;
    uint8  lastBits;

    if ((txData == NULL_PTR) || (txLen == 0U))
    {
        return MFRC522_STATUS_ERROR;
    }

    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMIRQ, 0x7FU);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_FIFOLEVEL, 0x80U);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_BITFRAMING, (uint8)(txLastBits & 0x07U));

    Base_Driver_Mfrc522_WriteFifo(txData, txLen);

    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_TRANSCEIVE);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_BITFRAMING, (uint8)(0x80U | (txLastBits & 0x07U)));

    for (loop = 0U; loop < timeoutLoops; ++loop)
    {
        irq = Base_Driver_Mfrc522_ReadReg(MFRC522_REG_COMIRQ);

        g_mfrc522_dbg_irq = irq;

        if ((irq & 0x01U) != 0U)
        {
            Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
            Base_Driver_Mfrc522_WriteReg(MFRC522_REG_BITFRAMING, 0x00U);
            return MFRC522_STATUS_TIMEOUT;
        }

        if ((irq & 0x20U) != 0U)
        {
            break;
        }

        if ((irq & 0x02U) != 0U)
        {
            break;
        }

        if ((irq & 0x10U) != 0U)
        {
            break;
        }
    }

    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_BITFRAMING, 0x00U);

    if (loop >= timeoutLoops)
    {
        return MFRC522_STATUS_TIMEOUT;
    }

    error = Base_Driver_Mfrc522_ReadReg(MFRC522_REG_ERROR);
    g_mfrc522_dbg_error = error;

    if ((error & 0x1BU) != 0U)
    {
        return MFRC522_STATUS_ERROR;
    }

    fifoLevel = Base_Driver_Mfrc522_ReadReg(MFRC522_REG_FIFOLEVEL);
    lastBits  = (uint8)(Base_Driver_Mfrc522_ReadReg(MFRC522_REG_CONTROL) & 0x07U);

    g_mfrc522_dbg_fifo = fifoLevel;

    if ((rxData != NULL_PTR) && (rxLen != NULL_PTR))
    {
        if (fifoLevel > MFRC522_FIFO_XFER_MAX)
        {
            return MFRC522_STATUS_ERROR;
        }

        *rxLen = fifoLevel;

        if (fifoLevel > 0U)
        {
            Base_Driver_Mfrc522_ReadFifo(rxData, fifoLevel);
        }
    }

    if (rxLastBits != NULL_PTR)
    {
        *rxLastBits = lastBits;
    }

    return MFRC522_STATUS_OK;
}

boolean Base_Driver_Mfrc522_Init(void)
{
    Base_Com_Qspi1_Init();
    Base_Driver_Mfrc522_SoftReset();

    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_COMIEN, 0x00U);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_DIVIEN, 0x00U);

    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_TXMODE, 0x00U);
    Base_Driver_Mfrc522_WriteReg(MFRC522_REG_RXMODE, 0x00U);

    Base_Driver_Mfrc522_SetBitMask(MFRC522_REG_TXASK, 0x40U);
    Base_Driver_Mfrc522_AntennaOn();

    return TRUE;
}

uint8 Base_Driver_Mfrc522_ReadVersion(void)
{
    return Base_Driver_Mfrc522_ReadReg(MFRC522_REG_VERSION);
}

Mfrc522_Status Base_Driver_Mfrc522_RequestA(uint8 atqa[2])
{
    uint8          reqa;
    uint8          rxLen;
    uint8          rxLastBits;
    Mfrc522_Status st;

    if (atqa == NULL_PTR)
    {
        return MFRC522_STATUS_ERROR;
    }

    reqa       = MFRC522_PICC_CMD_REQA;
    rxLen      = 0U;
    rxLastBits = 0U;

    st = Base_Driver_Mfrc522_Transceive(&reqa, 1U, atqa, &rxLen, &rxLastBits, 7U, 5000U);
    if (st != MFRC522_STATUS_OK)
    {
        return st;
    }

    if (rxLen != 2U)
    {
        return MFRC522_STATUS_NO_TAG;
    }

    if (rxLastBits != 0U)
    {
        return MFRC522_STATUS_ERROR;
    }

    return MFRC522_STATUS_OK;
}

boolean Base_Driver_Mfrc522_IsNewCardPresent(void)
{
    uint8 atqa[2];

    return (Base_Driver_Mfrc522_RequestA(atqa) == MFRC522_STATUS_OK) ? TRUE : FALSE;
}

Mfrc522_Status Base_Driver_Mfrc522_WakeupA(uint8 atqa[2])
{
    uint8          wupa;
    uint8          rxLen;
    uint8          rxLastBits;
    Mfrc522_Status st;

    if (atqa == NULL_PTR)
    {
        return MFRC522_STATUS_ERROR;
    }

    wupa       = MFRC522_PICC_CMD_WUPA;
    rxLen      = 0U;
    rxLastBits = 0U;

    st = Base_Driver_Mfrc522_Transceive(&wupa, 1U, atqa, &rxLen, &rxLastBits, 7U, 5000U);
    if (st != MFRC522_STATUS_OK)
    {
        return st;
    }

    if (rxLen != 2U)
    {
        return MFRC522_STATUS_NO_TAG;
    }

    if (rxLastBits != 0U)
    {
        return MFRC522_STATUS_ERROR;
    }

    return MFRC522_STATUS_OK;
}
