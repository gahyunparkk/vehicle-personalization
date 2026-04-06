/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_LCD.h"
#include "IfxPort.h"
#include "Ifx_Types.h"
#include "Stm/Std/IfxStm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
// 명령어 비트필드
#define ENABLE   0x4
#define READMODE 0x2
#define REGSEL   0x1

// 명령어
#define OP_FUNCSET   (uint8)0x28
#define OP_RESET     (uint8)0x30
#define OP_4BIT      (uint8)0x20
#define OP_CLEAR     (uint8)0x01
#define OP_OFF       (uint8)0x08
#define OP_MODESET   (uint8)0x06
#define OP_CURSOROFF (uint8)0x0C
#define OP_UPPERLINE (uint8)0x80
#define OP_LOWERLINE (uint8)0xC0
#define OP_RETURN    (uint8)0x02

// 비트 추출
#define UPPER4BIT(b) ((b) & 0xf0)
#define LOWER4BIT(b) (((b) & 0x0f) << 4)

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
// 백라이트 비트필드
static uint8 BACKLIGHT = 0x8;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void delay_us(uint32 microSeconds);
static void writechar(char ch);
static void writeoperation4bit(uint8 op);
static void writeoperation(uint8 op);
static void LCD_init_static(void);

void App_Manager_LCD_Init(void);
void LCD_clearScreen(void);
void LCD_printString(char *str, LCD_line_e line);
void LCD_lightoff(void);
void LCD_lighton(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
static void delay_us(uint32 microSeconds)
{
  Ifx_STM *stm = &MODULE_STM0;
  uint32 ticks = (uint32)IfxStm_getTicksFromMicroseconds(stm, microSeconds);
  IfxStm_waitTicks(stm, ticks);
}

static void writechar(char ch)
{
  if (!BACKLIGHT) return;
  I2C_writeSingleByte(UPPER4BIT(ch) | BACKLIGHT | ENABLE | REGSEL);
  delay_us(1);
  I2C_writeSingleByte(UPPER4BIT(ch) | BACKLIGHT | REGSEL);
  delay_us(1);
  I2C_writeSingleByte(LOWER4BIT(ch) | BACKLIGHT | ENABLE | REGSEL);
  delay_us(1);
  I2C_writeSingleByte(LOWER4BIT(ch) | BACKLIGHT | REGSEL);
  delay_us(1);
}

static void writeoperation4bit(uint8 op)
{
  I2C_writeSingleByte(LOWER4BIT(op) | BACKLIGHT | ENABLE);
  delay_us(1);
  I2C_writeSingleByte(LOWER4BIT(op) | BACKLIGHT);
  delay_us(100);
}

static void writeoperation(uint8 op)
{
  I2C_writeSingleByte(UPPER4BIT(op) | BACKLIGHT | ENABLE);
  delay_us(1);
  I2C_writeSingleByte(UPPER4BIT(op) | BACKLIGHT);
  delay_us(1);
  I2C_writeSingleByte(LOWER4BIT(op) | BACKLIGHT | ENABLE);
  delay_us(1);
  I2C_writeSingleByte(LOWER4BIT(op) | BACKLIGHT);
  delay_us(1);
}

static void LCD_init_static(void)
{
  init_I2C_module(), delay_us(50000);

  // 4비트 모드임을 강제로 인식시키기 위한 명령
  writeoperation4bit(OP_RESET), delay_us(5000);
  writeoperation4bit(OP_RESET), delay_us(200);
  writeoperation4bit(OP_RESET), delay_us(200);

  // 4비트 모드 설정
  writeoperation4bit(OP_4BIT), delay_us(200);

  // 초기 설정
  writeoperation(OP_FUNCSET), delay_us(70);
  writeoperation(OP_OFF), delay_us(70);
  writeoperation(OP_CLEAR), delay_us(2000);
  writeoperation(OP_MODESET), delay_us(70);
  writeoperation(OP_CURSOROFF), delay_us(70);
  writeoperation(OP_UPPERLINE), delay_us(70);
  writeoperation(OP_RETURN), delay_us(70);
}

void App_Manager_LCD_Init(void)
{
  LCD_init_static(), delay_us(1000);
  LCD_init_static(), delay_us(1000);
  LCD_init_static(), delay_us(1000);
}

void LCD_clearScreen(void)
{
  writeoperation(OP_CLEAR), delay_us(2000);
}

void LCD_printString(char *str, LCD_line_e line)
{
  if (line == UPPERLINE) writeoperation(OP_UPPERLINE), delay_us(70);
  else writeoperation(OP_LOWERLINE), delay_us(70);
  while (*str)
    writechar(*(str++));
}

void LCD_lightoff(void)
{
  BACKLIGHT = 0x0;
  LCD_clearScreen();
}

void LCD_lighton(void)
{
  BACKLIGHT = 0x8;
}