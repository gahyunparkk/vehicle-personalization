#include "App_LCD.h"
#include "IfxPort.h"
#include "Ifx_Types.h"

static uint8 BACKLIGHT = 0x8;
#define ENABLE   0x4
#define READMODE 0x2
#define REGSEL   0x1

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

#define UPPER4BIT(b) ((b) & 0xf0)
#define LOWER4BIT(b) (((b) & 0x0f) << 4)

#include "Stm/Std/IfxStm.h"

/**
 * @brief 마이크로초(us) 단위 블로킹 딜레이 함수
 * @param microSeconds 대기할 마이크로초 시간
 */
static void delay_us(uint32 microSeconds)
{
  /* * 1. 시스템 타이머 모듈 선택
   * CPU0에서 실행되는 코드라면 MODULE_STM0을 사용합니다.
   * (예: CPU1인 경우 MODULE_STM1 사용)
   * 보드 지원 패키지(BSP)가 설정되어 있다면 BSP_DEFAULT_TIMER 매크로를 권장합니다.
   */
  Ifx_STM *stm = &MODULE_STM0;

  /* 2. 현재 타이머 클럭 주파수를 바탕으로 해당 us에 필요한 틱(Tick) 수 계산 */
  uint32 ticks = (uint32)IfxStm_getTicksFromMicroseconds(stm, microSeconds);

  /* 3. 계산된 틱 수만큼 대기 (Blocking) */
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

void LCD_init(void)
{
  LCD_init_static();
  LCD_init_static();
  LCD_init_static();
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