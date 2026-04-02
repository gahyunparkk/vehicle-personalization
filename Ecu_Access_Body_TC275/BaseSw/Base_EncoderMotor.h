#ifndef BASE_ENCODERMOTOR_H_
#define BASE_ENCODERMOTOR_H_

#include "IfxPort.h"
#include "IfxGpt12.h"
#include "IfxGpt12_IncrEnc.h"
#include "IfxGpt12_PinMap.h"
#include "IfxPort_PinMap.h"
#include "IfxGtm_Tom_Pwm.h"
#include "IfxGtm_Tom.h"
#include "IfxGtm_PinMap.h"
#include "IfxCpu.h"
#include "Bsp.h"

/* ------------------------------------------------------------------
 * Channel A — Arduino Motor Shield R3 Ch.A
 * D12/P10.1 : Direction
 * D3 /P02.1 : PWM → TOM1_9
 * D9 /P02.7 : Brake
 * Encoder   : GPT12 T2 / D26(P00.8), D39(P00.7)
 * ------------------------------------------------------------------ */
#define MOTOR_A_BRAKE_PORT          IfxPort_P02_7.port
#define MOTOR_A_BRAKE_PIN_IDX       ((uint8)IfxPort_P02_7.pinIndex)
#define MOTOR_A_DIR_PORT            IfxPort_P10_1.port
#define MOTOR_A_DIR_PIN_IDX         ((uint8)IfxPort_P10_1.pinIndex)
#define MOTOR_A_PWM_PIN             &IfxGtm_TOM1_9_TOUT1_P02_1_OUT
#define MOTOR_A_ENC_PIN_A           ((const IfxGpt12_TxIn_In  *)&IfxGpt120_T2INA_P00_7_IN)
#define MOTOR_A_ENC_PIN_B           ((const IfxGpt12_TxEud_In  *)&IfxGpt120_T2EUDA_P00_8_IN)
#define ISR_PRIORITY_ENC_A          7

/* ------------------------------------------------------------------
 * Channel B — Arduino Motor Shield R3 Ch.B
 * D13/P10.2 : Direction
 * D11/P10.3 : PWM → TOM0_3 / TOUT105
 * D8 /P02.6 : Brake
 * Encoder   : GPT12 T3 / D4(P10.4), A4(P10.7)
 * ------------------------------------------------------------------ */
#define MOTOR_B_BRAKE_PORT          IfxPort_P02_6.port
#define MOTOR_B_BRAKE_PIN_IDX       ((uint8)IfxPort_P02_6.pinIndex)
#define MOTOR_B_DIR_PORT            IfxPort_P10_2.port
#define MOTOR_B_DIR_PIN_IDX         ((uint8)IfxPort_P10_2.pinIndex)
#define MOTOR_B_PWM_PIN             (&IfxGtm_TOM0_3_TOUT105_P10_3_OUT)
#define MOTOR_B_ENC_PIN_A           (&IfxGpt120_T3INB_P10_4_IN)
#define MOTOR_B_ENC_PIN_B           (&IfxGpt120_T3EUDB_P10_7_IN)
#define ISR_PRIORITY_ENC_B          6

/* ------------------------------------------------------------------
 * 공통 파라미터
 * ------------------------------------------------------------------ */
#define ENCODER_RESOLUTION          1200U
#define MOTOR_ENCODER_MODULO        (ENCODER_RESOLUTION * 4U)

#define MOTOR_PWM_PERIOD_TICKS      200000U
#define MOTOR_PWM_DUTY_MIN          0U
#define MOTOR_PWM_DUTY_MAX          1000U       /* permille(퍼밀, ‰) 기준 */

#define MOTOR_DIR_SETTLE_MS         5U          /* 방향 전환 후 H-bridge 안정화 대기 */
#define MOTOR_START_BOOST_DUTY      600U        /* 정지 마찰 극복용 초기 기동 최솟값 */
#define MOTOR_START_BOOST_MS        100U
#define MOTOR_SLOWDOWN_WINDOW_TICKS 50U         /* 목표 도달 전 감속 진입 구간 */
#define MOTOR_SLOWDOWN_MIN_DUTY     120U        /* 감속 후 최솟값 */
#define MOTOR_WRONG_DIR_MARGIN      100         /* 반대 방향 이탈 허용 범위 */
#define MOTOR_DEFAULT_TIMEOUT_MS    3000U

