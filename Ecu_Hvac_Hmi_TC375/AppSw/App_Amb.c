#include "App_Amb.h"
#include "Base_Neopixel.h"

#define MAXVAL 75
#define MINVAL 20
#define DVAL   5

static Amb_mode_e ambmode;
static int baseh, bases, basev;
static int nowv;
static boolean breathdesc;

void Amb_init(void)
{
  initNeopixel();
  ambmode = AMB_CONSTANT;
  baseh = 0;
  bases = 90;
  basev = MAXVAL;
}

void Amb_nextmode(void)
{
  nowv = MAXVAL;
  breathdesc = TRUE;
  switch (ambmode)
  {
  case AMB_CONSTANT:
    ambmode = AMB_BREATH;
    break;
  case AMB_BREATH:
    setAllLEDColorHSV(baseh, bases, MINVAL);
    ambmode = AMB_WAVE_L;
    break;
  case AMB_WAVE_L:
    setAllLEDColorHSV(baseh, bases, MINVAL);
    ambmode = AMB_WAVE_R;
    break;
  case AMB_WAVE_R:
    ambmode = AMB_CONSTANT;
    break;
  }
}

void Amb_transition(void)
{
  if (breathdesc)
  {
    if ((nowv -= DVAL) < MINVAL)
      breathdesc = FALSE, nowv = MINVAL;
  }
  else
  {
    if ((nowv += DVAL) > MAXVAL)
      breathdesc = TRUE, nowv = MAXVAL;
  }
  switch (ambmode)
  {
  case AMB_CONSTANT:
    setAllLEDColorHSV(baseh, bases, basev);
    break;
  case AMB_BREATH:
    setAllLEDColorHSV(baseh, bases, nowv);
    break;
  case AMB_WAVE_L:
    shiftLedsBackwardHSV(baseh, bases, nowv);
    break;
  case AMB_WAVE_R:
    shiftLedsForwardHSV(baseh, bases, nowv);
    break;
  }

  transmitNeopixel();
}

void Amb_changeColor(sint8 amount)
{
  baseh += amount + 360;
  baseh %= 360;
}