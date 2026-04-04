#include "Base_EncoderMotor.h"

MotorInstance_t g_motorA;
MotorInstance_t g_motorB;

static boolean s_gtmInit = FALSE;

/* ------------------------------------------------------------------ */
static void initCommonMotorFields(MotorInstance_t *m, const MotorConfig_t *cfg)
{
    m->brakePort       = cfg->brakePort;
    m->brakePin        = cfg->brakePin;
    m->dirPort         = cfg->dirPort;
    m->dirPin          = cfg->dirPin;
    m->brakeActiveHigh = cfg->brakeActiveHigh;
    m->dirForwardHigh  = cfg->dirForwardHigh;

    m->singleChPort      = cfg->encPort;
    m->singleChPin       = cfg->encPin;
    m->singleChPrevValid = FALSE;
    m->singleChPrevState = 0U;
    m->lastDirCmd        = DIR_STOPPED;

    m->tickAcc = 0;
    m->move.originStartTicks = 0;

    m->move.state             = MOTOR_MOVE_IDLE;
    m->move.startTicks        = 0;
    m->move.targetTicks       = 0;
    m->move.finalTargetTicks  = 0;
    m->move.requestedDuty     = 0U;
    m->move.appliedDuty       = 0U;
    m->move.timeoutTicks      = 0U;
    m->move.startStm          = 0U;
    m->move.boostUntilStm     = 0U;
    m->move.cmdDir            = DIR_STOPPED;
    m->move.toleranceTicks    = MOTOR_DEFAULT_TOLERANCE_TICKS;
    m->move.correctionEnabled = TRUE;
    m->move.correctionCount   = 0U;
    m->move.correctionMaxCount = MOTOR_CORRECTION_MAX_COUNT;
    m->move.correctionDuty    = MOTOR_CORRECTION_DUTY;
}

static void initMotorOutputsAndPwm(MotorInstance_t *m, const MotorConfig_t *cfg)
{
    IfxPort_setPinModeOutput(m->brakePort, m->brakePin,
                             IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(m->dirPort, m->dirPin,
                             IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);

    if (m->brakeActiveHigh) IfxPort_setPinHigh(m->brakePort, m->brakePin);
    else                    IfxPort_setPinLow(m->brakePort, m->brakePin);

    if (m->dirForwardHigh)  IfxPort_setPinHigh(m->dirPort, m->dirPin);
    else                    IfxPort_setPinLow(m->dirPort, m->dirPin);

    if (!s_gtmInit)
    {
        IfxGtm_enable(&MODULE_GTM);
        IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_FXCLK);
        s_gtmInit = TRUE;
    }

    IfxGtm_Tom_Pwm_initConfig(&m->pwmCfg, &MODULE_GTM);
    m->pwmCfg.tom                      = (cfg->pwmPin)->tom;
    m->pwmCfg.tomChannel               = (cfg->pwmPin)->channel;
    m->pwmCfg.clock                    = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk0;
    m->pwmCfg.pin.outputPin            = (cfg->pwmPin);
    m->pwmCfg.pin.outputMode           = IfxPort_OutputMode_pushPull;
    m->pwmCfg.pin.padDriver            = IfxPort_PadDriver_cmosAutomotiveSpeed1;
    m->pwmCfg.synchronousUpdateEnabled = TRUE;
    m->pwmCfg.immediateStartEnabled    = TRUE;
    m->pwmCfg.period                   = MOTOR_PWM_PERIOD_TICKS;
    m->pwmCfg.dutyCycle                = 0U;
    m->pwmCfg.signalLevel              = Ifx_ActiveState_high;

    IfxGtm_Tom_Pwm_init(&m->pwmDrv, &m->pwmCfg);
    IfxGtm_Tom_Pwm_start(&m->pwmDrv, TRUE);
}

static uint32 permilleToTicks(const MotorInstance_t *m, uint16 duty)
{
    if (duty > MOTOR_PWM_DUTY_MAX) duty = MOTOR_PWM_DUTY_MAX;
    return (m->pwmCfg.period * (uint32)duty) / 1000U;
}

static void applyDutyRaw(MotorInstance_t *m, uint32 ticks)
{
    if (ticks > m->pwmCfg.period) ticks = m->pwmCfg.period;

    m->pwmCfg.dutyCycle = ticks;
    IfxGtm_Tom_Ch_setCompareOneShadow(m->pwmDrv.tom, m->pwmDrv.tomChannel, ticks);
    IfxGtm_Tom_Tgc_trigger(m->pwmDrv.tgc[0]);
}

