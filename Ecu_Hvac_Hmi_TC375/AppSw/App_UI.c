/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_UI.h"
#include "App_Manager_System.h"
#include "App_Scheduler.h"
#include "Shared_Profile.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define SW_SHORTPRESS_10MS 3
#define SW_LONGPRESS_10MS  100

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
static enum {
  ST_PROFILE_SEL,
  ST_AMB_COL_SEL,
  ST_AMB_MOD_SEL,
  ST_COOLTEM_SEL,
  ST_HEATTEM_SEL,
  ST_PROFILE_ADD
} uistate;

static char coolline[] = "+  Over        -";
static char heatline[] = "+  Under       -";
static char profline[] = "+  Profile 0   -";
static uint8 profsel = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void joyutask(void);
static void joydtask(void);
static void joyltask(void);
static void joyrtask(void);
static void sw_shortpress(void);
static void sw_longpress(void);
static void profupdate(void);

void App_Manager_UI_Init(void);
void App_Manager_UI_Run(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void App_Manager_UI_Init(void)
{
  App_Manager_LCD_Init();
  LCD_clearScreen();
  LCD_printString("Inited", LOWERLINE);
  Joystick_init();
  App_Manager_Ambient_Init();
  Amb_on();
  uistate = ST_PROFILE_SEL;
  coolline[11] = 0xDF;
  heatline[11] = 0xDF;
  coolline[12] = 'C';
  heatline[12] = 'C';
  App_Manager_System_GetActiveProfileIndex(&profsel);
}

void App_Manager_UI_Run(void)
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
    if (++pushcnt > SW_LONGPRESS_10MS)
      pushcnt = SW_LONGPRESS_10MS;
    if (pushcnt == SW_LONGPRESS_10MS - 1)
      sw_longpress();
  }
  else
  {
    if (pushcnt >= SW_SHORTPRESS_10MS && pushcnt < SW_LONGPRESS_10MS)
      sw_shortpress();
    pushcnt = 0;
  }

  // LCD 출력
  static uint8 th = 0;
  static uint8 nowprof;
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    LCD_printString("\x7FProfile Select\x7E", UPPERLINE);
    profline[12] = profsel + '1';
    App_Manager_System_GetActiveProfileIndex(&nowprof);
    if (profsel == nowprof)
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
    Amb_mode_e ambmode;
    Amb_getmode(&ambmode);
    switch (ambmode)
    {
    case AMB_OFF:
      LCD_printString("+     Off      -", LOWERLINE);
      break;
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

  case ST_PROFILE_ADD:
    LCD_printString("  Register User ", UPPERLINE);
    LCD_printString("  Tag New RFID  ", LOWERLINE);
    break;
  }
}

static void joyutask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    if (profsel-- == 0) profsel = 2;
    break;
  case ST_AMB_COL_SEL:
    App_Ambient_changeColor(-20);
    profupdate();
    break;
  case ST_AMB_MOD_SEL:
    App_Ambient_Nextmode();
    App_Ambient_Nextmode();
    App_Ambient_Nextmode();
    App_Ambient_Nextmode();
    profupdate();
    break;
  case ST_COOLTEM_SEL:
    Hvac_setCoolThreshold(Hvac_getCoolThreshold() + 1);
    profupdate();
    break;
  case ST_HEATTEM_SEL:
    Hvac_setHeatThreshold(Hvac_getHeatThreshold() + 1);
    profupdate();
    break;
  default:
    break;
  }
}

static void joydtask()
{
  switch (uistate)
  {
  case ST_PROFILE_SEL:
    if (profsel++ == 2) profsel = 0;
    break;
  case ST_AMB_COL_SEL:
    App_Ambient_changeColor(20);
    profupdate();
    break;
  case ST_AMB_MOD_SEL:
    App_Ambient_Nextmode();
    profupdate();
    break;
  case ST_COOLTEM_SEL:
    Hvac_setCoolThreshold(Hvac_getCoolThreshold() - 1);
    profupdate();
    break;
  case ST_HEATTEM_SEL:
    Hvac_setHeatThreshold(Hvac_getHeatThreshold() - 1);
    profupdate();
    break;
  default:
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
  default:
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
  default:
    break;
  }
}

static void sw_shortpress(void)
{
  if (uistate == ST_PROFILE_SEL)
  {
    App_Manager_System_SetActiveProfileIndex(profsel);
    App_Scheduler_IdxTxReq();
  }
  if (uistate == ST_AMB_COL_SEL)
  {
    // LCD_lightoff();
    // Amb_off();
  }
  if (uistate == ST_AMB_MOD_SEL)
  {
    // LCD_lighton();
    // Amb_on();
  }
}

static void sw_longpress(void)
{
  if (uistate != ST_PROFILE_ADD)
  {
    uistate = ST_PROFILE_ADD;
    // 프로필 추가 요청
  }
  else
    uistate = ST_PROFILE_SEL;
}

static void profupdate(void)
{
  uint8 profidx;
  Shared_Profile_t newprofile;
  Amb_mode_e ambmode;
  uint16 hue;

  App_Manager_System_GetActiveProfileIndex(&profidx);
  App_Manager_System_GetProfile(profidx, &newprofile);
  Amb_getmode(&ambmode);
  Amb_getHue(&hue);
  hue >>= 1;
  newprofile.ambient_light = (ambmode << 4 | hue);
  newprofile.heater_on_threshold = Hvac_getHeatThreshold();
  newprofile.ac_on_threshold = Hvac_getCoolThreshold();
  App_Manager_System_SetProfile(profidx, &newprofile);

  App_Scheduler_TableTxReq();
}