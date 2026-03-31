#include "Base_joystick.h"
#include "Base_EVADC.h"
#include "IfxPort.h"
#include "Ifx_PinMap.h"
#include "Ifx_Types.h"

#define JOYSW &MODULE_P40, 7

void Joystick_init(void)
{
  init_EVADC();
  IfxPort_setPinModeInput(JOYSW, IfxPort_InputMode_pullUp);
}

#define OKZONE_LOWER_L 50
#define OKZONE_LOWER_H 200
#define OKZONE_UPPER_L 3850
#define OKZONE_UPPER_H 4050

joystick_dir_e Joystick_read(void)
{
  uint16 x, y;
  static joystick_dir_e dirx = JOY_NEUTRAL;
  static joystick_dir_e diry = JOY_NEUTRAL;

  read_EVADC_Values(&x, &y);
  if (dirx == JOY_NEUTRAL)
  {
    if (x < OKZONE_LOWER_L) dirx = JOY_LEFT;
    else if (x > OKZONE_UPPER_H) dirx = JOY_RIGHT;
  }
  else if (dirx == JOY_LEFT)
  {
    if (x > OKZONE_LOWER_H) dirx = JOY_NEUTRAL;
  }
  else if (dirx == JOY_RIGHT)
  {
    if (x < OKZONE_UPPER_L) dirx = JOY_NEUTRAL;
  }

  if (diry == JOY_NEUTRAL)
  {
    if (y < OKZONE_LOWER_L) diry = JOY_UP;
    else if (y > OKZONE_UPPER_H) diry = JOY_DOWN;
  }
  else if (diry == JOY_UP)
  {
    if (y > OKZONE_LOWER_H) diry = JOY_NEUTRAL;
  }
  else if (diry == JOY_DOWN)
  {
    if (y < OKZONE_UPPER_L) diry = JOY_NEUTRAL;
  }

  if (dirx == JOY_NEUTRAL) return diry;
  if (diry == JOY_NEUTRAL) return dirx;
  return JOY_NEUTRAL;
}

boolean Joystick_pushed(void)
{
  return !IfxPort_getPinState(JOYSW);
}