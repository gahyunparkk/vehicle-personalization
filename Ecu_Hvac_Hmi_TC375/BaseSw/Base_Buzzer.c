#include "Base_Buzzer.h"

#include "IfxGtm_Cmu.h"
#include "IfxGtm_Tom_Pwm.h"

/* 예시 주기 및 듀티 사이클 설정 (Ticks 단위) */
#define PWM_PERIOD 50000
#define PWM_DUTY   25000 /* 50% 듀티 사이클 */

IfxGtm_Tom_Pwm_Driver g_tomDriver; /* PWM 제어를 위한 전역 드라이버 구조체 */

void initPwm_Tout107(void)
{
  /* 1. GTM 모듈 활성화 */
  IfxGtm_enable(&MODULE_GTM);

  /* 2. CMU 클럭 설정 (FXCLK 활성화) */
  IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_FXCLK);

  /* 3. TOM PWM 설정 구조체 선언 및 기본값 초기화 */
  IfxGtm_Tom_Pwm_Config tomConfig;
  IfxGtm_Tom_Pwm_initConfig(&tomConfig, &MODULE_GTM);

  /* 4. 출력 핀 설정 (TOUT107 매핑)
     참고: 사용하시는 TC375 패키지(BGA292 등)에 따라 매크로 이름의 TOM 번호 및 채널이
     다를 수 있습니다. 프로젝트의 IfxGtm_PinMap.h 파일을 확인하시어
     정확한 TOUT107 핀 매크로(예: 주로 P10.5에 매핑됨)를 적용하시기 바랍니다. */
  tomConfig.pin.outputPin = &IfxGtm_TOM0_2_TOUT107_P10_5_OUT;

  /* 핀 맵 매크로에서 해당 핀이 연결된 TOM 모듈과 채널을 자동으로 가져옵니다. */
  tomConfig.tom = tomConfig.pin.outputPin->tom;
  tomConfig.tomChannel = tomConfig.pin.outputPin->channel;

  tomConfig.pin.outputMode = IfxPort_OutputMode_pushPull;
  tomConfig.pin.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

  /* 5. 클럭 소스, 주기, 듀티 사이클 등 PWM 특성 설정 */
  tomConfig.clock = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk2; /* 기본 제공되는 FXCLK0 사용 */
  tomConfig.period = PWM_PERIOD;                    /* PWM 주기 설정 */
  tomConfig.dutyCycle = PWM_DUTY;                   /* PWM 듀티 사이클 설정 */
  tomConfig.signalLevel = Ifx_ActiveState_high;     /* Active High 모드 */
  tomConfig.synchronousUpdateEnabled = TRUE;        /* 섀도우 레지스터 동기화 업데이트 활성화 */

  /* 6. TOM PWM 초기화 및 시작 */
  IfxGtm_Tom_Pwm_init(&g_tomDriver, &tomConfig);
  IfxGtm_Tom_Pwm_start(&g_tomDriver, TRUE);
}

/**
 * @brief PWM 듀티 사이클 업데이트 함수
 * @param dutyTicks 변경할 듀티 사이클 값 (Tick 단위)
 */
void updatePwm10_Duty(uint32 dutyTicks)
{
  /* 드라이버 구조체에서 현재 설정된 TOM 모듈과 채널 정보를 가져옵니다. */
  Ifx_GTM_TOM *tom = g_tomDriver.tom;
  IfxGtm_Tom_Ch channel = g_tomDriver.tomChannel;

  /* 섀도우 레지스터(SR1)에 새로운 듀티 값을 기록합니다.
     이 값은 현재 진행 중인 PWM 주기가 끝난 직후 다음 주기부터 하드웨어적으로 자동 적용됩니다. */
  IfxGtm_Tom_Ch_setCompareOneShadow(tom, channel, dutyTicks);
}

/**
 * @brief PWM 출력 시작(ON) 함수
 */
void startPwm10(void)
{
  /* TRUE 파라미터는 즉각적인 타이머 시작을 의미합니다. */
  IfxGtm_Tom_Pwm_start(&g_tomDriver, TRUE);
}

/**
 * @brief PWM 출력 중지(OFF) 함수
 */
void stopPwm10(void)
{
  /* TRUE 파라미터는 즉각적인 타이머 중지를 의미하며, 출력 핀은 기본 비활성 상태로 돌아갑니다. */
  IfxGtm_Tom_Pwm_stop(&g_tomDriver, TRUE);
}

/**
 * @brief [선택 사항] 타이머를 끄지 않고 듀티만 0%로 만들어 출력을 끄는 함수
 * (하드웨어 타이머를 계속 구동해야 하는 특수한 상황에서 유용합니다.)
 */
void turnOffPwm10_Soft(void)
{
  /* 듀티를 0으로 설정하여 로우(Low) 상태를 유지시킵니다. */
  updatePwm10_Duty(0);
}

/**
 * @brief 주파수(Hz)와 듀티비(%)를 입력받아 PWM을 업데이트하는 Wrapper 함수 (자동 클럭 보정)
 * @param freqHz       설정할 PWM 주파수 (단위: Hz)
 * @param dutyPercent  설정할 듀티 사이클 (단위: %, 0.0 ~ 100.0)
 */
void setPwm10_FreqAndDuty(uint32 freqHz, float32 dutyPercent)
{
  if (freqHz == 0)
  {
    freqHz = 1;
  }
  if (dutyPercent > 100.0f)
  {
    dutyPercent = 100.0f;
  }
  else if (dutyPercent < 0.0f)
  {
    dutyPercent = 0.0f;
  }

  /* 1. 클럭 주파수 읽기 대상을 Fxclk_2로 변경 */
  float32 currentFxClkFreq = IfxGtm_Cmu_getFxClkFrequency(&MODULE_GTM, IfxGtm_Cmu_Fxclk_2, TRUE);

  /* 2. 하드웨어 Tick 단위 변환 */
  uint32 periodTicks = (uint32)(currentFxClkFreq / (float32)freqHz);

  /* 3. 16비트 오버플로우 방어 로직 (안전 장치) */
  if (periodTicks > 0xFFFF)
  {
    periodTicks = 0xFFFF; /* 이 코드에 걸린다면 더 큰 분주비(Fxclk_3 등)가 필요합니다. */
  }

  uint32 dutyTicks = (uint32)((periodTicks * dutyPercent) / 100.0f);

  Ifx_GTM_TOM *tom = g_tomDriver.tom;
  IfxGtm_Tom_Ch channel = g_tomDriver.tomChannel;

  IfxGtm_Tom_Ch_setCompareZeroShadow(tom, channel, periodTicks);
  IfxGtm_Tom_Ch_setCompareOneShadow(tom, channel, dutyTicks);
}