static void Motor_StartDrive(MotorInstance_t *m,
                             MotorDirection_t dir,
                             uint16 duty,
                             uint32 timeoutMs,
                             MotorMoveState_t state,
                             boolean useBoost)
{
    uint32 now = IfxStm_get(BSP_DEFAULT_TIMER);
    uint16 initDuty = duty;

    if (useBoost)
    {
        if (initDuty < MOTOR_START_BOOST_DUTY)
            initDuty = MOTOR_START_BOOST_DUTY;
        m->move.boostUntilStm = now + ...;
    }
    else
    {
        m->move.boostUntilStm = now;
    }

    m->move.cmdDir       = dir;
    m->move.state        = state;
    m->move.requestedDuty = duty;
    m->move.timeoutTicks = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, timeoutMs);
    m->move.startStm     = now;
    m->move.boostUntilStm = now + IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, MOTOR_START_BOOST_MS);

    Motor_SetDuty(m, 0U);
    Motor_Brake(m);
    Motor_SetDirection(m, dir);
    Motor_Coast(m);

    m->move.appliedDuty = initDuty;
    Motor_SetDuty(m, initDuty);
}

static boolean Motor_TryStartCorrection(MotorInstance_t *m, sint32 error)
{
    sint32 correctionTicks;
    MotorDirection_t corrDir;

    if (!m->move.correctionEnabled) return FALSE;
    if (m->move.correctionCount >= m->move.correctionMaxCount) return FALSE;

    if (error > m->move.toleranceTicks)
    {
        /* overshoot: target+tolerance boundary까지 backward */
        correctionTicks = -(error - m->move.toleranceTicks);
        corrDir = DIR_BACKWARD;
    }
    else if (error < -m->move.toleranceTicks)
    {
        /* undershoot: target-tolerance boundary까지 forward */
        correctionTicks = (-error) - m->move.toleranceTicks;
        corrDir = DIR_FORWARD;
    }
    else
    {
        return FALSE;
    }

    if (correctionTicks == 0) return FALSE;

    m->move.correctionCount++;
    m->move.startTicks  = Motor_GetTicks(m);
    m->move.targetTicks = correctionTicks;

    Motor_StartDrive(m,
                     corrDir,
                     m->move.correctionDuty,
                     MOTOR_CORRECTION_TIMEOUT_MS,
                     MOTOR_MOVE_CORRECTING,
                     FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------ */
void Motor_Init(MotorInstance_t *m, const MotorConfig_t *cfg)
{
    initCommonMotorFields(m, cfg);
    initMotorOutputsAndPwm(m, cfg);

    IfxPort_setPinModeInput(cfg->encPort, cfg->encPin, IfxPort_InputMode_noPullDevice);

    Motor_ResetTicks(m);
}

/* ------------------------------------------------------------------ */
/* B와 동일한 polling 방식: rising edge만 count */
void Motor_EncoderService(MotorInstance_t *m)
{
    uint8 cur = IfxPort_getPinState(m->singleChPort, m->singleChPin);
    boolean ie = IfxCpu_disableInterrupts();

    if (!m->singleChPrevValid)
    {
        m->singleChPrevState = cur;
        m->singleChPrevValid = TRUE;
        IfxCpu_restoreInterrupts(ie);
        return;
    }

    if ((m->singleChPrevState == 0U) && (cur != 0U))
    {
        if (m->lastDirCmd == DIR_FORWARD)
        {
            m->tickAcc++;
        }
        else if (m->lastDirCmd == DIR_BACKWARD)
        {
            m->tickAcc--;
        }
    }

    m->singleChPrevState = cur;
    IfxCpu_restoreInterrupts(ie);
}

void Motor_Task1ms(MotorInstance_t *m)
{
    Motor_EncoderService(m);
    Motor_MoveService(m);
}

sint32 Motor_GetTicks(MotorInstance_t *m)
{
    return m->tickAcc;
}

sint32 Motor_GetRawPosition(MotorInstance_t *m)
{
    return m->tickAcc;
}

void Motor_ResetTicks(MotorInstance_t *m)
{
    boolean ie = IfxCpu_disableInterrupts();
    m->tickAcc           = 0;
    m->singleChPrevValid = FALSE;
    m->singleChPrevState = 0U;
    IfxCpu_restoreInterrupts(ie);
}

MotorDirection_t Motor_GetDirection(MotorInstance_t *m)
{
    return m->lastDirCmd;
}

/* ------------------------------------------------------------------ */
void Motor_SetDirection(MotorInstance_t *m, MotorDirection_t dir)
{
    m->lastDirCmd = dir;

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
}

void Motor_SetDuty(MotorInstance_t *m, uint16 dutyPermille)
{
    if (dutyPermille > MOTOR_PWM_DUTY_MAX) dutyPermille = MOTOR_PWM_DUTY_MAX;
    applyDutyRaw(m, permilleToTicks(m, dutyPermille));
}

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

/* ------------------------------------------------------------------ */
boolean Motor_MoveStart(MotorInstance_t *m,
                        sint32 ticks,
                        uint16 duty,
                        uint32 timeoutMs,
                        sint32 toleranceTicks)
{
    MotorDirection_t dir;

    if (ticks == 0)
    {
        Motor_Brake(m);
        m->move.state = MOTOR_MOVE_DONE;
        return TRUE;
    }

    if (Motor_IsBusy(m)) return FALSE;

    if (duty > MOTOR_PWM_DUTY_MAX) duty = MOTOR_PWM_DUTY_MAX;
    if (toleranceTicks < 0) toleranceTicks = -toleranceTicks;

    dir = (ticks > 0) ? DIR_FORWARD : DIR_BACKWARD;

    m->move.startTicks        = Motor_GetTicks(m);
    m->move.originStartTicks  = m->move.startTicks;
    m->move.targetTicks       = ticks;
    m->move.finalTargetTicks  = ticks;
    m->move.toleranceTicks    = toleranceTicks;
    m->move.correctionEnabled = TRUE;
    m->move.correctionCount   = 0U;
    m->move.correctionMaxCount = MOTOR_CORRECTION_MAX_COUNT;
    m->move.correctionDuty    = MOTOR_CORRECTION_DUTY;

    Motor_StartDrive(m, dir, duty, timeoutMs, MOTOR_MOVE_RUNNING, TRUE);

    return TRUE;
}

void Motor_MoveService(MotorInstance_t *m)
{
    sint32 cur;
    sint32 moved;
    sint32 finalError;
    uint32 now;
    uint32 elapsed;
    uint16 tgtDuty;

    if ((m->move.state != MOTOR_MOVE_RUNNING) &&
        (m->move.state != MOTOR_MOVE_CORRECTING))
    {
        return;
    }

    cur     = Motor_GetTicks(m);
    moved   = cur - m->move.startTicks;
    now     = IfxStm_get(BSP_DEFAULT_TIMER);
    elapsed = now - m->move.startStm;
    tgtDuty = m->move.requestedDuty;

    if ((m->move.targetTicks > 0 && moved >= m->move.targetTicks) ||
        (m->move.targetTicks < 0 && moved <= m->move.targetTicks))
    {
        Motor_Brake(m);
        m->move.appliedDuty = 0U;

        finalError = (Motor_GetTicks(m) - m->move.originStartTicks) - m->move.finalTargetTicks;

        if (m->move.state == MOTOR_MOVE_RUNNING)
        {
            if (Motor_TryStartCorrection(m, finalError))
            {
                return;
            }
        }

        m->move.state = MOTOR_MOVE_DONE;
        return;
    }

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

    if (now < m->move.boostUntilStm)
    {
        if (tgtDuty < MOTOR_START_BOOST_DUTY) tgtDuty = MOTOR_START_BOOST_DUTY;
    }
    /* slowdown 비활성화
    else
    {
        sint32 remaining = (m->move.targetTicks > 0)
            ? (m->move.targetTicks - moved)
            : (moved - m->move.targetTicks);

        if ((m->move.state == MOTOR_MOVE_RUNNING) &&
            (remaining < (sint32)MOTOR_SLOWDOWN_WINDOW_TICKS))
        {
            uint16 slowed = m->move.requestedDuty / 2U;
            if (slowed < MOTOR_SLOWDOWN_MIN_DUTY) slowed = MOTOR_SLOWDOWN_MIN_DUTY;
            tgtDuty = slowed;
        }
    }
     */
    if (tgtDuty != m->move.appliedDuty)
    {
        m->move.appliedDuty = tgtDuty;
        Motor_SetDuty(m, tgtDuty);
    }
}

boolean Motor_MoveToTick(MotorInstance_t *m,
                         sint32 absoluteTargetTick,
                         uint16 duty,
                         uint32 timeoutMs,
                         sint32 toleranceTicks)
{
    sint32 relativeTicks = absoluteTargetTick - Motor_GetTicks(m);
    return Motor_MoveStart(m, relativeTicks, duty, timeoutMs, toleranceTicks);
}

boolean Motor_IsBusy(MotorInstance_t *m)
{
    return (m->move.state == MOTOR_MOVE_RUNNING ||
            m->move.state == MOTOR_MOVE_CORRECTING) ? TRUE : FALSE;
}

MotorMoveState_t Motor_GetState(MotorInstance_t *m)
{
    return m->move.state;
}

void Motor_ClearState(MotorInstance_t *m)
{
    m->move.state              = MOTOR_MOVE_IDLE;
    m->move.startTicks         = 0;
    m->move.targetTicks        = 0;
    m->move.finalTargetTicks   = 0;
    m->move.originStartTicks = 0;
    m->move.requestedDuty      = 0U;
    m->move.appliedDuty        = 0U;
    m->move.timeoutTicks       = 0U;
    m->move.startStm           = 0U;
    m->move.boostUntilStm      = 0U;
    m->move.cmdDir             = DIR_STOPPED;
    m->move.toleranceTicks     = MOTOR_DEFAULT_TOLERANCE_TICKS;
    m->move.correctionEnabled  = TRUE;
    m->move.correctionCount    = 0U;
    m->move.correctionMaxCount = MOTOR_CORRECTION_MAX_COUNT;
    m->move.correctionDuty     = MOTOR_CORRECTION_DUTY;
}
