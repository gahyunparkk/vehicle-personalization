#include "App_UI.h"

static void joyutask(void);
static void joydtask(void);
static void joyltask(void);
static void joyrtask(void);
static void swtask(void);

static enum {
  ST_PROFILE_SEL,
  ST_AMB_COL_SEL,
  ST_AMB_MOD_SEL,
  ST_COOLTEM_SEL,
  ST_HEATTEM_SEL
} uistate;

void UI_init(void)
{
  uistate = ST_AMB_COL_SEL;
}

void UI_task(void)
{
  // 조이스틱 인식
  static joystick_dir_e prev = JOY_NEUTRAL, res = JOY_NEUTRAL;
  res = Joystick_read();
  if (res != prev)
  {
    switch (res)
    {
    case JOY_UP:
      joyutask();
      break;
    case JOY_DOWN:
      joydtask();
      break;
    case JOY_LEFT:
      joyltask();
      break;
    case JOY_RIGHT:
      joyrtask();
      break;
    default:;
      // LCD_printString("NEUTRAL", UPPERLINE);
    }
  }
  prev = res;

  // 스위치 인식
  static uint8 pushcnt = 0;
  if (Joystick_pushed())
  {
    if (++pushcnt > 3) pushcnt = 4;
    if (pushcnt == 3) swtask();
  }
  else
  {
    pushcnt = 0;
  }

  // 앰비언트 틱 진행
  Amb_transition();

  // LCD 출력
  static char lowline[] = "+              -";
  static uint8 th = 0;
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    LCD_printString("<PROFILE SELECT>", UPPERLINE);
    LCD_printString("+  PROFILE 01  -", LOWERLINE);
    break;
  case ST_AMB_COL_SEL:
    LCD_printString("<AMB COLOR SET >", UPPERLINE);
    LCD_printString("+    COLOR     -", LOWERLINE);
    break;
  case ST_AMB_MOD_SEL:
    LCD_printString("< AMB MODE SET >", UPPERLINE);
    switch (Amb_getmode())
    {
    case AMB_CONSTANT:
      LCD_printString("+   CONSTANT   -", LOWERLINE);
      break;
    case AMB_BREATH:
      LCD_printString("+    BREATH    -", LOWERLINE);
      break;
    case AMB_WAVE_L:
      LCD_printString("+  WAVE(LEFT)  -", LOWERLINE);
      break;
    case AMB_WAVE_R:
      LCD_printString("+  WAVE(RIGHT) -", LOWERLINE);
      break;
    }
    break;
  case ST_COOLTEM_SEL:
    LCD_printString("< COOLING TEMP >", UPPERLINE);
    th = Hvac_getCoolThreshold();
    lowline[7] = (th < 10 ? ' ' : (th / 10) + '0');
    lowline[8] = (th % 10 + '0');
    LCD_printString(lowline, LOWERLINE);
    break;
  case ST_HEATTEM_SEL:
    LCD_printString("< HEATING TEMP >", UPPERLINE);
    th = Hvac_getHeatThreshold();
    lowline[7] = (th < 10 ? ' ' : (th / 10) + '0');
    lowline[8] = (th % 10 + '0');
    LCD_printString(lowline, LOWERLINE);
    break;
  }
}

static void joyutask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    break;
  case ST_AMB_COL_SEL:
    Amb_changeColor(-30);
    break;
  case ST_AMB_MOD_SEL:
    Amb_nextmode();
    Amb_nextmode();
    Amb_nextmode();
    break;
  case ST_COOLTEM_SEL:
    Hvac_setCoolThreshold(Hvac_getCoolThreshold() + 1);
    break;
  case ST_HEATTEM_SEL:
    Hvac_setHeatThreshold(Hvac_getHeatThreshold() + 1);
    break;
  }
}

static void joydtask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    break;
  case ST_AMB_COL_SEL:
    Amb_changeColor(30);
    break;
  case ST_AMB_MOD_SEL:
    Amb_nextmode();
    break;
  case ST_COOLTEM_SEL:
    Hvac_setCoolThreshold(Hvac_getCoolThreshold() - 1);
    break;
  case ST_HEATTEM_SEL:
    Hvac_setHeatThreshold(Hvac_getHeatThreshold() - 1);
    break;
  }
}

static void joyltask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    uistate = ST_HEATTEM_SEL;
    break;
  case ST_AMB_COL_SEL:
    uistate = ST_PROFILE_SEL;
    break;
  case ST_AMB_MOD_SEL:
    uistate = ST_AMB_COL_SEL;
    break;
  case ST_COOLTEM_SEL:
    uistate = ST_AMB_MOD_SEL;
    break;
  case ST_HEATTEM_SEL:
    uistate = ST_COOLTEM_SEL;
    break;
  }
}

static void joyrtask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    uistate = ST_AMB_COL_SEL;
    break;
  case ST_AMB_COL_SEL:
    uistate = ST_AMB_MOD_SEL;
    break;
  case ST_AMB_MOD_SEL:
    uistate = ST_COOLTEM_SEL;
    break;
  case ST_COOLTEM_SEL:
    uistate = ST_HEATTEM_SEL;
    break;
  case ST_HEATTEM_SEL:
    uistate = ST_PROFILE_SEL;
    break;
  }
}

static void swtask(void)
{
}