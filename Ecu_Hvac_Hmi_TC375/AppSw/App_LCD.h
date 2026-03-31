#ifndef LCD_H_
#define LCD_H_

#include "I2C.h"

#ifndef LCDENUM
#define LCDENUM
typedef enum
{
  UPPERLINE,
  LOWERLINE
} LCD_line_e;
#endif

void LCD_init(void);
void LCD_clearScreen(void);
void LCD_printString(char *str, LCD_line_e line);

#endif