#include "Base_Shock.h"
#include "App_EmerAlert.h"

#include "IfxPort.h"
#include "IfxScuEru.h"
#include "IfxSrc.h"

#define SHOCK_TIME        500
#define SHOCK_SENSITIVITY 120

// 사용하는 OGU 채널 설정 (3번 채널 사용)
#define ERU_OGU_CHANNEL 3
// REQ 핀 설정 (하드웨어 연결에 맞게 P02.7 또는 P10.3 확인)
#define SHOCK_SENSOR_REQIN (&IfxScu_REQ3A_P10_3_IN)

void init_ShockSensor_FlagPolling(void)
{
  // 1. 핀을 입력으로 설정
  IfxPort_setPinMode(SHOCK_SENSOR_REQIN->pin.port, SHOCK_SENSOR_REQIN->pin.pinIndex, IfxPort_Mode_inputPullUp);

  // 2. ERU (Event Request Unit) 설정
  IfxScuEru_clearAllEventFlags();

  // 입력 핀과 ERU 연결 및 Edge 설정 (Rising Edge 감지)
  IfxScuEru_initReqPin(SHOCK_SENSOR_REQIN, IfxPort_InputMode_pullUp);
  IfxScuEru_enableRisingEdgeDetection(SHOCK_SENSOR_REQIN->channelId); // LOW -> HIGH

  // 이벤트를 발생시킬 OGU 채널로 라우팅
  IfxScuEru_enableTriggerPulse(SHOCK_SENSOR_REQIN->channelId);

  // 트리거 연결 (입력 채널의 신호를 선택된 OGU로 전달)
  IfxScuEru_connectTrigger(SHOCK_SENSOR_REQIN->channelId, (IfxScuEru_InputNodePointer)ERU_OGU_CHANNEL);

  // ★ OGU에서 신호를 통과시키도록 Gating Pattern 설정 (필수)
  IfxScuEru_setInterruptGatingPattern((IfxScuEru_OutputChannel)ERU_OGU_CHANNEL, IfxScuEru_InterruptGatingPattern_alwaysActive);

  // 3. SRC (Service Request Control) 설정
  // ★ 에러 수정: MODULE_SRC 구조체를 통해 지정된 OGU 채널의 SRC 레지스터에 직접 접근
  volatile Ifx_SRC_SRCR *src = &MODULE_SRC.SCU.SCUERU[(int)ERU_OGU_CHANNEL];

  // 인터럽트 노드를 초기화합니다.
  IfxSrc_init(src, IfxSrc_Tos_cpu0, 10);

  // CPU가 ISR로 점프하지 않도록 차단합니다 (Polling 방식이므로 SRE = 0).
  IfxSrc_disable(src);

  // 초기화 과정에서 설정되었을 수 있는 쓰레기 플래그(SRR)를 제거합니다.
  IfxSrc_clearRequest(src);
}

void shock_task(void)
{
  // ★ 에러 수정: 초기화 함수와 동일하게 MODULE_SRC 구조체를 사용하여 포인터 획득
  volatile Ifx_SRC_SRCR *src = &MODULE_SRC.SCU.SCUERU[(int)ERU_OGU_CHANNEL];

  volatile static int shockcnt = 0;
  volatile static int taskcnt = 0;

  if (taskcnt++ == SHOCK_TIME)
  {
    shockcnt = 0;
    taskcnt = 0;
  }

  // SRC의 SRR(Service Request) 비트가 1인지 확인합니다.
  if (IfxSrc_isRequested(src))
  {
    // --------------------------------------------------
    if (++shockcnt == SHOCK_SENSITIVITY)
    {
      Emer_buzz500ms();
    }
    // --------------------------------------------------

    // 처리 완료 후 반드시 플래그를 지워줍니다.
    IfxSrc_clearRequest(src);
  }
}
