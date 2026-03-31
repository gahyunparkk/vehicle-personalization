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

#define OKZONE_LOWER   50
#define OKZONE_UPPER   4050
#define DEADZONE_LOWER 1300
#define DEADZONE_UPPER 2800

joystick_dir_e Joystick_read(void)
{
  uint16 x, y;
  read_EVADC_Values(&x, &y);
  if (y > DEADZONE_LOWER && y < DEADZONE_UPPER)
  {
    if (x < OKZONE_LOWER)
      return JOY_LEFT;
    else if (x > OKZONE_UPPER)
      return JOY_RIGHT;
  }
  else if (x > DEADZONE_LOWER && x < DEADZONE_UPPER)
  {
    if (y < OKZONE_LOWER)
      return JOY_UP;
    else if (y > OKZONE_UPPER)
      return JOY_DOWN;
  }
  return JOY_NEUTRAL;
}

boolean Joystick_pushed(void)
{
  return !IfxPort_getPinState(JOYSW);
}