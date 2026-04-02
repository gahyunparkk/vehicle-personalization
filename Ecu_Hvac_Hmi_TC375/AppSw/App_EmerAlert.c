#include "App_EmerAlert.h"
#include "Base_Buzzer.h"
#include "Driver_Stm.h"

#include "Stm/Std/IfxStm.h"

/**
 * @brief 마이크로초(us) 단위 블로킹 딜레이 함수
 * @param microSeconds 대기할 마이크로초 시간
 */
static void delay_ms(uint32 milliSeconds)
{
  /* * 1. 시스템 타이머 모듈 선택
   * CPU0에서 실행되는 코드라면 MODULE_STM0을 사용합니다.
   * (예: CPU1인 경우 MODULE_STM1 사용)
   * 보드 지원 패키지(BSP)가 설정되어 있다면 BSP_DEFAULT_TIMER 매크로를 권장합니다.
   */
  Ifx_STM *stm = &MODULE_STM0;

  /* 2. 현재 타이머 클럭 주파수를 바탕으로 해당 us에 필요한 틱(Tick) 수 계산 */
  uint32 ticks = (uint32)IfxStm_getTicksFromMilliseconds(stm, milliSeconds);

  /* 3. 계산된 틱 수만큼 대기 (Blocking) */
  IfxStm_waitTicks(stm, ticks);
}

void Emer_buzz50ms(void)
{
  setPwm10_FreqAndDuty(1000, 80);
  delay_ms(50);
  turnOffPwm10_Soft();
}

void Emer_buzz500ms(void)
{
  setPwm10_FreqAndDuty(880, 80);
  delay_ms(500);
  turnOffPwm10_Soft();
}