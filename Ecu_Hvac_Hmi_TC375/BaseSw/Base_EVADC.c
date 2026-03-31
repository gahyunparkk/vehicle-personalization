#include "IfxEvadc_Adc.h"

/* EVADC 핸들 구조체를 전역 변수로 선언합니다. */
IfxEvadc_Adc evadc;
IfxEvadc_Adc_Group adcGroup;
IfxEvadc_Adc_Channel adcChannel[2];

/* 두 개의 ADC 값을 저장할 변수 */
uint16 adcResult0 = 0;
uint16 adcResult1 = 0;

void init_EVADC(void)
{
  /* 1. EVADC 모듈 설정 및 초기화 */
  IfxEvadc_Adc_Config adcConfig;
  IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
  IfxEvadc_Adc_initModule(&evadc, &adcConfig);

  /* 2. EVADC 그룹 설정 (Group 0 사용) */
  IfxEvadc_Adc_GroupConfig adcGroupConfig;
  IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &evadc);
  adcGroupConfig.groupId = IfxEvadc_GroupId_8;
  adcGroupConfig.master = adcGroupConfig.groupId;

  // Gating을 항상 허용하여 큐가 막히지 않도록 합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;
  // 소프트웨어 트리거로 동작하도록 설정합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.triggerMode = IfxEvadc_TriggerMode_noExternalTrigger;

  // 큐 0을 사용하도록 설정합니다.
  adcGroupConfig.arbiter.requestSlotQueue0Enabled = TRUE;
  IfxEvadc_Adc_initGroup(&adcGroup, &adcGroupConfig);

  /* 3. EVADC 채널 설정 (Channel 0, Channel 1) */
  IfxEvadc_Adc_ChannelConfig adcChannelConfig;

  // Channel 0 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_7;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_7; // 결과를 저장할 전용 레지스터
  IfxEvadc_Adc_initChannel(&adcChannel[0], &adcChannelConfig);

  // Channel 1 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_6;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_6;
  IfxEvadc_Adc_initChannel(&adcChannel[1], &adcChannelConfig);

  /* 4. 변환 큐에 채널 추가 (자동 재충전 옵션 사용) */
  IfxEvadc_Adc_addToQueue(&adcChannel[0], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);
  IfxEvadc_Adc_addToQueue(&adcChannel[1], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);

  /* 5. 큐 변환 시작 */
  IfxEvadc_Adc_startQueue(&adcGroup, IfxEvadc_RequestSource_queue0);
}

void read_EVADC_Values(uint16 *res1, uint16 *res2)
{
  Ifx_EVADC_G_RES conversionResult;

  /* Channel 0 결과 읽기 */
  do
  {
    // 결과 레지스터에서 값을 읽어옵니다.
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel[0]);
  } while (!conversionResult.B.VF); // Valid Flag(유효성 플래그)가 설정될 때까지 대기

  *res1 = conversionResult.B.RESULT;

  /* Channel 1 결과 읽기 */
  do
  {
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel[1]);
  } while (!conversionResult.B.VF);

  *res2 = conversionResult.B.RESULT;
}
