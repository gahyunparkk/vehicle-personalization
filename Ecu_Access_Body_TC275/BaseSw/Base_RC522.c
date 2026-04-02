/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_QSPI1.h"
#include "Base_RC522.h"
#include "Bsp.h"
#include "IfxStm.h"
#include "UART_Config.h"
/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* ---------- MFRC522 register map ---------- */
#define RC522_REG_COMMAND          0x01U
#define RC522_REG_COMIEN           0x02U
#define RC522_REG_DIVIEN           0x03U
#define RC522_REG_COMIRQ           0x04U
#define RC522_REG_DIVIRQ           0x05U
#define RC522_REG_ERROR            0x06U
#define RC522_REG_STATUS1          0x07U
#define RC522_REG_STATUS2          0x08U
#define RC522_REG_FIFODATA         0x09U
#define RC522_REG_FIFOLEVEL        0x0AU
#define RC522_REG_CONTROL          0x0CU
#define RC522_REG_BITFRAMING       0x0DU
#define RC522_REG_COLL             0x0EU

#define RC522_REG_MODE             0x11U
#define RC522_REG_TXMODE           0x12U
#define RC522_REG_RXMODE           0x13U
#define RC522_REG_TXCONTROL        0x14U
#define RC522_REG_TXASK            0x15U

#define RC522_REG_CRCRESULT_H      0x21U
#define RC522_REG_CRCRESULT_L      0x22U

#define RC522_REG_TMODE            0x2AU
#define RC522_REG_TPRESCALER       0x2BU
#define RC522_REG_TRELOAD_H        0x2CU
#define RC522_REG_TRELOAD_L        0x2DU

#define RC522_REG_VERSION          0x37U

/* ---------- MFRC522 command set ---------- */
#define RC522_CMD_IDLE             0x00U
#define RC522_CMD_CALCCRC          0x03U
#define RC522_CMD_TRANSCEIVE       0x0CU
#define RC522_CMD_MFAUTHENT        0x0EU
#define RC522_CMD_SOFTRESET        0x0FU

/* ---------- PICC commands ---------- */
#define RC522_PICC_CMD_REQA        0x26U
#define RC522_PICC_CMD_WUPA        0x52U

/* ---------- Cascade / UID ---------- */
#define RC522_SEL_CL1              0x93U
#define RC522_SEL_CL2              0x95U
#define RC522_SEL_CL3              0x97U
#define RC522_CT                   0x88U

/* ---------- Limits ---------- */
#define RC522_FIFO_XFER_MAX        64U

#ifndef RC522_WAIT_US
#define RC522_WAIT_US(us)          waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, (us)))
#endif

#ifndef RC522_WAIT_MS
#define RC522_WAIT_MS(ms)          waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, (ms)))
#endif

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
volatile uint32 g_rc522_dbg = 0;
volatile uint8 dbg_irq = 0U;
volatile uint8 dbg_error = 0U;
volatile uint8 dbg_fifo = 0U;
/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static uint8        RC522_BuildReadAddr(uint8 reg);
static uint8        RC522_BuildWriteAddr(uint8 reg);

// static Rc522_Status RC522_CalculateCrc(const uint8 *data, uint8 len, uint8 outCrc[2]);
static Rc522_Status RC522_Transceive(const uint8 *txData,
                                     uint8        txLen,
                                     uint8       *rxData,
                                     uint8       *rxLen,
                                     uint8       *rxLastBits,
                                     uint8        txLastBits,
                                     uint32       timeoutLoops);

