#ifndef APP_MANGER_POSITIONAXIS_H_
#define APP_MANGER_POSITIONAXIS_H_

#include "Base_EncoderMotor.h"

/*
 * Generic position-axis wrapper for seat / side-mirror encoder motors.
 *
 * Policy reflected from discussion:
 * 1) Profile restore = always go to logical tick 0 first, then move to target tick.
 * 2) Manual adjust = smooth jog while command is kept active.
 * 3) Profile save/load and button scan are handled outside the wrapper.
 */

typedef enum
{
    AXIS_MODE_IDLE = 0,
    AXIS_MODE_RESTORE_TO_ZERO,
    AXIS_MODE_RESTORE_TO_TARGET,
    AXIS_MODE_JOG_POSITIVE,
    AXIS_MODE_JOG_NEGATIVE,
    AXIS_MODE_ERROR
} PositionAxisMode_t;

typedef enum
{
    AXIS_RESULT_NONE = 0,
    AXIS_RESULT_DONE,
    AXIS_RESULT_TIMEOUT,
    AXIS_RESULT_WRONG_DIR,
    AXIS_RESULT_CANCELED,
    AXIS_RESULT_REJECTED
} PositionAxisResult_t;

typedef struct
{
    MotorInstance_t *motor;

    sint32           minTick;
    sint32           maxTick;

    sint32           jogStepTick;
    uint16           jogDuty;
    uint32           jogTimeoutMs;
    sint32           jogToleranceTicks;
    uint32           jogIssuePeriodMs;
    boolean          jogUseGridStep;

    uint16           restoreDuty;
    uint32           restoreTimeoutMs;
    sint32           restoreToleranceTicks;
} PositionAxisConfig_t;

typedef struct
{
    MotorInstance_t       *motor;

    sint32                 minTick;
    sint32                 maxTick;

    sint32                 jogStepTick;
    uint16                 jogDuty;
    uint32                 jogTimeoutMs;
    sint32                 jogToleranceTicks;
    uint32                 jogIssuePeriodMs;
    boolean                jogUseGridStep;

    uint16                 restoreDuty;
    uint32                 restoreTimeoutMs;
    sint32                 restoreToleranceTicks;

    sint32                 restoreTargetTick;
    uint32                 lastJogIssueStm;

    PositionAxisMode_t     mode;
    PositionAxisResult_t   lastResult;
} PositionAxis_t;

/* Optional semantic aliases */
typedef PositionAxis_t MirrorAxis_t;
typedef PositionAxis_t SeatAxis_t;
typedef PositionAxisConfig_t MirrorAxisConfig_t;
typedef PositionAxisConfig_t SeatAxisConfig_t;

void                 PositionAxis_Init(PositionAxis_t *a, const PositionAxisConfig_t *cfg);
void                 PositionAxis_Task1ms(PositionAxis_t *a);

boolean              PositionAxis_StartRestore(PositionAxis_t *a, sint32 targetTick);
boolean              PositionAxis_StartParkZero(PositionAxis_t *a);

void                 PositionAxis_StartJogPositive(PositionAxis_t *a);
void                 PositionAxis_StartJogNegative(PositionAxis_t *a);
void                 PositionAxis_Stop(PositionAxis_t *a);

boolean              PositionAxis_IsBusy(PositionAxis_t *a);
PositionAxisMode_t   PositionAxis_GetMode(PositionAxis_t *a);
PositionAxisResult_t PositionAxis_GetResult(PositionAxis_t *a);
void                 PositionAxis_ClearResult(PositionAxis_t *a);

sint32               PositionAxis_GetTick(PositionAxis_t *a);

#endif /* APP_MANGER_POSITIONAXIS_H_ */
