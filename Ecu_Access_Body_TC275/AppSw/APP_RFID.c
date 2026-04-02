/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_RFID.h"
#include "IfxPort.h"
#include "Bsp.h"
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_SUCCESS_PORT      (&MODULE_P00)
#define LED_SUCCESS_PIN       5

#define LED_FAIL_PORT         (&MODULE_P00)
#define LED_FAIL_PIN          6

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/


/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/


/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Led_Init(void);
static void App_LedSuccess_On(void);
static void App_LedSuccess_Off(void);
static void App_LedFail_On(void);
static void App_LedFail_Off(void);
static void App_LedAll_Off(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void App_Init(RC522_Context_t *ctx)
{
    App_Led_Init();
    RC522_Fsm_Init(ctx, 0U);
}


void App_ApplyRfidLed(const RC522_Context_t *ctx)
{
    RC522_LedPattern_t pattern;

    if (ctx == NULL_PTR)
    {
        App_LedAll_Off();
        return;
    }

    pattern = RC522_Fsm_GetLedPattern(ctx);

    switch (pattern)
    {
    case RC522_LED_SUCCESS:
        App_LedSuccess_On();
        App_LedFail_Off();
        break;

    case RC522_LED_FAIL:
        App_LedSuccess_Off();
        App_LedFail_On();
        break;

    case RC522_LED_LOCKOUT:
        App_LedSuccess_Off();
        App_LedFail_On();
        break;

    case RC522_LED_OFF:
    default:
        App_LedAll_Off();
        break;
    }
}


void App_HandleRfidEvents(RC522_Context_t *ctx)
{
    const Rc522_Uid *uid;

    if (ctx == NULL_PTR)
    {
        return;
    }

    if (RC522_Fsm_ConsumeSuccessEvent(ctx) != FALSE)
    {
        uid = RC522_Fsm_GetConfirmedUid(ctx);

        /* 성공 시 처리 */
        /* 예: 문 열림 플래그 set, UART 출력 등 */

        UART_Printf("RFID SUCCESS UID=%02X %02X %02X %02X\r\n",
                       uid->uid[0], uid->uid[1], uid->uid[2], uid->uid[3]);

        (void)uid;
    }

    if (RC522_Fsm_ConsumeFailEvent(ctx) != FALSE)
    {
        /* 실패 시 처리 */
        /* App_UartPrintf("RFID FAIL streak=%u\r\n", context->failStreak); */
    }
}

uint32 App_GetTickMs(void)
{
    static uint32 tickMs = 0U;
    static boolean initialized = FALSE;
    static uint32 lastStmTick = 0U;
    uint32 currentStmTick;
    uint32 diffTick;
    uint32 tickPerMs;

    currentStmTick = IfxStm_getLower(BSP_DEFAULT_TIMER);

    if (initialized == FALSE)
    {
        initialized = TRUE;
        lastStmTick = currentStmTick;
        return 0U;
    }

    diffTick   = currentStmTick - lastStmTick;
    tickPerMs  = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1U);

    if (tickPerMs > 0U)
    {
        tickMs += (diffTick / tickPerMs);
        lastStmTick += (diffTick / tickPerMs) * tickPerMs;
    }

    return tickMs;
}

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void App_LedSuccess_On(void)
{
    IfxPort_setPinLow(LED_SUCCESS_PORT, LED_SUCCESS_PIN);
}

static void App_LedSuccess_Off(void)
{
    IfxPort_setPinHigh(LED_SUCCESS_PORT, LED_SUCCESS_PIN);
}

static void App_LedFail_On(void)
{
    IfxPort_setPinLow(LED_FAIL_PORT, LED_FAIL_PIN);
}

static void App_LedFail_Off(void)
{
    IfxPort_setPinHigh(LED_FAIL_PORT, LED_FAIL_PIN);
}

static void App_LedAll_Off(void)
{
    App_LedSuccess_Off();
    App_LedFail_Off();
}

static void App_Led_Init(void)
{
    IfxPort_setPinModeOutput(LED_SUCCESS_PORT, LED_SUCCESS_PIN, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(LED_FAIL_PORT, LED_FAIL_PIN, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);

    App_LedAll_Off();
}