static void         RC522_WriteFifo(const uint8 *data, uint8 len);
static void         RC522_ReadFifo(uint8 *data, uint8 len);
static void         RC522_SoftReset(void);
static void         RC522_AntennaOn(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
static uint8 RC522_BuildReadAddr(uint8 reg)
{
    return (uint8)(0x80U | ((reg << 1U) & 0x7EU));
}

static uint8 RC522_BuildWriteAddr(uint8 reg)
{
    return (uint8)((reg << 1U) & 0x7EU);
}

//static Rc522_Status RC522_CalculateCrc(const uint8 *data, uint8 len, uint8 outCrc[2])
//{
//    uint32 loop;
//
//    if ((data == NULL_PTR) || (len == 0U) || (outCrc == NULL_PTR))
//    {
//        return RC522_STATUS_ERROR;
//    }
//
//    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
//    RC522_WriteReg(RC522_REG_DIVIRQ, 0x04U);
//    RC522_WriteReg(RC522_REG_FIFOLEVEL, 0x80U);
//
//    RC522_WriteFifo(data, len);
//    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_CALCCRC);
//
//    for (loop = 0U; loop < 5000U; ++loop)
//    {
//        if ((RC522_ReadReg(RC522_REG_DIVIRQ) & 0x04U) != 0U)
//        {
//            outCrc[0] = RC522_ReadReg(RC522_REG_CRCRESULT_L);
//            outCrc[1] = RC522_ReadReg(RC522_REG_CRCRESULT_H);
//
//            RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
//            return RC522_STATUS_OK;
//        }
//    }
//
//    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
//    return RC522_STATUS_TIMEOUT;
//}

Rc522_Status RC522_ReadUid(Rc522_Uid *outUid)
{
    Rc522_Status st;
    uint8        txBuf[2];
    uint8        rxBuf[5];
    uint8        rxLen;
    uint8        rxLastBits;
    uint8        bcc;
    uint8        atqa[2]; // 응답 여부 및 UID 크기 정보 전달

    // 아직 선택되지 않은 Type A 카드가 있는지 polling
    // IDLE 상태 카드에 대한 호출

    if (outUid == NULL_PTR)
    {
        return RC522_STATUS_ERROR;
    }

    outUid->size   = 0U;
    outUid->sak    = 0U;
    outUid->uid[0] = 0U;
    outUid->uid[1] = 0U;
    outUid->uid[2] = 0U;
    outUid->uid[3] = 0U;

    st = RC522_RequestA(atqa);
    if (st != RC522_STATUS_OK)
    {
        return st;
    }

    RC522_ClearBitMask(RC522_REG_COLL, 0x80U);

    txBuf[0] = 0x93U;
    txBuf[1] = 0x20U;

    rxLen      = 0U;
    rxLastBits = 0U;

    st = RC522_Transceive(txBuf, 2U, rxBuf, &rxLen, &rxLastBits, 0U, 5000U);
    if (st != RC522_STATUS_OK)
    {
        return st;
    }

    if ((rxLen != 5U) || (rxLastBits != 0U))
    {
        return RC522_STATUS_ERROR;
    }

    bcc = (uint8)(rxBuf[0] ^ rxBuf[1] ^ rxBuf[2] ^ rxBuf[3]);
    if (bcc != rxBuf[4])
    {
        return RC522_STATUS_ERROR;
    }

    /* 0x88이면 cascade tag라서 이 최소 버전에서는 미지원 */
    if (rxBuf[0] == RC522_CT)
    {
        return RC522_STATUS_ERROR;
    }

    outUid->uid[0] = rxBuf[0];
    outUid->uid[1] = rxBuf[1];
    outUid->uid[2] = rxBuf[2];
    outUid->uid[3] = rxBuf[3];
    outUid->size   = 4U;
    outUid->sak    = 0U;

    // 아마 명시적으로 HALT 상태로 돌리는 것을 안했기 때문에 일종의 초기화처럼 동작?
    (void)RC522_RequestA(atqa); // dummy task

    return RC522_STATUS_OK;
}


uint8 RC522_ReadReg(uint8 reg)
{
    uint8 tx[2];
    uint8 rx[2];

    tx[0] = RC522_BuildReadAddr(reg);
    tx[1] = 0x00U;

    rx[0] = 0U;
    rx[1] = 0U;

    QSPI1_Transfer(tx, rx, 2U);

    return rx[1];
}

void RC522_WriteReg(uint8 reg, uint8 value)
{
    uint8 tx[2];
    uint8 rx[2];

    tx[0] = RC522_BuildWriteAddr(reg);
    tx[1] = value;

    rx[0] = 0U;
    rx[1] = 0U;

    QSPI1_Transfer(tx, rx, 2U);
}

void RC522_SetBitMask(uint8 reg, uint8 mask)
{
    uint8 value;

    value = RC522_ReadReg(reg);
    RC522_WriteReg(reg, (uint8)(value | mask));
}

void RC522_ClearBitMask(uint8 reg, uint8 mask)
{
    uint8 value;

    value = RC522_ReadReg(reg);
    RC522_WriteReg(reg, (uint8)(value & (uint8)(~mask)));
}

static void RC522_WriteFifo(const uint8 *data, uint8 len)
{
    uint8 tx[1U + RC522_FIFO_XFER_MAX];
    uint8 rx[1U + RC522_FIFO_XFER_MAX];
    uint8 i;

    if ((data == NULL_PTR) || (len == 0U) || (len > RC522_FIFO_XFER_MAX))
    {
        return;
    }

    tx[0] = RC522_BuildWriteAddr(RC522_REG_FIFODATA);
    rx[0] = 0U;

    for (i = 0U; i < len; ++i)
    {
        tx[1U + i] = data[i];
        rx[1U + i] = 0U;
    }

    QSPI1_Transfer(tx, rx, (uint32)len + 1U);
}

static void RC522_ReadFifo(uint8 *data, uint8 len)
{
    uint8 i;

    if ((data == NULL_PTR) || (len == 0U) || (len > RC522_FIFO_XFER_MAX))
    {
        return;
    }

    for (i = 0U; i < len; ++i)
    {
        data[i] = RC522_ReadReg(RC522_REG_FIFODATA);
    }
}

static void RC522_SoftReset(void)
{
    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_SOFTRESET);
    RC522_WAIT_MS(2);
}

