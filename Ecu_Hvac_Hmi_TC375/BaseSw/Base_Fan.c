#include "Base_Fan.h"

#include "Gtm/Tom/Pwm/IfxGtm_Tom_Pwm.h"

// PWM 제어를 위한 전역 드라이버 구조체
IfxGtm_Tom_Pwm_Driver g_tomPwmDriver;

void Fan_init(void)
{
  // 1. GTM 모듈 활성화
  Ifx_GTM *gtm = &MODULE_GTM;
  IfxGtm_enable(gtm);

  // 2. GTM CMU(Clock Management Unit) 클럭 활성화 (FXCLK 사용)
  IfxGtm_Cmu_enableClocks(gtm, IFXGTM_CMU_CLKEN_FXCLK);

  // 3. PWM 설정 구조체 선언 및 기본값 초기화
  IfxGtm_Tom_Pwm_Config g_motorPwmConfig;
  IfxGtm_Tom_Pwm_initConfig(&g_motorPwmConfig, gtm);

  // // 4. P11.2 핀 매핑 및 출력 모드 설정
  // // (주의: IfxGtm_TOM0_4_TOUT12_P11_2_OUT는 예시이며, 실제 핀맵 헤더에 정의된 P11.2 TOUT 매크로를 사용해야 합니다)
  // tomPwmConfig.pin.outputPin = &IfxGtm_TOM0_4_TOUT12_P11_2_OUT;
  // tomPwmConfig.pin.outputMode = IfxPort_OutputMode_pushPull;
  // tomPwmConfig.pin.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

  // // 5. PWM 파라미터 설정 (주기 및 듀티 사이클)
  // tomPwmConfig.clock = IfxGtm_Tom_Ch_clkSrc_cmuFxclk0; // 타이머 클럭 소스 선택
  // tomPwmConfig.period = 10000;                         // 전체 주기 (타이머 틱 기준)
  // tomPwmConfig.dutyCycle = 5000;                       // 듀티 사이클 (50% 설정)
  // tomPwmConfig.signalLevel = Ifx_ActiveState_high;     // Active High 동작

  g_motorPwmConfig.tom = IfxGtm_TOM0_9_TOUT1_P02_1_OUT.tom;
  g_motorPwmConfig.tomChannel = IfxGtm_TOM0_9_TOUT1_P02_1_OUT.channel;
  g_motorPwmConfig.pin.outputPin = &IfxGtm_TOM0_9_TOUT1_P02_1_OUT;
  g_motorPwmConfig.pin.outputMode = IfxPort_OutputMode_pushPull;
  g_motorPwmConfig.pin.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;
  g_motorPwmConfig.synchronousUpdateEnabled = TRUE;
  g_motorPwmConfig.period = 1000;
  g_motorPwmConfig.dutyCycle = 0;
  g_motorPwmConfig.clock = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk0;

  // 6. PWM 드라이버 초기화 및 출력 시작
  IfxGtm_Tom_Pwm_init(&g_tomPwmDriver, &g_motorPwmConfig);
  IfxGtm_Tom_Pwm_start(&g_tomPwmDriver, TRUE);
}

void Fan_setSpeed(uint8 speed)
{
  // TOM 모듈의 Compare 1 Shadow 레지스터(SR1)를 업데이트하여 듀티 사이클 변경
  IfxGtm_Tom_Ch_setCompareOneShadow(
      g_tomPwmDriver.tom,        // 현재 사용 중인 TOM 모듈 포인터
      g_tomPwmDriver.tomChannel, // 현재 할당된 TOM 채널 번호
      (uint32)speed * 10         // 새로 적용할 듀티 값 (타이머 틱 기준)
  );
}
