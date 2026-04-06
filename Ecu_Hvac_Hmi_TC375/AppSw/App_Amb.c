/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Amb.h"
#include "Base_Neopixel.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
// 숨쉬기, 파도타기 최대/최소 밝기
#define MAXVAL 75
#define MINVAL 20

// 숨쉬기, 파도타기 밝기 변화량
#define DVAL_BR 2
#define DVAL_WA 5

// 변화 속도 분주비
#define PRESCALER 10

// 비상 모드 깜빡임 속도
#define BLINKPERIOD 10

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
static Amb_mode_e ambmode;
static int baseh, bases, basev;
static int nowv;
static boolean breathdesc;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_Ambient_Init(void);
void App_Ambient_Nextmode(void);
void App_Manager_Ambient_Run(void);
void App_Ambient_changeColor(sint8 amount);
void Amb_getmode(Amb_mode_e *mode);
void Amb_off(void);
void Amb_on(void);
void Amb_getHue(uint16 *hue);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_Ambient_Init(void)
{
  initNeopixel();
  ambmode = AMB_OFF;
  baseh = 0;
  bases = 90;
  basev = MAXVAL;
}

void App_Ambient_Nextmode(void)
{
  nowv = MAXVAL;
  breathdesc = TRUE;
  switch (ambmode)
  {
  case AMB_OFF:
    ambmode = AMB_CONSTANT;
    break;
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
    ambmode = AMB_OFF;
    break;
  }
}

void App_Manager_Ambient_Run(void)
{
  static int cnt = PRESCALER;
  static int blinkcnt = BLINKPERIOD;

  if (--cnt) return;
  cnt = PRESCALER;

  if (breathdesc)
  {
    if ((nowv -= (ambmode == AMB_BREATH ? DVAL_BR : DVAL_WA)) < MINVAL)
      breathdesc = FALSE, nowv = MINVAL;
  }
  else
  {
    if ((nowv += (ambmode == AMB_BREATH ? DVAL_BR : DVAL_WA)) > MAXVAL)
      breathdesc = TRUE, nowv = MAXVAL;
  }
  switch (ambmode)
  {
  case AMB_OFF:
    setAllLEDColorHSV(baseh, bases, 0);
    break;
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
  case AMB_BLINK:
    if (--blinkcnt)
      break;
    blinkcnt = BLINKPERIOD;
    nowv = nowv ? 0 : MAXVAL;
    setAllLEDColorHSV(baseh, bases, nowv);
    break;
  }

  transmitNeopixel();
}

void App_Ambient_changeColor(sint8 amount)
{
  baseh += amount + 360;
  baseh %= 360;
}

void Amb_getmode(Amb_mode_e *mode)
{
  *mode = ambmode;
}

void Amb_setmode(Amb_mode_e mode)
{
  ambmode = mode;
}

void Amb_setcolor2x(uint8 amount)
{
  baseh = 2 * (int)amount;
}

void Amb_off(void)
{
  basev = 0;
  ambmode = AMB_CONSTANT;
}

void Amb_on(void)
{
  basev = MAXVAL;
}

void Amb_getHue(uint16 *hue)
{
  *hue = (uint16)baseh;
}
