#include <App_Manger_DoorActuator.h>

static sint16 doorClampAngle(const DoorActuator_t *d, sint16 angleDeg)
{
    sint16 low  = d->closeAngleDeg;
    sint16 high = d->openAngleDeg;

    if (low > high)
    {
        sint16 tmp = low;
        low = high;
        high = tmp;
    }

    if (angleDeg < low)  angleDeg = low;
    if (angleDeg > high) angleDeg = high;
    return angleDeg;
}

void DoorActuator_Init(DoorActuator_t *d, const DoorActuatorConfig_t *cfg)
{
    d->servo         = cfg->servo;
    d->closeAngleDeg = cfg->closeAngleDeg;
    d->openAngleDeg  = cfg->openAngleDeg;

    DoorActuator_SetAngle(d, cfg->initAngleDeg);
}

void DoorActuator_Open(DoorActuator_t *d)
{
    Servo_SetAngle(d->servo, d->openAngleDeg);
    d->state = DOOR_STATE_OPEN;
}

void DoorActuator_Close(DoorActuator_t *d)
{
    Servo_SetAngle(d->servo, d->closeAngleDeg);
    d->state = DOOR_STATE_CLOSED;
}

void DoorActuator_SetOpenRatio(DoorActuator_t *d, uint8 percent)
{
    sint32 span;
    sint32 angle;

    if (percent > 100U) percent = 100U;

    span  = (sint32)d->openAngleDeg - (sint32)d->closeAngleDeg;
    angle = (sint32)d->closeAngleDeg + ((span * percent) / 100);

    DoorActuator_SetAngle(d, (sint16)angle);
}

void DoorActuator_SetAngle(DoorActuator_t *d, sint16 angleDeg)
{
    angleDeg = doorClampAngle(d, angleDeg);
    Servo_SetAngle(d->servo, angleDeg);

    if (angleDeg == d->closeAngleDeg)      d->state = DOOR_STATE_CLOSED;
    else if (angleDeg == d->openAngleDeg)  d->state = DOOR_STATE_OPEN;
    else                                   d->state = DOOR_STATE_PARTIAL;
}

DoorState_t DoorActuator_GetState(DoorActuator_t *d)
{
    return d->state;
}

sint16 DoorActuator_GetAngle(DoorActuator_t *d)
{
    return Servo_GetAngle(d->servo);
}
