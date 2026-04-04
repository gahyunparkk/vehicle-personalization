#include "Base_ServoMotor.h"
#include "IfxGtm.h"

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */
static uint16 servoClampPulseTicks(const ServoInstance_t *s, uint16 pulseTicks)
{
    if (pulseTicks < s->minPulseTicks) pulseTicks = s->minPulseTicks;
    if (pulseTicks > s->maxPulseTicks) pulseTicks = s->maxPulseTicks;
    return pulseTicks;
}

static sint16 servoClampAngle(sint16 angleDeg)
{
    if (angleDeg < SERVO_ANGLE_MIN) angleDeg = SERVO_ANGLE_MIN;
    if (angleDeg > SERVO_ANGLE_MAX) angleDeg = SERVO_ANGLE_MAX;
    return angleDeg;
}

static uint16 servoAngleToPulseTicks(const ServoInstance_t *s, sint16 angleDeg)
{
    uint32 pulseSpan;
    uint32 pulseTicks;

    angleDeg = servoClampAngle(angleDeg);
    pulseSpan = (uint32)(s->maxPulseTicks - s->minPulseTicks);

    pulseTicks = (uint32)s->minPulseTicks + ((uint32)(angleDeg - SERVO_ANGLE_MIN) * pulseSpan) / SERVO_ANGLE_SPAN;
    return (uint16)pulseTicks;
}

static sint16 servoPulseTicksToAngle(const ServoInstance_t *s, uint16 pulseTicks)
{
    uint32 pulseSpan;
    uint32 angleDeg;

    pulseTicks = servoClampPulseTicks(s, pulseTicks);
    pulseSpan = (uint32)(s->maxPulseTicks - s->minPulseTicks);

    if (pulseSpan == 0U) return SERVO_ANGLE_MIN;

    angleDeg = ((uint32)(pulseTicks - s->minPulseTicks) * SERVO_ANGLE_SPAN) / pulseSpan;
    angleDeg += SERVO_ANGLE_MIN;

    if (angleDeg > SERVO_ANGLE_MAX) angleDeg = SERVO_ANGLE_MAX;

    return (sint16)angleDeg;
}

/* Shadow compare update: same design style as encoder motor */
static void servoApplyPulseRaw(ServoInstance_t *s, uint16 ticks)
{
    if (ticks > s->pwmCfg.period) ticks = (uint16)s->pwmCfg.period;

    s->pwmCfg.dutyCycle = ticks;
    IfxGtm_Tom_Ch_setCompareOneShadow(s->pwmDrv.tom, s->pwmDrv.tomChannel, ticks);
    IfxGtm_Tom_Tgc_trigger(s->pwmDrv.tgc[0]);
}

/* ------------------------------------------------------------------
 * Init
 * ------------------------------------------------------------------ */
void Servo_Init(ServoInstance_t *s, const ServoConfig_t *cfg)
{
    s->pwmPeriodTicks   = cfg->pwmPeriodTicks;
    s->minPulseTicks    = cfg->minPulseTicks;
    s->centerPulseTicks = cfg->centerPulseTicks;
    s->maxPulseTicks    = cfg->maxPulseTicks;

    s->currentAngleDeg  = servoClampAngle((sint16)cfg->initAngleDeg);

    IfxGtm_Tom_Pwm_initConfig(&s->pwmCfg, &MODULE_GTM);
    s->pwmCfg.tom                      = cfg->pwmPin->tom;
    s->pwmCfg.tomChannel               = cfg->pwmPin->channel;
    s->pwmCfg.clock                    = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk2;
    s->pwmCfg.pin.outputPin            = cfg->pwmPin;
    s->pwmCfg.pin.outputMode           = IfxPort_OutputMode_pushPull;
    s->pwmCfg.pin.padDriver            = IfxPort_PadDriver_cmosAutomotiveSpeed1;
    s->pwmCfg.synchronousUpdateEnabled = TRUE;
    s->pwmCfg.immediateStartEnabled    = TRUE;
    s->pwmCfg.period                   = s->pwmPeriodTicks;
    s->pwmCfg.dutyCycle                = servoAngleToPulseTicks(s, s->currentAngleDeg);
    s->pwmCfg.signalLevel              = Ifx_ActiveState_high;

    IfxGtm_Tom_Pwm_init(&s->pwmDrv, &s->pwmCfg);
    IfxGtm_Tom_Pwm_start(&s->pwmDrv, TRUE);
}

/* ------------------------------------------------------------------
 * Basic control
 * ------------------------------------------------------------------ */
void Servo_SetPulseTicks(ServoInstance_t *s, uint16 pulseTicks)
{
    pulseTicks = servoClampPulseTicks(s, pulseTicks);
    s->currentAngleDeg = servoPulseTicksToAngle(s, pulseTicks);

    servoApplyPulseRaw(s, pulseTicks);
}

void Servo_SetAngle(ServoInstance_t *s, sint16 angleDeg)
{
    uint16 pulseTicks;

    angleDeg = servoClampAngle(angleDeg);
    s->currentAngleDeg = angleDeg;

    pulseTicks = servoAngleToPulseTicks(s, angleDeg);
    servoApplyPulseRaw(s, pulseTicks);
}

uint16 Servo_GetPulseTicks(ServoInstance_t *s)
{
    return servoAngleToPulseTicks(s, s->currentAngleDeg);
}

sint16 Servo_GetAngle(ServoInstance_t *s)
{
    return s->currentAngleDeg;
}
