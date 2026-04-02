#include <Base_EncoderMotor.h>

MotorInstance_t g_motorA;
MotorInstance_t g_motorB;

/* GPT12, GTM은 채널 공유, 한 번만 init하도록 */
static boolean s_gpt12Init = FALSE;
static boolean s_gtmInit   = FALSE;

/* ------------------------------------------------------------------
 * ISR — T3(Ch.B), T2(Ch.A) 각각 별도 등록
 * pinZ = NULL이라 실제 발생은 없고, T2/T3가 같은 onZeroIrq를 공유하므로 겹치지 않게 NULL로 사용 X
 * ------------------------------------------------------------------ */
IFX_INTERRUPT(isrEncB, 0, ISR_PRIORITY_ENC_B);
void isrEncB(void) { IfxGpt12_IncrEnc_onZeroIrq(&g_motorB.enc); }

IFX_INTERRUPT(isrEncA, 0, ISR_PRIORITY_ENC_A);
void isrEncA(void) { IfxGpt12_IncrEnc_onZeroIrq(&g_motorA.enc); }

/* ------------------------------------------------------------------
 * 내부 헬퍼 함수
 * ------------------------------------------------------------------ */
static uint32 permilleToTicks(const MotorInstance_t *m, uint16 duty)
{
    if (duty > MOTOR_PWM_DUTY_MAX) duty = MOTOR_PWM_DUTY_MAX;
    return (m->pwmCfg.period * (uint32)duty) / 1000U;
}

/* shadow register 경유: 다음 PWM 주기에 atomic하게 반영됨 */
static void applyDutyRaw(MotorInstance_t *m, uint32 ticks)
{
    if (ticks > m->pwmCfg.period) ticks = m->pwmCfg.period;
    m->pwmCfg.dutyCycle = ticks;
    IfxGtm_Tom_Ch_setCompareOneShadow(m->pwmDrv.tom, m->pwmDrv.tomChannel, ticks);
    IfxGtm_Tom_Tgc_trigger(m->pwmDrv.tgc[0]);
}

static sint32 getRawPosition(MotorInstance_t *m)
{
    IfxGpt12_IncrEnc_update(&m->enc);
    return IfxGpt12_IncrEnc_getRawPosition(&m->enc);
}

/* ------------------------------------------------------------------
 * Motor_Init
 * ------------------------------------------------------------------ */
