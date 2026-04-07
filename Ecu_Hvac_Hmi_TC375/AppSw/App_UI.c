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
#define SW_LONGPRESS_10MS  200

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

static char lowline[] = "    /+         -";
static char lowlinebuf[17];
static char coolline[] = "Over     ";
static char heatline[] = "Undr     ";
static char profline[] = " Prof  0 ";
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
static void bufwrite(char *str);

void App_Manager_UI_Init(void);
void App_Manager_UI_Run(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
static void bufwrite(char *str)
{
  for (int i = 0; i < 17; i++) lowlinebuf[i] = lowline[i];
  for (int i = 0; str[i]; i++) lowlinebuf[i + 6] = str[i];
}

void App_Manager_UI_Init(void)
{
  App_Manager_LCD_Init();
  LCD_clearScreen();
  LCD_printString("Wating for Msg..", LOWERLINE);
  Joystick_init();
  App_Manager_Ambient_Init();
  Amb_on();
  uistate = ST_PROFILE_SEL;
  lowline[2] = 0xDF;
  lowline[3] = 'C';
  coolline[7] = 0xDF;
  heatline[7] = 0xDF;
  coolline[8] = 'C';
  heatline[8] = 'C';
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
  static uint8 temp;
  static uint8 nowprof;
  temp = App_Manager_Hvac_getTemp();
  lowline[0] = (temp < 10 ? ' ' : (temp / 10) + '0');
  lowline[1] = (temp % 10 + '0');

  switch (uistate)
  {
  case ST_PROFILE_SEL:
    LCD_printString("\x7FProfile Select\x7E", UPPERLINE);
    profline[6] = profsel + '1';
    App_Manager_System_GetActiveProfileIndex(&nowprof);
    if (profsel == nowprof)
      profline[0] = '[', profline[7] = ']';
    else
      profline[0] = ' ', profline[7] = ' ';
    bufwrite(profline);
    LCD_printString(lowlinebuf, LOWERLINE);
    break;

  case ST_AMB_COL_SEL:
    LCD_printString("\x7F Amb Col Set  \x7E", UPPERLINE);
    bufwrite("  Color  ");
    LCD_printString(lowlinebuf, LOWERLINE);
    break;

  case ST_AMB_MOD_SEL:
    LCD_printString("\x7F Amb Mode Set \x7E", UPPERLINE);
    Amb_mode_e ambmode;
    Amb_getmode(&ambmode);
    switch (ambmode)
    {
    case AMB_OFF:
      bufwrite("   Off   ");
      LCD_printString(lowlinebuf, LOWERLINE);
      break;
    case AMB_CONSTANT:
      bufwrite("Constant ");
      LCD_printString(lowlinebuf, LOWERLINE);
      break;
    case AMB_BREATH:
      bufwrite(" Breath  ");
      LCD_printString(lowlinebuf, LOWERLINE);
      break;
    case AMB_WAVE_L:
      bufwrite(" Wave(1) ");
      LCD_printString(lowlinebuf, LOWERLINE);
      break;
    case AMB_WAVE_R:
      bufwrite(" Wave(2) ");
      LCD_printString(lowlinebuf, LOWERLINE);
      break;
    }
    break;

  case ST_COOLTEM_SEL:
    LCD_printString("\x7F Cooling When \x7E", UPPERLINE);
    th = Hvac_getCoolThreshold();
    coolline[5] = (th < 10 ? ' ' : (th / 10) + '0');
    coolline[6] = (th % 10 + '0');
    bufwrite(coolline);
    LCD_printString(lowlinebuf, LOWERLINE);
    break;

  case ST_HEATTEM_SEL:
    LCD_printString("\x7F Heating When \x7E", UPPERLINE);
    th = Hvac_getHeatThreshold();
    heatline[5] = (th < 10 ? ' ' : (th / 10) + '0');
    heatline[6] = (th % 10 + '0');
    bufwrite(heatline);
    LCD_printString(lowlinebuf, LOWERLINE);
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
  newprofile.ambient_light = (ambmode << 8 | hue >> 1);
  newprofile.heater_on_threshold = Hvac_getHeatThreshold();
  newprofile.ac_on_threshold = Hvac_getCoolThreshold();
  App_Manager_System_SetProfile(profidx, &newprofile);

  App_Scheduler_TableTxReq();
}