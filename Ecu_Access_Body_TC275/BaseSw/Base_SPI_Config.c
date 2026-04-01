#include "Base_SPI_Config.h"
#include "IfxPort.h"
#include "IfxQspi_PinMap.h"
#include "IfxScuWdt.h"

IfxQspi_SpiMaster_Channel spiChannel;
IfxQspi_SpiMaster spiMaster;

void init_QSPI1(void) {
    uint16 safetyPwd = IfxScuWdt_getSafetyWatchdogPassword();
    uint16 cpu0Pwd = IfxScuWdt_getCpuWatchdogPassword();
    IfxQspi_SpiMaster_Config spiMasterConfig;
    IfxQspi_SpiMaster_ChannelConfig spiChannelConfig;
    IfxQspi_SpiMaster_Pins pins;

    /* 엔진 시동 (CLC 설정) */
    IfxScuWdt_clearSafetyEndinit(safetyPwd);
    IfxScuWdt_clearCpuEndinit(cpu0Pwd);
    MODULE_QSPI1.CLC.U = 0x00000000;
    IfxScuWdt_setSafetyEndinit(safetyPwd);
    IfxScuWdt_setCpuEndinit(cpu0Pwd);
    while (MODULE_QSPI1.CLC.B.DISS != 0);

    /* 핀 설정 */
    pins.mrst      = &IfxQspi1_MRSTA_P10_1_IN;
    pins.mrstMode  = IfxPort_InputMode_pullUp;
    pins.mtsr      = &IfxQspi1_MTSR_P10_3_OUT;
    pins.mtsrMode  = IfxPort_OutputMode_pushPull;
    pins.sclk      = &IfxQspi1_SCLK_P10_2_OUT;
    pins.sclkMode  = IfxPort_OutputMode_pushPull;

    /* 모듈 초기화 */
    IfxQspi_SpiMaster_initModuleConfig(&spiMasterConfig, &MODULE_QSPI1);

    spiMasterConfig.pins = &pins;
    IfxQspi_SpiMaster_initModule(&spiMaster, &spiMasterConfig);

    /* 채널 초기화 */
    IfxQspi_SpiMaster_initChannelConfig(&spiChannelConfig, &spiMaster);
    spiChannelConfig.ch.baudrate = 100000;

    MODULE_QSPI1.ECON[spiChannel.channelId].B.CPOL = 0; // 클럭 평상시 0V (Idle Low)
    MODULE_QSPI1.ECON[spiChannel.channelId].B.CPH = 0; // 첫 번째 엣지에서 데이터 캡처

    IfxQspi_SpiMaster_initChannel(&spiChannel, &spiChannelConfig);
    MODULE_QSPI1.ECON[0].B.CPOL = 0; MODULE_QSPI1.ECON[0].B.CPH = 0;
    MODULE_QSPI1.ECON[1].B.CPOL = 0; MODULE_QSPI1.ECON[1].B.CPH = 0;
    MODULE_QSPI1.ECON[2].B.CPOL = 0; MODULE_QSPI1.ECON[2].B.CPH = 0;
    MODULE_QSPI1.ECON[3].B.CPOL = 0; MODULE_QSPI1.ECON[3].B.CPH = 0;

    /* 수동 GPIO (CS: D10, RST: D9) */
    IfxPort_setPinModeOutput(&MODULE_P10, 5, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed1);
    IfxPort_setPinHigh(&MODULE_P10, 5);
    IfxPort_setPinModeOutput(&MODULE_P02, 7, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed1);
    IfxPort_setPinHigh(&MODULE_P02, 7);
}
