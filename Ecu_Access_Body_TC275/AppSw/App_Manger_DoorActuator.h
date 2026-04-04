#ifndef APP_MANGER_DOORACTUATOR_H_
#define APP_MANGER_DOORACTUATOR_H_

#include "Base_ServoMotor.h"

typedef enum
{
    DOOR_STATE_CLOSED = 0,
    DOOR_STATE_OPEN,
    DOOR_STATE_PARTIAL
} DoorState_t;

typedef struct
{
    ServoInstance_t *servo;
    sint16           closeAngleDeg;
    sint16           openAngleDeg;
    DoorState_t      state;
} DoorActuator_t;

typedef struct
{
    ServoInstance_t *servo;
    sint16           closeAngleDeg;
    sint16           openAngleDeg;
    sint16           initAngleDeg;
} DoorActuatorConfig_t;

void        DoorActuator_Init(DoorActuator_t *d, const DoorActuatorConfig_t *cfg);
void        DoorActuator_Open(DoorActuator_t *d);
void        DoorActuator_Close(DoorActuator_t *d);
void        DoorActuator_SetOpenRatio(DoorActuator_t *d, uint8 percent);
void        DoorActuator_SetAngle(DoorActuator_t *d, sint16 angleDeg);
DoorState_t DoorActuator_GetState(DoorActuator_t *d);
sint16      DoorActuator_GetAngle(DoorActuator_t *d);

#endif /* APP_MANGER_DOORACTUATOR_H_ */
