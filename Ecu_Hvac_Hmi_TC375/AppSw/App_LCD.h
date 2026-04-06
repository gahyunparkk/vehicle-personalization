#ifndef LCD_H_
#define LCD_H_

#include "Base_I2C.h"

#ifndef LCDENUM
#define LCDENUM
typedef enum
{
  UPPERLINE,
  LOWERLINE
} LCD_line_e;
#endif

void App_Manager_LCD_Init(void);
void LCD_clearScreen(void);
void LCD_printString(char *str, LCD_line_e line);
void LCD_lightoff(void);
void LCD_lighton(void);

#endif
