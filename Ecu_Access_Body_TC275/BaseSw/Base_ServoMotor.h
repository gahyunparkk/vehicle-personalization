#ifndef BASE_SERVOMOTOR_H_
#define BASE_SERVOMOTOR_H_

#include "IfxPort.h"
#include "IfxPort_PinMap.h"
#include "IfxGtm_Tom_Pwm.h"
#include "IfxGtm_Tom.h"
#include "IfxGtm_PinMap.h"
#include "IfxCpu.h"

/* ------------------------------------------------------------------
 * ShieldBuddy TC275 + Arduino Motor Shield R3
 * Servo signal -> OUT5 (D5)
 *
 * ShieldBuddy mapping:
 * D5 (PWM) = P2.3
 *
 * Current working mapping:
 * TOM1_11 -> P02.3
 * ------------------------------------------------------------------ */
#define SERVO_D5_PWM_PIN            (&IfxGtm_TOM1_11_TOUT3_P02_3_OUT)

/* ------------------------------------------------------------------
 * SG90 timing in TOM ticks
 *
 * working calibration for current setup:
 * period: 20 ms  -> 7813 ticks
 * pulse : 0.5 ms ->  195 ticks
 * 1.5 ms ->  586 ticks
 * 2.5 ms ->  977 ticks
 * ------------------------------------------------------------------ */
#define SERVO_PWM_PERIOD_TICKS      7813U
#define SERVO_PULSE_MIN_TICKS        195U
#define SERVO_PULSE_CENTER_TICKS     586U
#define SERVO_PULSE_MAX_TICKS        977U

#define SERVO_ANGLE_MIN               0U
#define SERVO_ANGLE_CENTER           90U
#define SERVO_ANGLE_MAX             180U

#define SERVO_ANGLE_SPAN            (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN)

/* 상태 머신 및 스텝 구조체 모두 제거, 현재 각도만 기억하도록 간소화 */
typedef struct
{
    IfxGtm_Tom_Pwm_Config pwmCfg;
    IfxGtm_Tom_Pwm_Driver pwmDrv;

    uint32 pwmPeriodTicks;
    uint16 minPulseTicks;
    uint16 centerPulseTicks;
    uint16 maxPulseTicks;

    sint16 currentAngleDeg;
} ServoInstance_t;

typedef struct
{
    const IfxGtm_Tom_ToutMap *pwmPin;

    uint32 pwmPeriodTicks;
    uint16 minPulseTicks;
    uint16 centerPulseTicks;
    uint16 maxPulseTicks;
    uint16 initAngleDeg;
} ServoConfig_t;

void             Servo_Init(ServoInstance_t *s, const ServoConfig_t *cfg);

void             Servo_SetPulseTicks(ServoInstance_t *s, uint16 pulseTicks);
void             Servo_SetAngle(ServoInstance_t *s, sint16 angleDeg);

uint16           Servo_GetPulseTicks(ServoInstance_t *s);
sint16           Servo_GetAngle(ServoInstance_t *s);

#endif /* BASE_SERVOMOTOR_H_ */