/* ------------------------------------------------------------------
 * 타입 정의
 * ------------------------------------------------------------------ */
typedef enum
{
    DIR_STOPPED = 0,
    DIR_FORWARD,
    DIR_BACKWARD
} MotorDirection_t;

typedef enum
{
    MOTOR_MOVE_IDLE = 0,
    MOTOR_MOVE_RUNNING,
    MOTOR_MOVE_DONE,
    MOTOR_MOVE_TIMEOUT,
    MOTOR_MOVE_WRONG_DIR
} MotorMoveState_t;

typedef struct
{
    MotorMoveState_t state;
    sint32           startTicks;
    sint32           targetTicks;
    uint16           requestedDuty;
    uint16           appliedDuty;
    uint32           timeoutTicks;
    uint32           startStm;
    uint32           boostUntilStm;
    MotorDirection_t cmdDir;
} MotorMoveCtrl_t;

/* 채널 하나의 전체 상태 — 두 인스턴스(g_motorA, g_motorB)에 대해 각각 사용 */
typedef struct
{
    IfxGpt12_IncrEnc      enc;
    IfxGtm_Tom_Pwm_Config pwmCfg;
    IfxGtm_Tom_Pwm_Driver pwmDrv;

    Ifx_P  *brakePort;
    uint8   brakePin;
    Ifx_P  *dirPort;
    uint8   dirPin;
    boolean brakeActiveHigh;
    boolean dirForwardHigh;

    boolean tickInit;
    sint32  tickPrev;
    sint32  tickAcc;

    MotorMoveCtrl_t move;
} MotorInstance_t;

/* Motor_Init() 에 넘겨주는 채널별 configuration */
typedef struct
{
    const IfxGtm_Tom_ToutMap  *pwmPin;
    const IfxGpt12_TxIn_In    *encPinA;
    const IfxGpt12_TxEud_In    *encPinB;
    Ifx_P  *brakePort;
    uint8   brakePin;
    Ifx_P  *dirPort;
    uint8   dirPin;
    boolean brakeActiveHigh;
    boolean dirForwardHigh;
    uint8   encIsrPriority;
} MotorConfig_t;

/* ISR에서 직접 접근하므로 전역 선언 */
extern MotorInstance_t g_motorA;
extern MotorInstance_t g_motorB;

/* ------------------------------------------------------------------
 * API — 모든 함수가 MotorInstance_t* 를 첫 인자로 받음
 *
 * 사용 패턴:
 *   Motor_Init(&g_motorB, &cfgB);
 *   Motor_MoveStart(&g_motorB, 1200, 700, MOTOR_DEFAULT_TIMEOUT_MS);
 *   while (Motor_IsBusy(&g_motorB)) { Motor_MoveService(&g_motorB); }
 *   result = Motor_GetState(&g_motorB);
 *   Motor_ClearState(&g_motorB);
 * ------------------------------------------------------------------ */
void             Motor_Init(MotorInstance_t *m, const MotorConfig_t *cfg);

sint32           Motor_GetTicks(MotorInstance_t *m);     /* 호출마다 내부 상태 갱신 — 루프당 1회만 */
void             Motor_ResetTicks(MotorInstance_t *m);
MotorDirection_t Motor_GetDirection(MotorInstance_t *m);

void             Motor_SetDirection(MotorInstance_t *m, MotorDirection_t dir);
void             Motor_SetDuty(MotorInstance_t *m, uint16 dutyPermille);
void             Motor_Brake(MotorInstance_t *m);
void             Motor_Coast(MotorInstance_t *m);

boolean          Motor_MoveStart(MotorInstance_t *m, sint32 ticks, uint16 duty, uint32 timeoutMs);
void             Motor_MoveService(MotorInstance_t *m);   /* 메인 루프에서 반드시 주기 호출 */
boolean          Motor_IsBusy(MotorInstance_t *m);
MotorMoveState_t Motor_GetState(MotorInstance_t *m);
void             Motor_ClearState(MotorInstance_t *m);    /* 다음 명령 전 반드시 호출 */

#endif /* BASE_ENCODERMOTOR_H_ */
