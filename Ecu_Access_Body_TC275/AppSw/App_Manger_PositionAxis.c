#include <App_Manger_PositionAxis.h>

static sint32 axisClampTick(const PositionAxis_t *a, sint32 tick)
{
    if (tick < a->minTick) tick = a->minTick;
    if (tick > a->maxTick) tick = a->maxTick;
    return tick;
}

static uint32 axisMsToStmTicks(uint32 ms)
{
    return IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, ms);
}

static void axisSetErrorFromMotorState(PositionAxis_t *a, MotorMoveState_t st)
{
    a->mode = AXIS_MODE_ERROR;

    if (st == MOTOR_MOVE_TIMEOUT)      a->lastResult = AXIS_RESULT_TIMEOUT;
    else if (st == MOTOR_MOVE_WRONG_DIR) a->lastResult = AXIS_RESULT_WRONG_DIR;
    else                               a->lastResult = AXIS_RESULT_REJECTED;
}

static boolean axisStartAbsoluteMove(PositionAxis_t *a,
                                     sint32 targetTick,
                                     uint16 duty,
                                     uint32 timeoutMs,
                                     sint32 toleranceTicks)
{
    Motor_ClearState(a->motor);
    targetTick = axisClampTick(a, targetTick);

    if (!Motor_MoveToTick(a->motor, targetTick, duty, timeoutMs, toleranceTicks))
    {
        a->lastResult = AXIS_RESULT_REJECTED;
        return FALSE;
    }

    return TRUE;
}

static sint32 axisCalcJogNextTick(const PositionAxis_t *a, sint32 curTick, boolean positive)
{
    sint32 step = a->jogStepTick;
    sint32 nextTick;
    sint32 rem;

    if (step <= 0) step = 1;

    if (!a->jogUseGridStep)
    {
        nextTick = positive ? (curTick + step) : (curTick - step);
        return axisClampTick(a, nextTick);
    }

    rem = curTick % step;
    if (rem < 0) rem += step;

    if (positive)
    {
        nextTick = (rem == 0) ? (curTick + step) : (curTick + (step - rem));
    }
    else
    {
        nextTick = (rem == 0) ? (curTick - step) : (curTick - rem);
    }

    return axisClampTick(a, nextTick);
}

static void axisHandleRestoreToZero(PositionAxis_t *a)
{
    MotorMoveState_t st = Motor_GetState(a->motor);

    if (Motor_IsBusy(a->motor)) return;

    if (st == MOTOR_MOVE_DONE)
    {
        Motor_ClearState(a->motor);

        if (a->restoreTargetTick == 0)
        {
            a->mode = AXIS_MODE_IDLE;
            a->lastResult = AXIS_RESULT_DONE;
            return;
        }

        if (axisStartAbsoluteMove(a,
                                  a->restoreTargetTick,
                                  a->restoreDuty,
                                  a->restoreTimeoutMs,
                                  a->restoreToleranceTicks))
        {
            a->mode = AXIS_MODE_RESTORE_TO_TARGET;
            return;
        }

        a->mode = AXIS_MODE_ERROR;
        return;
    }

    if ((st == MOTOR_MOVE_TIMEOUT) || (st == MOTOR_MOVE_WRONG_DIR))
    {
        axisSetErrorFromMotorState(a, st);
        Motor_ClearState(a->motor);
    }
}

static void axisHandleRestoreToTarget(PositionAxis_t *a)
{
    MotorMoveState_t st = Motor_GetState(a->motor);

    if (Motor_IsBusy(a->motor)) return;

    if (st == MOTOR_MOVE_DONE)
    {
        Motor_ClearState(a->motor);
        a->mode = AXIS_MODE_IDLE;
        a->lastResult = AXIS_RESULT_DONE;
        return;
    }

    if ((st == MOTOR_MOVE_TIMEOUT) || (st == MOTOR_MOVE_WRONG_DIR))
    {
        axisSetErrorFromMotorState(a, st);
        Motor_ClearState(a->motor);
    }
}

static void axisHandleJog(PositionAxis_t *a, boolean positive)
{
    uint32 now;
    sint32 curTick;
    sint32 nextTick;
    MotorMoveState_t st;

    st = Motor_GetState(a->motor);

    if (Motor_IsBusy(a->motor))
    {
        return;
    }

    if ((st == MOTOR_MOVE_TIMEOUT) || (st == MOTOR_MOVE_WRONG_DIR))
    {
        axisSetErrorFromMotorState(a, st);
        Motor_ClearState(a->motor);
        return;
    }

    if (st == MOTOR_MOVE_DONE)
    {
        Motor_ClearState(a->motor);
    }

    now = IfxStm_get(BSP_DEFAULT_TIMER);
    if ((a->lastJogIssueStm != 0U) &&
        ((now - a->lastJogIssueStm) < axisMsToStmTicks(a->jogIssuePeriodMs)))
    {
        return;
    }

    curTick = Motor_GetTicks(a->motor);
    nextTick = axisCalcJogNextTick(a, curTick, positive);

    if (nextTick == curTick)
    {
        return;
    }

    if (axisStartAbsoluteMove(a,
                              nextTick,
                              a->jogDuty,
                              a->jogTimeoutMs,
                              a->jogToleranceTicks))
    {
        a->lastJogIssueStm = now;
    }
    else
    {
        a->mode = AXIS_MODE_ERROR;
    }
}

