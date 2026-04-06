/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Hvac.h"
#include "Base_Fan.h"
#include "IfxPort.h"
#include "Ifx_PinMap.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
// LED 포트
#define BLUELED &MODULE_P10, 2
#define REDLED  &MODULE_P10, 1

// 임계값 설정 최대/최소치
#define MIN_H_TH 6
#define MAX_H_TH 22
#define MIN_C_TH 16
#define MAX_C_TH 30

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
volatile static sint8 heat_threshold;
volatile static sint8 cool_threshold;
volatile static sint8 currentTemp;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void App_Manaver_HVAC_Init(void);

// 온도 설정 함수
uint8 Hvac_setHeatThreshold(sint8 th);
uint8 Hvac_getHeatThreshold(void);
uint8 Hvac_setCoolThreshold(sint8 th);
uint8 Hvac_getCoolThreshold(void);
void App_Manager_Hvac_updateTemp(sint8 temp);

// 주기적으로 실행해야 할 함수
void App_Manager_HVAC_Run(void);

// 내부 라이브러리
static void turnonCooling(void);
static void turnonHeating(void);
static void turnoff(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void App_Manaver_HVAC_Init(void)
{
  IfxPort_setPinModeOutput(BLUELED, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinModeOutput(REDLED, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinHigh(BLUELED);
  IfxPort_setPinHigh(REDLED);
  heat_threshold = 18;
  cool_threshold = 26;
  Fan_init();
}

uint8 Hvac_setHeatThreshold(sint8 th)
{
  if (th < MIN_H_TH || th > MAX_H_TH || th + 2 >= cool_threshold)
    return 1;
  heat_threshold = th;
  return 0;
}

uint8 Hvac_getHeatThreshold(void)
{
  return heat_threshold;
}

uint8 Hvac_setCoolThreshold(sint8 th)
{
  if (th < MIN_C_TH || th > MAX_C_TH || th - 2 <= heat_threshold)
    return 1;
  cool_threshold = th;
  return 0;
}

uint8 Hvac_getCoolThreshold(void)
{
  return cool_threshold;
}

void App_Manager_HVAC_Run(void)
{
  if (currentTemp >= cool_threshold + 2)
    turnonCooling();
  else if (currentTemp <= heat_threshold - 2)
    turnonHeating();
  else if (currentTemp <= cool_threshold - 1 && currentTemp >= heat_threshold + 1)
    turnoff();
}

static void turnonCooling(void)
{
  IfxPort_setPinLow(BLUELED);
  Fan_setSpeed(80);
}

static void turnonHeating(void)
{
  IfxPort_setPinLow(REDLED);
  Fan_setSpeed(80);
}

static void turnoff(void)
{
  IfxPort_setPinHigh(BLUELED);
  IfxPort_setPinHigh(REDLED);
  Fan_setSpeed(0);
}

void App_Manager_Hvac_updateTemp(sint8 temp)
{
  currentTemp = temp;
}