void Motor_Init(MotorInstance_t *m, const MotorConfig_t *cfg)
{
    /* 인스턴스 필드 초기화 */
    m->brakePort       = cfg->brakePort;
    m->brakePin        = cfg->brakePin;
    m->dirPort         = cfg->dirPort;
    m->dirPin          = cfg->dirPin;
    m->brakeActiveHigh = cfg->brakeActiveHigh;
    m->dirForwardHigh  = cfg->dirForwardHigh;
    m->tickInit        = FALSE;
    m->tickPrev        = 0;
    m->tickAcc         = 0;
    m->move.state         = MOTOR_MOVE_IDLE;
    m->move.startTicks    = 0;
    m->move.targetTicks   = 0;
    m->move.requestedDuty = 0U;
    m->move.appliedDuty   = 0U;
    m->move.timeoutTicks  = 0U;
    m->move.startStm      = 0U;
    m->move.boostUntilStm = 0U;
    m->move.cmdDir        = DIR_STOPPED;

    IfxPort_setPinModeOutput(m->brakePort, m->brakePin,
                             IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(m->dirPort, m->dirPin,
                             IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);

    /* 초기 상태: 브레이크 ON, 방향 Forward */
    if (m->brakeActiveHigh) IfxPort_setPinHigh(m->brakePort, m->brakePin);
    else                    IfxPort_setPinLow(m->brakePort, m->brakePin);

    if (m->dirForwardHigh)  IfxPort_setPinHigh(m->dirPort, m->dirPin);
    else                    IfxPort_setPinLow(m->dirPort, m->dirPin);

    /* GPT12 모듈 레벨 설정: T2, T3가 공유하므로 한 번만 */
    if (!s_gpt12Init)
    {
        IfxGpt12_enableModule(&MODULE_GPT120);
        IfxGpt12_setGpt1BlockPrescaler(&MODULE_GPT120, IfxGpt12_Gpt1BlockPrescaler_8);
        IfxGpt12_setGpt2BlockPrescaler(&MODULE_GPT120, IfxGpt12_Gpt2BlockPrescaler_4);
        s_gpt12Init = TRUE;
    }

    IfxGpt12_IncrEnc_Config encCfg;
    IfxGpt12_IncrEnc_initConfig(&encCfg, &MODULE_GPT120);
    encCfg.base.offset             = 0;
    encCfg.base.reversed           = FALSE;
    encCfg.base.resolution         = ENCODER_RESOLUTION;
    encCfg.base.resolutionFactor   = IfxStdIf_Pos_ResolutionFactor_fourFold;
    encCfg.base.updatePeriod       = 1e-3f;
    encCfg.base.speedModeThreshold = 200.0f;
    encCfg.base.minSpeed           = 1.0f;
    encCfg.base.maxSpeed           = 1000.0f;
    encCfg.pinA                    = cfg->encPinA;
    encCfg.pinB                    = cfg->encPinB;
    encCfg.pinZ                    = NULL_PTR;
    encCfg.pinMode                 = IfxPort_InputMode_pullUp;
    encCfg.pinDriver               = IfxPort_PadDriver_cmosAutomotiveSpeed3;
    encCfg.zeroIsrPriority         = cfg->encIsrPriority;
    encCfg.zeroIsrProvider         = IfxSrc_Tos_cpu0;
    encCfg.initPins                = TRUE;
    IfxGpt12_IncrEnc_init(&m->enc, &encCfg);

    /* GTM 마찬가지 */
    if (!s_gtmInit)
    {
        IfxGtm_enable(&MODULE_GTM);
        IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_FXCLK);
        s_gtmInit = TRUE;
    }

    IfxGtm_Tom_Pwm_initConfig(&m->pwmCfg, &MODULE_GTM);
    m->pwmCfg.tom                      = cfg->pwmPin->tom;
    m->pwmCfg.tomChannel               = cfg->pwmPin->channel;
    m->pwmCfg.clock                    = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk0;
    m->pwmCfg.pin.outputPin            = cfg->pwmPin;
    m->pwmCfg.pin.outputMode           = IfxPort_OutputMode_pushPull;
    m->pwmCfg.pin.padDriver            = IfxPort_PadDriver_cmosAutomotiveSpeed1;
    m->pwmCfg.synchronousUpdateEnabled = TRUE;
    m->pwmCfg.immediateStartEnabled    = TRUE;
    m->pwmCfg.period                   = MOTOR_PWM_PERIOD_TICKS;
    m->pwmCfg.dutyCycle                = 0U;
    m->pwmCfg.signalLevel              = Ifx_ActiveState_high;
    IfxGtm_Tom_Pwm_init(&m->pwmDrv, &m->pwmCfg);
    IfxGtm_Tom_Pwm_start(&m->pwmDrv, TRUE);

    Motor_ResetTicks(m);
}

/* ------------------------------------------------------------------
 * 엔코더 Ticks
 * ------------------------------------------------------------------ */
void Motor_ResetTicks(MotorInstance_t *m)
{
    m->tickInit = FALSE;
    m->tickPrev = 0;
    m->tickAcc  = 0;
    (void)Motor_GetTicks(m);
}