static void RC522_AntennaOn(void)
{
    uint8 value;

    value = RC522_ReadReg(RC522_REG_TXCONTROL);

    if ((value & 0x03U) != 0x03U)
    {
        RC522_WriteReg(RC522_REG_TXCONTROL, (uint8)(value | 0x03U));
    }
}

static Rc522_Status RC522_Transceive(const uint8 *txData,
                                     uint8        txLen,
                                     uint8       *rxData,
                                     uint8       *rxLen,
                                     uint8       *rxLastBits,
                                     uint8        txLastBits,
                                     uint32       timeoutLoops)
{
    uint32 loop;
    uint8  irq;
    uint8  error;
    uint8  fifoLevel;
    uint8  lastBits;

    if ((txData == NULL_PTR) || (txLen == 0U))
    {
        return RC522_STATUS_ERROR;
    }

    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteReg(RC522_REG_COMIRQ, 0x7FU);          /* clear all IRQ */
    RC522_WriteReg(RC522_REG_FIFOLEVEL, 0x80U);       /* flush FIFO */
    RC522_WriteReg(RC522_REG_BITFRAMING, (uint8)(txLastBits & 0x07U));

    RC522_WriteFifo(txData, txLen);

    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    RC522_WriteReg(RC522_REG_BITFRAMING, (uint8)(0x80U | (txLastBits & 0x07U))); /* StartSend */

    for (loop = 0U; loop < timeoutLoops; ++loop)
    {
        irq = RC522_ReadReg(RC522_REG_COMIRQ);

        if ((irq & 0x01U) != 0U)   /* TimerIRq */
        {
            RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
            RC522_WriteReg(RC522_REG_BITFRAMING, 0x00U);
            return RC522_STATUS_TIMEOUT;
        }

        if ((irq & 0x20U) != 0U)   /* RxIRq */
        {
            break;
        }

        if ((irq & 0x02U) != 0U)   /* ErrIrq */
        {
            break;
        }

        if ((irq & 0x10U) != 0U)   /* IdleIrq */
        {
            break;
        }
    }

    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteReg(RC522_REG_BITFRAMING, 0x00U);

    if (loop >= timeoutLoops)
    {
        return RC522_STATUS_TIMEOUT;
    }

    error = RC522_ReadReg(RC522_REG_ERROR);
    if ((error & 0x1BU) != 0U)
    {
        return RC522_STATUS_ERROR;
    }

    fifoLevel = RC522_ReadReg(RC522_REG_FIFOLEVEL);
    lastBits  = (uint8)(RC522_ReadReg(RC522_REG_CONTROL) & 0x07U);

    if ((rxData != NULL_PTR) && (rxLen != NULL_PTR))
    {
        if (fifoLevel > RC522_FIFO_XFER_MAX)
        {
            return RC522_STATUS_ERROR;
        }

        *rxLen = fifoLevel;

        if (fifoLevel > 0U)
        {
            RC522_ReadFifo(rxData, fifoLevel);
        }
    }

    if (rxLastBits != NULL_PTR)
    {
        *rxLastBits = lastBits;
    }

    return RC522_STATUS_OK;
}

