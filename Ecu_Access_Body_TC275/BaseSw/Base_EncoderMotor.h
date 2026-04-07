#ifndef BASE_ENCODERMOTOR_H_
#define BASE_ENCODERMOTOR_H_

#include "Ifx_Types.h"
#include "IfxPort.h"
#include "IfxPort_PinMap.h"
#include "IfxGtm_Tom_Pwm.h"
#include "IfxGtm_Tom.h"
#include "IfxGtm_PinMap.h"
#include "IfxCpu.h"
#include "IfxStm.h"
#include "Bsp.h"

/* ------------------------------------------------------------------
 * Channel A
 * ------------------------------------------------------------------ */
#define MOTOR_A_BRAKE_PORT          IfxPort_P02_7.port
#define MOTOR_A_BRAKE_PIN_IDX       ((uint8)IfxPort_P02_7.pinIndex)
#define MOTOR_A_DIR_PORT            IfxPort_P10_1.port
#define MOTOR_A_DIR_PIN_IDX         ((uint8)IfxPort_P10_1.pinIndex)
#define MOTOR_A_PWM_PIN             (&IfxGtm_TOM1_9_TOUT1_P02_1_OUT)
#define MOTOR_A_ENC_SINGLE_PORT     IfxPort_P00_7.port
#define MOTOR_A_ENC_SINGLE_PIN_IDX  ((uint8)IfxPort_P00_7.pinIndex)

/* ------------------------------------------------------------------
 * Channel B
 * ------------------------------------------------------------------ */
#define MOTOR_B_BRAKE_PORT          IfxPort_P02_6.port
#define MOTOR_B_BRAKE_PIN_IDX       ((uint8)IfxPort_P02_6.pinIndex)
#define MOTOR_B_DIR_PORT            IfxPort_P10_2.port
#define MOTOR_B_DIR_PIN_IDX         ((uint8)IfxPort_P10_2.pinIndex)
#define MOTOR_B_PWM_PIN             (&IfxGtm_TOM0_3_TOUT105_P10_3_OUT)
#define MOTOR_B_ENC_SINGLE_PORT     IfxPort_P10_4.port
#define MOTOR_B_ENC_SINGLE_PIN_IDX  ((uint8)IfxPort_P10_4.pinIndex)

/* ------------------------------------------------------------------ */
#define MOTOR_PWM_PERIOD_TICKS        200000U
#define MOTOR_PWM_DUTY_MAX            1000U

#define MOTOR_START_BOOST_DUTY        600U
#define MOTOR_START_BOOST_MS          100U
#define MOTOR_SLOWDOWN_WINDOW_TICKS   50U
#define MOTOR_SLOWDOWN_MIN_DUTY       120U
#define MOTOR_WRONG_DIR_MARGIN        5
#define MOTOR_DEFAULT_TIMEOUT_MS      3000U

#define MOTOR_DEFAULT_TOLERANCE_TICKS 2
#define MOTOR_CORRECTION_DUTY         340U
#define MOTOR_CORRECTION_MAX_COUNT    3U
#define MOTOR_CORRECTION_TIMEOUT_MS   800U

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
    MOTOR_MOVE_CORRECTING,
    MOTOR_MOVE_DONE,
    MOTOR_MOVE_TIMEOUT,
    MOTOR_MOVE_WRONG_DIR
} MotorMoveState_t;

typedef struct
{
    MotorMoveState_t state;

    sint32           startTicks;
    sint32           targetTicks;
    sint32           finalTargetTicks;
    sint32           originStartTicks;

    uint16           requestedDuty;
    uint16           appliedDuty;

    uint32           timeoutTicks;
    uint32           startStm;
    uint32           boostUntilStm;

    MotorDirection_t cmdDir;

    sint32           toleranceTicks;

    boolean          correctionEnabled;
    uint8            correctionCount;
    uint8            correctionMaxCount;
    uint16           correctionDuty;
} MotorMoveCtrl_t;

typedef struct
{
    IfxGtm_Tom_Pwm_Config pwmCfg;
    IfxGtm_Tom_Pwm_Driver pwmDrv;

    Ifx_P  *brakePort;
    uint8   brakePin;
    Ifx_P  *dirPort;
    uint8   dirPin;
    boolean brakeActiveHigh;
    boolean dirForwardHigh;

    Ifx_P  *singleChPort;
    uint8   singleChPin;
    boolean singleChPrevValid;
    uint8   singleChPrevState;
    MotorDirection_t lastDirCmd;

    sint32 tickAcc;

    MotorMoveCtrl_t move;
} MotorInstance_t;

typedef struct
{
    const IfxGtm_Tom_ToutMap *pwmPin;
    Ifx_P                    *brakePort;
    uint8                     brakePin;
    Ifx_P                    *dirPort;
    uint8                     dirPin;
    boolean                   brakeActiveHigh;
    boolean                   dirForwardHigh;
    Ifx_P                    *encPort;
    uint8                     encPin;
} MotorConfig_t;

extern MotorInstance_t g_motorA;
extern MotorInstance_t g_motorB;

void             Motor_Init(MotorInstance_t *m, const MotorConfig_t *cfg);

void             Motor_EncoderService(MotorInstance_t *m);
void             Motor_Task1ms(MotorInstance_t *m);

sint32           Motor_GetTicks(MotorInstance_t *m);
sint32           Motor_GetRawPosition(MotorInstance_t *m);
void             Motor_ResetTicks(MotorInstance_t *m);
MotorDirection_t Motor_GetDirection(MotorInstance_t *m);

void             Motor_SetDirection(MotorInstance_t *m, MotorDirection_t dir);
void             Motor_SetDuty(MotorInstance_t *m, uint16 dutyPermille);
void             Motor_Brake(MotorInstance_t *m);
void             Motor_Coast(MotorInstance_t *m);

boolean          Motor_MoveStart(MotorInstance_t *m,
                                 sint32 ticks,
                                 uint16 duty,
                                 uint32 timeoutMs,
                                 sint32 toleranceTicks);

boolean          Motor_MoveStartEx(MotorInstance_t *m,
                                   sint32 ticks,
                                   uint16 duty,
                                   uint32 timeoutMs,
                                   sint32 toleranceTicks,
                                   boolean useBoost);

boolean          Motor_MoveToTick(MotorInstance_t *m,
                                  sint32 absoluteTargetTick,
                                  uint16 duty,
                                  uint32 timeoutMs,
                                  sint32 toleranceTicks);

boolean          Motor_MoveToTickEx(MotorInstance_t *m,
                                    sint32 absoluteTargetTick,
                                    uint16 duty,
                                    uint32 timeoutMs,
                                    sint32 toleranceTicks,
                                    boolean useBoost);

void             Motor_MoveService(MotorInstance_t *m);
boolean          Motor_IsBusy(MotorInstance_t *m);
MotorMoveState_t Motor_GetState(MotorInstance_t *m);
void             Motor_ClearState(MotorInstance_t *m);

#endif /* BASE_ENCODERMOTOR_H_ */
