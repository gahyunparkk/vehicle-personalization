#include "App_UI.h"

static void joyutask(void);
static void joydtask(void);
static void joyltask(void);
static void joyrtask(void);
static void swtask(void);

// TODO
static uint8 getCurrentProfile(void);
static void setCurrentProfile(uint8 prof);

static enum {
  ST_PROFILE_SEL,
  ST_AMB_COL_SEL,
  ST_AMB_MOD_SEL,
  ST_COOLTEM_SEL,
  ST_HEATTEM_SEL
} uistate;

static char coolline[] = "+  Over        -";
static char heatline[] = "+  Under       -";
static char profline[] = "+  Profile 0   -";
static uint8 profsel = 0;

void UI_init(void)
{
  LCD_init();
  LCD_clearScreen();
  Joystick_init();
  Amb_init();
  uistate = ST_PROFILE_SEL;
  coolline[11] = 0xDF;
  heatline[11] = 0xDF;
  coolline[12] = 'C';
  heatline[12] = 'C';
  profsel = getCurrentProfile();
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

  // LCD 출력
  static uint8 th = 0;
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    LCD_printString("\x7FProfile Select\x7E", UPPERLINE);
    profline[12] = profsel + '0';
    if (profsel == getCurrentProfile())
      profline[2] = '[', profline[13] = ']';
    else
      profline[2] = ' ', profline[13] = ' ';
    LCD_printString(profline, LOWERLINE);
    break;

  case ST_AMB_COL_SEL:
    LCD_printString("\x7F Amb Col Set  \x7E", UPPERLINE);
    LCD_printString("+    Color     -", LOWERLINE);
    break;

  case ST_AMB_MOD_SEL:
    LCD_printString("\x7F Amb Mode Set \x7E", UPPERLINE);
    switch (Amb_getmode())
    {
    case AMB_CONSTANT:
      LCD_printString("+   Constant   -", LOWERLINE);
      break;
    case AMB_BREATH:
      LCD_printString("+    Breath    -", LOWERLINE);
      break;
    case AMB_WAVE_L:
      LCD_printString("+  Wave(left)  -", LOWERLINE);
      break;
    case AMB_WAVE_R:
      LCD_printString("+  Wave(Right) -", LOWERLINE);
      break;
    }
    break;

  case ST_COOLTEM_SEL:
    LCD_printString("\x7F Cooling When \x7E", UPPERLINE);
    th = Hvac_getCoolThreshold();
    coolline[9] = (th < 10 ? ' ' : (th / 10) + '0');
    coolline[10] = (th % 10 + '0');
    LCD_printString(coolline, LOWERLINE);
    break;

  case ST_HEATTEM_SEL:
    LCD_printString("\x7F Heating When \x7E", UPPERLINE);
    th = Hvac_getHeatThreshold();
    heatline[9] = (th < 10 ? ' ' : (th / 10) + '0');
    heatline[10] = (th % 10 + '0');
    LCD_printString(heatline, LOWERLINE);
    break;
  }
}

static void joyutask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    if (--profsel == 0) profsel = 5;
    break;
  case ST_AMB_COL_SEL:
    Amb_changeColor(-20);
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
    if (++profsel == 6) profsel = 1;
    break;
  case ST_AMB_COL_SEL:
    Amb_changeColor(20);
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
  if (uistate == ST_PROFILE_SEL)
  {
    setCurrentProfile(profsel);
  }
  if (uistate == ST_AMB_COL_SEL)
  {
    LCD_lightoff();
    Amb_off();
  }
  if (uistate == ST_AMB_MOD_SEL)
  {
    LCD_lighton();
    Amb_on();
  }
}

static uint8 cp = 1;
static uint8 getCurrentProfile(void) // TODO
{
  return cp;
}

static void setCurrentProfile(uint8 prof) // TODO
{
  cp = prof;
}
