#include "RC522_Driver.h"
#include "SPI_Config.h"
#include "IfxQspi_SpiMaster.h"
#include "IfxPort.h"

#define RC522_CS_LOW   IfxPort_setPinLow(&MODULE_P10, 5)
#define RC522_CS_HIGH  IfxPort_setPinHigh(&MODULE_P10, 5)

IfxQspi_Status status_s;

void Write_RC522(uint8 addr, uint8 val) {
    uint8 data[2];
    data[0] = (addr << 1) & 0x7E;
    data[1] = val;

    RC522_CS_LOW;
    IfxQspi_SpiMaster_exchange(&spiChannel, data, 0, 2);

    /* [추가] 전송이 끝날 때까지 기다려야 합니다! */

    status_s = IfxQspi_SpiMaster_getStatus(&spiChannel);
    while(status_s == IfxQspi_Status_busy) {
        status_s = IfxQspi_SpiMaster_getStatus(&spiChannel);
    }

    RC522_CS_HIGH;
}

uint8 Read_RC522(uint8 addr) {
    uint8 tx[2], rx[2];
    tx[0] = ((addr << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    RC522_CS_LOW;
    IfxQspi_SpiMaster_exchange(&spiChannel, tx, rx, 2);

    /* [추가] 전송이 끝날 때까지 기다려야 합니다! */
    while(IfxQspi_SpiMaster_getStatus(&spiChannel) == IfxQspi_Status_busy);

    RC522_CS_HIGH;
    return rx[1];
}

void RC522_Init(void) {
    Write_RC522(0x01, 0x0F); // Soft Reset
    for(volatile int i=0; i<10000; i++); // 리셋 대기용 짧은 딜레이

    Write_RC522(0x14, 0x11); // TModeReg
    Write_RC522(0x11, 0x3D); // Antenna On
}

uint8 RC522_Request(uint8 reqMode, uint8 *TagType) {
    // 테스트용: 레지스터 하나를 읽어 응답이 오는지 확인
    uint8 val = Read_RC522(0x01);
    return (val != 0xFF) ? STATUS_OK : 0x01;
}

uint8 RC522_Anticoll(uint8 *serNum) {
    for(int i=0; i<5; i++) serNum[i] = 0xAA; // 가짜 데이터
    return STATUS_OK;
}