boolean RC522_Init(void)
{
    RC522_SoftReset();

    RC522_WriteReg(RC522_REG_COMIEN, 0x00U);
    RC522_WriteReg(RC522_REG_DIVIEN, 0x00U);

    RC522_WriteReg(RC522_REG_TXMODE, 0x00U);
    RC522_WriteReg(RC522_REG_RXMODE, 0x00U);

    RC522_SetBitMask(RC522_REG_TXASK, 0x40U);
    RC522_AntennaOn();

    return TRUE;
}

uint8 RC522_ReadVersion(void)
{
    return RC522_ReadReg(RC522_REG_VERSION);
}

Rc522_Status RC522_RequestA(uint8 atqa[2])
{
    uint8        reqa;
    uint8        rxLen;
    uint8        rxLastBits;
    Rc522_Status st;

    if (atqa == NULL_PTR)
    {
        return RC522_STATUS_ERROR;
    }

    reqa       = RC522_PICC_CMD_REQA;
    rxLen      = 0U;
    rxLastBits = 0U;

    st = RC522_Transceive(&reqa, 1U, atqa, &rxLen, &rxLastBits, 7U, 5000U);
    if (st != RC522_STATUS_OK)
    {
        return st;
    }

    if (rxLen != 2U)
    {
        return RC522_STATUS_NO_TAG;
    }

    if (rxLastBits != 0U)
    {
        return RC522_STATUS_ERROR;
    }

    return RC522_STATUS_OK;
}

boolean RC522_IsNewCardPresent(void)
{
    uint8 atqa[2];

    return (RC522_RequestA(atqa) == RC522_STATUS_OK) ? TRUE : FALSE;
}

Rc522_Status RC522_WakeupA(uint8 atqa[2])
{
    uint8        wupa;
    uint8        rxLen;
    uint8        rxLastBits;
    Rc522_Status st;

    if (atqa == NULL_PTR)
    {
        return RC522_STATUS_ERROR;
    }

    wupa       = 0x52U;
    rxLen      = 0U;
    rxLastBits = 0U;

    st = RC522_Transceive(&wupa, 1U, atqa, &rxLen, &rxLastBits, 7U, 5000U);
    if (st != RC522_STATUS_OK)
    {
        return st;
    }

    if (rxLen != 2U)
    {
        return RC522_STATUS_NO_TAG;
    }

    if (rxLastBits != 0U)
    {
        return RC522_STATUS_ERROR;
    }

    return RC522_STATUS_OK;
}
