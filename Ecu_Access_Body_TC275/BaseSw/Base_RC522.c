#include <BaseSw/Base_RC522.h>
#include <BaseSw/Base_SPI_Config.h>
#include "IfxPort.h"
#include "IfxQspi_SpiMaster.h"

extern IfxQspi_SpiMaster spiMaster;
extern IfxQspi_SpiMaster_Channel spiChannel;

#define RC522_CS_LOW   IfxPort_setPinLow(&MODULE_P10, 5)
#define RC522_CS_HIGH  IfxPort_setPinHigh(&MODULE_P10, 5)

/* --- [에러 완벽 차단] 상태 코드 강제 정의 --- */
// STATUS_OK가 헤더에 없으면 0으로 만들고, 경고(W560) 안 뜨게 uint8로 형변환!
#ifndef STATUS_OK
#define STATUS_OK ((uint8)0)
#endif

// STATUS_ERR가 헤더에 없으면 여기서 1로 확실하게 못 박아줍니다!
#ifndef STATUS_ERR
#define STATUS_ERR ((uint8)1)
#endif

#ifndef PCD_TRANSCEIVE
#define PCD_IDLE              0x00
#define PCD_TRANSCEIVE        0x0C
#define PICC_REQIDL           0x26
#define PICC_ANTICOLL         0x93
#define MAX_LEN               16

// 레지스터 주소
#define CommandReg            0x01
#define CommIrqReg            0x04
#define ErrorReg              0x06
#define Status2Reg            0x08
#define FIFODataReg           0x09
#define FIFOLevelReg          0x0A
#define ControlReg            0x0C
#define BitFramingReg         0x0D
#define CollReg               0x0E
#endif

/* 우리가 완성한 무적의 하드웨어 통신 함수 */
void Write_RC522(uint8 addr, uint8 val) {
    uint8 data[2];
    uint8 dummyRx[2];
    data[0] = (addr << 1) & 0x7E;
    data[1] = val;

    RC522_CS_LOW;
    IfxQspi_SpiMaster_exchange(&spiChannel, data, dummyRx, 2);

    while(IfxQspi_SpiMaster_getStatus(&spiChannel) == IfxQspi_Status_busy) {
        IfxQspi_SpiMaster_isrTransmit(&spiMaster);
        IfxQspi_SpiMaster_isrReceive(&spiMaster);
        IfxQspi_SpiMaster_isrError(&spiMaster);
    }

    for(volatile int i=0; i<200; i++) {
        IfxQspi_SpiMaster_isrReceive(&spiMaster);
    }
    RC522_CS_HIGH;
}

