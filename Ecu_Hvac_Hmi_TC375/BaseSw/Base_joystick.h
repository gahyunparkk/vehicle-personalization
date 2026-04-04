#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include "Ifx_Types.h"

#ifndef JOYSTICK_E_
#define JOYSTICK_E_
typedef enum
{
  JOY_NEUTRAL,
  JOY_UP,
  JOY_DOWN,
  JOY_LEFT,
  JOY_RIGHT
} joystick_dir_e;
#endif

void Joystick_init(void);
joystick_dir_e Joystick_read(void);
boolean Joystick_pushed(void);

#endif