/* tickPrev/tickAcc 갱신 구간만 인터럽트 보호 */
sint32 Motor_GetTicks(MotorInstance_t *m)
{
    sint32 cur = getRawPosition(m);

    boolean ie = IfxCpu_disableInterrupts();

    if (!m->tickInit)
    {
        m->tickPrev = cur;
        m->tickInit = TRUE;
        IfxCpu_restoreInterrupts(ie);
        return 0;
    }

    sint32 diff = cur - m->tickPrev;

    /*  */
    if (diff >  (sint32)(MOTOR_ENCODER_MODULO / 2U)) diff -= (sint32)MOTOR_ENCODER_MODULO;
    if (diff < -(sint32)(MOTOR_ENCODER_MODULO / 2U)) diff += (sint32)MOTOR_ENCODER_MODULO;
    m->tickAcc += diff;
    m->tickPrev = cur;

    IfxCpu_restoreInterrupts(ie);
    return m->tickAcc;
}

MotorDirection_t Motor_GetDirection(MotorInstance_t *m)
{
    switch (IfxGpt12_IncrEnc_getDirection(&m->enc))
    {
        case IfxStdIf_Pos_Dir_forward:  return DIR_FORWARD;
        case IfxStdIf_Pos_Dir_backward: return DIR_BACKWARD;
        default:                        return DIR_STOPPED;
    }
}

/* ------------------------------------------------------------------
 * 출력 제어
 * ------------------------------------------------------------------ */
void Motor_SetDirection(MotorInstance_t *m, MotorDirection_t dir)
{
    if (dir == DIR_FORWARD)
    {
        if (m->dirForwardHigh) IfxPort_setPinHigh(m->dirPort, m->dirPin);
        else                   IfxPort_setPinLow(m->dirPort, m->dirPin);
    }
    else if (dir == DIR_BACKWARD)
    {
        if (m->dirForwardHigh) IfxPort_setPinLow(m->dirPort, m->dirPin);
        else                   IfxPort_setPinHigh(m->dirPort, m->dirPin);
    }
    else
    {
        Motor_Brake(m);
    }
}

void Motor_SetDuty(MotorInstance_t *m, uint16 dutyPermille)
{
    if (dutyPermille > MOTOR_PWM_DUTY_MAX) dutyPermille = MOTOR_PWM_DUTY_MAX;
    applyDutyRaw(m, permilleToTicks(m, dutyPermille));
}

/* duty 먼저 0 → 브레이크 핀 활성: 과전류 위험 방지 */
void Motor_Brake(MotorInstance_t *m)
{
    Motor_SetDuty(m, 0U);
    if (m->brakeActiveHigh) IfxPort_setPinHigh(m->brakePort, m->brakePin);
    else                    IfxPort_setPinLow(m->brakePort, m->brakePin);
}

void Motor_Coast(MotorInstance_t *m)
{
    if (m->brakeActiveHigh) IfxPort_setPinLow(m->brakePort, m->brakePin);
    else                    IfxPort_setPinHigh(m->brakePort, m->brakePin);
}

/* ------------------------------------------------------------------
 * Non-blocking 이동
 * ------------------------------------------------------------------ */
/* 성공(이동 시작 || ticks==0)시 TRUE, RUNNING 중 호출(Busy) 시 FALSE */
boolean Motor_MoveStart(MotorInstance_t *m, sint32 ticks, uint16 duty, uint32 timeoutMs)
{
    if (ticks == 0)
    {
        Motor_Brake(m);
        m->move.state = MOTOR_MOVE_DONE;
        return TRUE;
    }

    if (Motor_IsBusy(m)) return FALSE;

    if (duty > MOTOR_PWM_DUTY_MAX) duty = MOTOR_PWM_DUTY_MAX;

    m->move.startTicks    = Motor_GetTicks(m);
    m->move.targetTicks   = ticks;
    m->move.requestedDuty = duty;
    m->move.timeoutTicks  = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, timeoutMs);
    m->move.startStm      = IfxStm_get(BSP_DEFAULT_TIMER);
    m->move.boostUntilStm = m->move.startStm
                          + IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, MOTOR_START_BOOST_MS);
    m->move.cmdDir        = (ticks > 0) ? DIR_FORWARD : DIR_BACKWARD;
    m->move.state         = MOTOR_MOVE_RUNNING;

    /* H-bridge 방향 전환 시퀀스 */
    Motor_SetDuty(m, 0U);
    Motor_Brake(m);
    waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, MOTOR_DIR_SETTLE_MS));
    Motor_SetDirection(m, m->move.cmdDir);
    waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, MOTOR_DIR_SETTLE_MS));
    Motor_Coast(m);

    uint16 initDuty = (duty < MOTOR_START_BOOST_DUTY) ? MOTOR_START_BOOST_DUTY : duty;
    m->move.appliedDuty = initDuty;
    Motor_SetDuty(m, initDuty);

    return TRUE;
}