uint8 Read_RC522(uint8 addr) {
    uint8 tx[2];
    uint8 rx[2] = {0xFF, 0xFF};

    tx[0] = ((addr << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    RC522_CS_LOW;
    IfxQspi_SpiMaster_exchange(&spiChannel, tx, rx, 2);

    while(IfxQspi_SpiMaster_getStatus(&spiChannel) == IfxQspi_Status_busy) {
        IfxQspi_SpiMaster_isrTransmit(&spiMaster);
        IfxQspi_SpiMaster_isrReceive(&spiMaster);
        IfxQspi_SpiMaster_isrError(&spiMaster);
    }

    /* 하드웨어 버퍼에서 직접 데이터 강제 추출 */
    if (((MODULE_QSPI1.STATUS.U >> 13) & 0x07) > 0) {
        rx[1] = (uint8)MODULE_QSPI1.RXEXIT.U;
    }

    RC522_CS_HIGH;
    return rx[1];
}

/* 비트 조작 보조 함수 */
void SetBitMask(uint8 reg, uint8 mask) {
    uint8 tmp = Read_RC522(reg);
    Write_RC522(reg, tmp | mask);  // 해당 비트만 1로 켬
}

void ClearBitMask(uint8 reg, uint8 mask) {
    uint8 tmp = Read_RC522(reg);
    Write_RC522(reg, tmp & (~mask)); // 해당 비트만 0으로 끔
}

/* 칩 내부 FIFO 버퍼를 이용한 핵심 통신 엔진 */
uint8 RC522_ToCard(uint8 command, uint8 *sendData, uint8 sendLen, uint8 *backData, uint16 *backLen) {
    uint8 status = STATUS_ERR;
    uint8 irqEn = 0x00;
    uint8 waitIRq = 0x00;
    uint8 lastBits;
    uint8 n;
    uint16 i;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    Write_RC522(0x02, irqEn | 0x80);    // CommIEnReg 설정
    ClearBitMask(CommIrqReg, 0x80);     // 모든 인터럽트 해제
    SetBitMask(FIFOLevelReg, 0x80);     // FIFO 버퍼 싹 비우기

    Write_RC522(CommandReg, PCD_IDLE);  // 현재 명령 중지

    // 전송할 데이터를 FIFO에 밀어넣기
    for (i = 0; i < sendLen; i++) {
        Write_RC522(FIFODataReg, sendData[i]);
    }

    // 전송 명령 실행
    Write_RC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        SetBitMask(BitFramingReg, 0x80); // StartSend=1
    }

    // 카드가 대답할 때까지 대기 (타임아웃 설정)
    i = 2000;
    do {
        n = Read_RC522(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    ClearBitMask(BitFramingReg, 0x80);   // StartSend=0

    // 에러 검출 및 데이터 수신
    if (i != 0) {
        if (!(Read_RC522(ErrorReg) & 0x1B)) { // 에러가 없다면
            status = STATUS_OK;
            if (n & irqEn & 0x01) status = STATUS_ERR;

            if (command == PCD_TRANSCEIVE) {
                n = Read_RC522(FIFOLevelReg); // FIFO에 몇 바이트 들어왔나
                lastBits = Read_RC522(ControlReg) & 0x07;
                if (lastBits) *backLen = (n - 1) * 8 + lastBits;
                else          *backLen = n * 8;

                if (n == 0) n = 1;
                if (n > MAX_LEN) n = MAX_LEN;

                // FIFO에서 진짜 카드 데이터 꺼내오기
                for (i = 0; i < n; i++) {
                    backData[i] = Read_RC522(FIFODataReg);
                }
            }
        } else {
            status = STATUS_ERR;
        }
    }
    return status;
}

/* 카드 탐지 및 UID(고유번호) 읽기 함수 */
uint8 RC522_Request(uint8 reqMode, uint8 *TagType) {
    uint8 status;
    uint16 backBits;

    Write_RC522(BitFramingReg, 0x07); // 송신 비트 설정
    TagType[0] = reqMode;             // 0x26 (PICC_REQIDL)

    status = RC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    if ((status != STATUS_OK) || (backBits != 0x10)) {
        status = STATUS_ERR;
    }
    return status;
}

uint8 RC522_Anticoll(uint8 *serNum) {
    uint8 status;
    uint8 i;
    uint8 serNumCheck = 0;
    uint16 unLen;

    ClearBitMask(Status2Reg, 0x08);
    ClearBitMask(CollReg, 0x80);
    Write_RC522(BitFramingReg, 0x00);

    // 충돌 방지 명령어 (0x93, 0x20) 발사
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;

    status = RC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == STATUS_OK) {
        // 체크섬 검사: 받은 4바이트를 XOR 연산해서 5번째 바이트와 맞는지 확인
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum[i];
        }
        if (serNumCheck != serNum[4]) {
            status = STATUS_ERR; // 데이터 깨짐
        }
    }
    return status;
}

void RC522_Init(void) {
    Write_RC522(CommandReg, 0x0F); // Soft Reset
    for(volatile int i=0; i<50000; i++);

    Write_RC522(0x2A, 0x8D); // TModeReg: 타이머 설정 (0x2A)
    Write_RC522(0x2B, 0x3E); // TPrescalerReg
    Write_RC522(0x2D, 30);   // TReloadRegL
    Write_RC522(0x2C, 0);    // TReloadRegH

    Write_RC522(0x15, 0x40); // TxASKReg: ASK 변조 강제 설정
    Write_RC522(0x11, 0x3D); // ModeReg: 통신 기본 설정

    Write_RC522(0x26, 0x70); // RFCfgReg: 수신기 게인 최대 설정

    /* 안테나 전원 켜기 ( 0x14가 안테나 방) */
    SetBitMask(0x14, 0x03);  // TxControlReg: 안테나 빔 발사
}