void PositionAxis_Init(PositionAxis_t *a, const PositionAxisConfig_t *cfg)
{
    a->motor                 = cfg->motor;
    a->minTick               = cfg->minTick;
    a->maxTick               = cfg->maxTick;
    a->jogStepTick           = cfg->jogStepTick;
    a->jogDuty               = cfg->jogDuty;
    a->jogTimeoutMs          = cfg->jogTimeoutMs;
    a->jogToleranceTicks     = cfg->jogToleranceTicks;
    a->jogIssuePeriodMs      = cfg->jogIssuePeriodMs;
    a->jogUseGridStep        = cfg->jogUseGridStep;
    a->restoreDuty           = cfg->restoreDuty;
    a->restoreTimeoutMs      = cfg->restoreTimeoutMs;
    a->restoreToleranceTicks = cfg->restoreToleranceTicks;
    a->restoreTargetTick     = 0;
    a->lastJogIssueStm       = 0U;
    a->mode                  = AXIS_MODE_IDLE;
    a->lastResult            = AXIS_RESULT_NONE;
}

void PositionAxis_Task1ms(PositionAxis_t *a)
{
    Motor_Task1ms(a->motor);

    switch (a->mode)
    {
    case AXIS_MODE_RESTORE_TO_ZERO:
        axisHandleRestoreToZero(a);
        break;

    case AXIS_MODE_RESTORE_TO_TARGET:
        axisHandleRestoreToTarget(a);
        break;

    case AXIS_MODE_JOG_POSITIVE:
        axisHandleJog(a, TRUE);
        break;

    case AXIS_MODE_JOG_NEGATIVE:
        axisHandleJog(a, FALSE);
        break;

    case AXIS_MODE_IDLE:
    case AXIS_MODE_ERROR:
    default:
        break;
    }
}

boolean PositionAxis_StartRestore(PositionAxis_t *a, sint32 targetTick)
{
    if (PositionAxis_IsBusy(a)) return FALSE;

    a->restoreTargetTick = axisClampTick(a, targetTick);
    a->lastResult        = AXIS_RESULT_NONE;
    a->lastJogIssueStm   = 0U;

    if (!axisStartAbsoluteMove(a,
                               0,
                               a->restoreDuty,
                               a->restoreTimeoutMs,
                               a->restoreToleranceTicks))
    {
        a->mode = AXIS_MODE_ERROR;
        return FALSE;
    }

    a->mode = AXIS_MODE_RESTORE_TO_ZERO;
    return TRUE;
}

boolean PositionAxis_StartParkZero(PositionAxis_t *a)
{
    return PositionAxis_StartRestore(a, 0);
}

void PositionAxis_StartJogPositive(PositionAxis_t *a)
{
    if ((a->mode == AXIS_MODE_RESTORE_TO_ZERO) ||
        (a->mode == AXIS_MODE_RESTORE_TO_TARGET))
    {
        return;
    }

    a->lastResult = AXIS_RESULT_NONE;
    if (a->mode != AXIS_MODE_JOG_POSITIVE) a->lastJogIssueStm = 0U;
    a->mode = AXIS_MODE_JOG_POSITIVE;
}

void PositionAxis_StartJogNegative(PositionAxis_t *a)
{
    if ((a->mode == AXIS_MODE_RESTORE_TO_ZERO) ||
        (a->mode == AXIS_MODE_RESTORE_TO_TARGET))
    {
        return;
    }

    a->lastResult = AXIS_RESULT_NONE;
    if (a->mode != AXIS_MODE_JOG_NEGATIVE) a->lastJogIssueStm = 0U;
    a->mode = AXIS_MODE_JOG_NEGATIVE;
}

void PositionAxis_Stop(PositionAxis_t *a)
{
    Motor_Brake(a->motor);
    Motor_ClearState(a->motor);

    a->mode = AXIS_MODE_IDLE;
    a->lastJogIssueStm = 0U;
    a->lastResult = AXIS_RESULT_CANCELED;
}

boolean PositionAxis_IsBusy(PositionAxis_t *a)
{
    return (a->mode == AXIS_MODE_RESTORE_TO_ZERO) ||
           (a->mode == AXIS_MODE_RESTORE_TO_TARGET) ||
           (a->mode == AXIS_MODE_JOG_POSITIVE) ||
           (a->mode == AXIS_MODE_JOG_NEGATIVE);
}

PositionAxisMode_t PositionAxis_GetMode(PositionAxis_t *a)
{
    return a->mode;
}

PositionAxisResult_t PositionAxis_GetResult(PositionAxis_t *a)
{
    return a->lastResult;
}

void PositionAxis_ClearResult(PositionAxis_t *a)
{
    a->lastResult = AXIS_RESULT_NONE;
}

sint32 PositionAxis_GetTick(PositionAxis_t *a)
{
    return Motor_GetTicks(a->motor);
}
