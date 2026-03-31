#include "SPI_Config.h"
#include "IfxPort.h"
#include "IfxQspi_PinMap.h"
#include "IfxScuWdt.h"

/* 전역 변수 선언 */
IfxQspi_SpiMaster_Channel spiChannel;
IfxQspi_SpiMaster spiMaster;

void init_QSPI1(void) {
    /* [1단계: 선언부] 모든 변수 선언을 함수 최상단에 배치 (C90 표준 준수) */
    uint16 safetyPwd;
    uint16 cpu0Pwd;
    IfxQspi_SpiMaster_Config spiMasterConfig;
    IfxQspi_SpiMaster_ChannelConfig spiChannelConfig;
    IfxQspi_SpiMaster_Pins pins;

    /* [2단계: 구조체 설정] 엔진을 건드리지 않고 설정값만 채웁니다 */
    pins.mrst      = &IfxQspi1_MRSTA_P10_1_IN;           // D12 핀 (P10.1)
    pins.mrstMode  = IfxPort_InputMode_pullUp;           // Pull-up으로 복구 (노이즈 방지)

    pins.mtsr      = &IfxQspi1_MTSR_P10_3_OUT;           // D11 핀 (P10.3)
    pins.mtsrMode  = IfxPort_OutputMode_pushPull;

    pins.sclk      = &IfxQspi1_SCLK_P10_2_OUT;           // D13 핀 (P10.2)
    pins.sclkMode  = IfxPort_OutputMode_pushPull;

    IfxQspi_SpiMaster_initModuleConfig(&spiMasterConfig, &MODULE_QSPI1);
    spiMasterConfig.pins = &pins;

    /* [3단계: iLLD 모듈 초기화] 이 함수들이 레지스터를 초기화합니다 */
    IfxQspi_SpiMaster_initModule(&spiMaster, &spiMasterConfig);

    IfxQspi_SpiMaster_initChannelConfig(&spiChannelConfig, &spiMaster);
    spiChannelConfig.ch.baudrate = 1000000;            // 1MHz
    IfxQspi_SpiMaster_initChannel(&spiChannel, &spiChannelConfig);

    /* [4단계:  - 시동 걸기] 모든 초기화 후 마지막에 엔진을 깨웁니다
    safetyPwd = IfxScuWdt_getSafetyWatchdogPassword();
    cpu0Pwd = IfxScuWdt_getCpuWatchdogPassword();

    // 자물쇠를 풀고 CLC를 0으로 고정 (0x08 -> 0x00)
    IfxScuWdt_clearSafetyEndinit(safetyPwd);
    IfxScuWdt_clearCpuEndinit(cpu0Pwd);

    MODULE_QSPI1.CLC.U = 0x00000000;

    IfxScuWdt_setSafetyEndinit(safetyPwd);
    IfxScuWdt_setCpuEndinit(cpu0Pwd);

    /* 엔진이 완전히 깨어날 때까지(DISS=0) 대기
    while (MODULE_QSPI1.CLC.B.DISS != 0);*/

    /* [5단계: 수동 GPIO 설정] CS와 RST를 제어합니다 */
    // CS (D10 -> P10.5)
    IfxPort_setPinModeOutput(&MODULE_P10, 5, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed1);
    IfxPort_setPinHigh(&MODULE_P10, 5);

    // RST (D9 -> P02.7)
    IfxPort_setPinModeOutput(&MODULE_P02, 7, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed1);
    IfxPort_setPinHigh(&MODULE_P02, 7);
}