void Motor_MoveService(MotorInstance_t *m)
{
    if (m->move.state != MOTOR_MOVE_RUNNING) return;

    sint32 cur     = Motor_GetTicks(m);
    sint32 moved   = cur - m->move.startTicks;
    uint32 now     = IfxStm_get(BSP_DEFAULT_TIMER);
    uint32 elapsed = now - m->move.startStm;
    uint16 tgtDuty = m->move.requestedDuty;

    /* 목표 도달 확인 */
    if ((m->move.targetTicks > 0 && moved >= m->move.targetTicks) ||
        (m->move.targetTicks < 0 && moved <= m->move.targetTicks))
    {
        Motor_Brake(m);
        m->move.appliedDuty = 0U;
        m->move.state = MOTOR_MOVE_DONE;
        return;
    }

    /* 반대 방향 이탈 감지 및 return */
    if ((m->move.targetTicks > 0 && moved < -MOTOR_WRONG_DIR_MARGIN) ||
        (m->move.targetTicks < 0 && moved >  MOTOR_WRONG_DIR_MARGIN))
    {
        Motor_Brake(m);
        m->move.appliedDuty = 0U;
        m->move.state = MOTOR_MOVE_WRONG_DIR;
        return;
    }

    if (elapsed >= m->move.timeoutTicks)
    {
        Motor_Brake(m);
        m->move.appliedDuty = 0U;
        m->move.state = MOTOR_MOVE_TIMEOUT;
        return;
    }

    /* duty: boost → 요청 duty → 감속 구간 */
    if (now < m->move.boostUntilStm)
    {
        if (tgtDuty < MOTOR_START_BOOST_DUTY) tgtDuty = MOTOR_START_BOOST_DUTY;
    }
    else
    {
        sint32 remaining = (m->move.targetTicks > 0)
            ? (m->move.targetTicks - moved)
            : (moved - m->move.targetTicks);

        if (remaining < (sint32)MOTOR_SLOWDOWN_WINDOW_TICKS)
        {
            uint16 slowed = m->move.requestedDuty / 2U;
            if (slowed < MOTOR_SLOWDOWN_MIN_DUTY) slowed = MOTOR_SLOWDOWN_MIN_DUTY;
            tgtDuty = slowed;
        }
    }

    if (tgtDuty != m->move.appliedDuty)
    {
        m->move.appliedDuty = tgtDuty;
        Motor_SetDuty(m, tgtDuty);
    }
}

boolean Motor_IsBusy(MotorInstance_t *m)
{
    return (m->move.state == MOTOR_MOVE_RUNNING) ? TRUE : FALSE;
}

MotorMoveState_t Motor_GetState(MotorInstance_t *m)
{
    return m->move.state;
}

/* 직전 이동 파라미터까지 전부 초기화: 다음 MoveStart 전에 호출 */
void Motor_ClearState(MotorInstance_t *m)
{
    m->move.state         = MOTOR_MOVE_IDLE;
    m->move.startTicks    = 0;
    m->move.targetTicks   = 0;
    m->move.requestedDuty = 0U;
    m->move.appliedDuty   = 0U;
    m->move.timeoutTicks  = 0U;
    m->move.startStm      = 0U;
    m->move.boostUntilStm = 0U;
    m->move.cmdDir        = DIR_STOPPED;
}
