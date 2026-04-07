#include "string.h"

#include "App_Scheduler.h"

#include "IfxStm.h"
#include "IfxPort.h"
#include "Bsp.h"

#include "Driver_Stm.h"
#include "UART_Config.h"

#include "Base_EncoderMotor.h"
#include "Base_ServoMotor.h"
#include "App_Manger_PositionAxis.h"
#include "App_Manger_DoorActuator.h"
#include "App_Manager_Rfid.h"

#include "Shared_Profile.h"
#include "Shared_System_State.h"
#include "Shared_Can_Message.h"
#include "Shared_Util_Time.h"

#include "MULTICAN_FD.h"


/* -------------------------------------------------------------------------------------------------
 * Button mapping
 * D6 / D7   : mirror jog - / +
 * D22 / D23 : seat jog - / +
 *
 * NOTE:
 * D22/D23 pin mapping is placeholder.
 * 반드시 실제 ShieldBuddy TC275 핀맵에 맞게 수정 필요.
 * ------------------------------------------------------------------------------------------------- */
#define BUTTON_ACTIVE_LOW            1

#define BUTTON_D6_PORT               IfxPort_P02_4.port
#define BUTTON_D6_PIN_IDX            ((uint8)IfxPort_P02_4.pinIndex)

#define BUTTON_D7_PORT               IfxPort_P02_5.port
#define BUTTON_D7_PIN_IDX            ((uint8)IfxPort_P02_5.pinIndex)

#define BUTTON_D22_PORT              IfxPort_P14_0.port
#define BUTTON_D22_PIN_IDX           ((uint8)IfxPort_P14_0.pinIndex)

#define BUTTON_D23_PORT              IfxPort_P14_1.port
#define BUTTON_D23_PIN_IDX           ((uint8)IfxPort_P14_1.pinIndex)

/* -------------------------------------------------------------------------------------------------
 * Status LED mapping
 * - Recommended external LEDs on free GPIOs
 * - yellow : P13.0
 * - red    : P13.1
 * ------------------------------------------------------------------------------------------------- */
#define STATUS_LED_ACTIVE_HIGH       1

#define STATUS_LED_YELLOW_PORT       IfxPort_P13_0.port
#define STATUS_LED_YELLOW_PIN_IDX    ((uint8)IfxPort_P13_0.pinIndex)

#define STATUS_LED_RED_PORT          IfxPort_P13_1.port
#define STATUS_LED_RED_PIN_IDX       ((uint8)IfxPort_P13_1.pinIndex)

#define STATUS_LED_SUCCESS_HOLD_MS   (1000U)
#define STATUS_LED_FAIL_HOLD_MS      (1000U)
#define STATUS_LED_LOCKOUT_HOLD_MS   (5000U)
#define STATUS_LED_BLINK_PERIOD_MS   (500U)

/* -------------------------------------------------------------------------------------------------
 * Motion / actuator tuning
 * ------------------------------------------------------------------------------------------------- */
#define MIRROR_MIN_TICK              (0)
#define MIRROR_MAX_TICK              (255)
#define MIRROR_JOG_STEP_TICK         (5)
#define MIRROR_JOG_DUTY              (550U)
#define MIRROR_JOG_TIMEOUT_MS        (300U)
#define MIRROR_JOG_TOL_TICK          (3)
#define MIRROR_JOG_ISSUE_MS          (20U)
#define MIRROR_RESTORE_DUTY          (600U)
#define MIRROR_RESTORE_TIMEOUT_MS    (6000U)
#define MIRROR_RESTORE_TOL_TICK      (5)

#define SEAT_MIN_TICK                (0)
#define SEAT_MAX_TICK                (255)
#define SEAT_JOG_STEP_TICK           (5)
#define SEAT_JOG_DUTY                (550U)
#define SEAT_JOG_TIMEOUT_MS          (300U)
#define SEAT_JOG_TOL_TICK            (3)
#define SEAT_JOG_ISSUE_MS            (20U)
#define SEAT_RESTORE_DUTY            (600U)
#define SEAT_RESTORE_TIMEOUT_MS      (6000U)
#define SEAT_RESTORE_TOL_TICK        (5)

#define DOOR_CLOSE_ANGLE_DEG         (0)
#define DOOR_OPEN_ANGLE_DEG          (90)
#define DOOR_INIT_ANGLE_DEG          (0)

#define MIRROR_TICK_PER_PROFILE_UNIT (1)
#define SEAT_TICK_PER_PROFILE_UNIT   (1)

typedef enum
{
    STATUS_LED_MODE_OFF = 0,
    STATUS_LED_MODE_SUCCESS,
    STATUS_LED_MODE_FAIL,
    STATUS_LED_MODE_LOCKOUT_BLINK
} App_Status_Led_Mode_t;

/* -------------------------------------------------------------------------------------------------
 * Runtime
 * ------------------------------------------------------------------------------------------------- */
typedef struct
{
    ServoInstance_t        servoDoor;
    DoorActuator_t         door;
    PositionAxis_t         mirrorAxis;
    PositionAxis_t         seatAxis;

    uint8                  currentState;
    uint8                  prevState;
    uint8                  lastHandledState;
    uint8                  activeProfileIdx;

    Shared_Profile_Table_t profileTable;
    App_Manager_Rfid_Output_t rfidOut;

    boolean                rfidTransitionLatched;

    App_Status_Led_Mode_t  statusLedMode;
    uint32                 statusLedExpireMs;
    uint32                 statusLedNextBlinkMs;
    boolean                statusLedBlinkOn;

    /* CAN TX request */
    boolean                   txProfileIdxRequested;
    uint8                     txProfileIdxValue;
    boolean                   txProfileTableRequested;
} AppRuntime_t;

static AppRuntime_t g_app;


/* debug watch */
volatile uint8                g_dbgCurrentState;
volatile uint8                g_dbgPrevState;
volatile uint8                g_dbgActiveProfileIdx;
volatile PositionAxisMode_t   g_dbgMirrorMode;
volatile PositionAxisMode_t   g_dbgSeatMode;
volatile PositionAxisResult_t g_dbgMirrorResult;
volatile PositionAxisResult_t g_dbgSeatResult;
volatile sint32               g_dbgMirrorTick;
volatile sint32               g_dbgSeatTick;
volatile DoorState_t          g_dbgDoorState;
volatile sint16               g_dbgDoorAngle;
static uint64                 max_elapsed_ms = 0;
static uint64                 max_elapsed_ms1 = 0;
static uint64                 max_elapsed_ms10 = 0;
static uint64                 max_elapsed_ms100 = 0;
static uint64                 max_elapsed_ms1000 = 0;

/* -------------------------------------------------------------------------------------------------
 * Local Helper prototypes
 * ------------------------------------------------------------------------------------------------- */
static uint32  App_GetNowMs(void);

static boolean App_ReadButton(Ifx_P *port, uint8 pinIdx);
static void    App_InitButtons(void);
static void    App_InitStatusLeds(void);
static void    App_InitProfileTableDefault(void);
static void    App_InitMirrorAxis(void);
static void    App_InitSeatAxis(void);
static void    App_InitDoor(void);

static void    App_OnStateChanged(uint8 newState);
static void    App_HandleStateTransition1ms(void);
static void    App_UpdateSystemState(uint8 newState);

static sint32  App_ProfileToMirrorTick(uint8 mirrorAngle);
static sint32  App_ProfileToSeatTick(uint8 seatPos);
static uint8   App_MirrorTickToProfile(sint32 mirrorTick);
static uint8   App_SeatTickToProfile(sint32 seatTick);

static void    App_ApplyProfileByIndex(uint8 profileIdx);
static void    App_SaveCurrentPositionToActiveProfile(void);
static void    App_MergeReceivedProfileTable(const Shared_Profile_Table_t *profileTable);
static void    App_MergeReceivedHvacProfileTable(const Shared_Profile_Table_t *profileTable);

static void    App_HandleButtons1ms(void);
static void    App_HandleRfid1ms(void);
static void    App_HandleStateEntry(void);
static void    App_HandleStateSteady100ms(void);
static void    App_HandleStatusLed100ms(void);
static void    App_UpdateDebug(void);

/* CAN placeholders */
static void    App_HandleCanRx1ms(void);
static void    App_HandleCanTx10ms(void);
static boolean App_PollProfileTableAtInit(uint32 timeout_ms);
static void    App_StatusLed_SetYellow(boolean on);
static void    App_StatusLed_SetRed(boolean on);
static void    App_StatusLed_SetAllOff(void);
static void    App_StatusLed_StartSuccess(uint32 now_ms);
static void    App_StatusLed_StartFail(uint32 now_ms);
static void    App_StatusLed_StartLockout(uint32 now_ms);

/* -------------------------------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------------------------------- */
static uint32 App_GetNowMs(void)
{
    uint32 tickPerMs = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1U);
    uint64 stmTick   = IfxStm_get(BSP_DEFAULT_TIMER);

    if (tickPerMs == 0U)
    {
        return 0U;
    }

    return (uint32)(stmTick / tickPerMs);
}

static boolean App_ReadButton(Ifx_P *port, uint8 pinIdx)
{
    uint8 state = IfxPort_getPinState(port, pinIdx);

#if BUTTON_ACTIVE_LOW
    return (state == 0U) ? TRUE : FALSE;
#else
    return (state != 0U) ? TRUE : FALSE;
#endif
}

static void App_InitButtons(void)
{
    IfxPort_setPinModeInput(BUTTON_D6_PORT,  BUTTON_D6_PIN_IDX,  IfxPort_InputMode_pullUp);
    IfxPort_setPinModeInput(BUTTON_D7_PORT,  BUTTON_D7_PIN_IDX,  IfxPort_InputMode_pullUp);
    IfxPort_setPinModeInput(BUTTON_D22_PORT, BUTTON_D22_PIN_IDX, IfxPort_InputMode_pullUp);
    IfxPort_setPinModeInput(BUTTON_D23_PORT, BUTTON_D23_PIN_IDX, IfxPort_InputMode_pullUp);
}

static void App_InitStatusLeds(void)
{
    IfxPort_setPinModeOutput(STATUS_LED_YELLOW_PORT,
                             STATUS_LED_YELLOW_PIN_IDX,
                             IfxPort_OutputMode_pushPull,
                             IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(STATUS_LED_RED_PORT,
                             STATUS_LED_RED_PIN_IDX,
                             IfxPort_OutputMode_pushPull,
                             IfxPort_OutputIdx_general);

    App_StatusLed_SetAllOff();
}

static void App_InitProfileTableDefault(void)
{
    uint8 i;

    for (i = 0U; i < SHARED_PROFILE_TOTAL_COUNT; ++i)
    {
        g_app.profileTable.profile[i].profile_id          = i;
        g_app.profileTable.profile[i].side_motor_angle    = 0U;
        g_app.profileTable.profile[i].seat_motor_angle    = 128U;
        g_app.profileTable.profile[i].ambient_light       = 0U;
        g_app.profileTable.profile[i].ac_on_threshold     = 0;
        g_app.profileTable.profile[i].heater_on_threshold = 0;
    }

    g_app.profileTable.profile[SHARED_PROFILE_INDEX_DEFAULT].profile_id       = 100U;
    g_app.profileTable.profile[SHARED_PROFILE_INDEX_DEFAULT].side_motor_angle = 20U;
    g_app.profileTable.profile[SHARED_PROFILE_INDEX_DEFAULT].seat_motor_angle = 128U;

    g_app.profileTable.profile[SHARED_PROFILE_INDEX_EMERGENCY].profile_id       = 999U;
    g_app.profileTable.profile[SHARED_PROFILE_INDEX_EMERGENCY].side_motor_angle = 0U;
    g_app.profileTable.profile[SHARED_PROFILE_INDEX_EMERGENCY].seat_motor_angle = 0U;
}

static void App_InitMirrorAxis(void)
{
    MotorConfig_t motorCfg;
    PositionAxisConfig_t axisCfg;

    motorCfg.pwmPin          = MOTOR_A_PWM_PIN;
    motorCfg.brakePort       = MOTOR_A_BRAKE_PORT;
    motorCfg.brakePin        = MOTOR_A_BRAKE_PIN_IDX;
    motorCfg.dirPort         = MOTOR_A_DIR_PORT;
    motorCfg.dirPin          = MOTOR_A_DIR_PIN_IDX;
    motorCfg.brakeActiveHigh = TRUE;
    motorCfg.dirForwardHigh  = TRUE;
    motorCfg.encPort         = MOTOR_A_ENC_SINGLE_PORT;
    motorCfg.encPin          = MOTOR_A_ENC_SINGLE_PIN_IDX;

    Motor_Init(&g_motorA, &motorCfg);

    axisCfg.motor                 = &g_motorA;
    axisCfg.minTick               = MIRROR_MIN_TICK;
    axisCfg.maxTick               = MIRROR_MAX_TICK;
    axisCfg.jogStepTick           = MIRROR_JOG_STEP_TICK;
    axisCfg.jogDuty               = MIRROR_JOG_DUTY;
    axisCfg.jogTimeoutMs          = MIRROR_JOG_TIMEOUT_MS;
    axisCfg.jogToleranceTicks     = MIRROR_JOG_TOL_TICK;
    axisCfg.jogIssuePeriodMs      = MIRROR_JOG_ISSUE_MS;
    axisCfg.jogUseGridStep        = TRUE;
    axisCfg.restoreDuty           = MIRROR_RESTORE_DUTY;
    axisCfg.restoreTimeoutMs      = MIRROR_RESTORE_TIMEOUT_MS;
    axisCfg.restoreToleranceTicks = MIRROR_RESTORE_TOL_TICK;

    PositionAxis_Init(&g_app.mirrorAxis, &axisCfg);
}

static void App_InitSeatAxis(void)
{
    MotorConfig_t motorCfg;
    PositionAxisConfig_t axisCfg;

    motorCfg.pwmPin          = MOTOR_B_PWM_PIN;
    motorCfg.brakePort       = MOTOR_B_BRAKE_PORT;
    motorCfg.brakePin        = MOTOR_B_BRAKE_PIN_IDX;
    motorCfg.dirPort         = MOTOR_B_DIR_PORT;
    motorCfg.dirPin          = MOTOR_B_DIR_PIN_IDX;
    motorCfg.brakeActiveHigh = TRUE;
    motorCfg.dirForwardHigh  = TRUE;
    motorCfg.encPort         = MOTOR_B_ENC_SINGLE_PORT;
    motorCfg.encPin          = MOTOR_B_ENC_SINGLE_PIN_IDX;

    Motor_Init(&g_motorB, &motorCfg);

    axisCfg.motor                 = &g_motorB;
    axisCfg.minTick               = SEAT_MIN_TICK;
    axisCfg.maxTick               = SEAT_MAX_TICK;
    axisCfg.jogStepTick           = SEAT_JOG_STEP_TICK;
    axisCfg.jogDuty               = SEAT_JOG_DUTY;
    axisCfg.jogTimeoutMs          = SEAT_JOG_TIMEOUT_MS;
    axisCfg.jogToleranceTicks     = SEAT_JOG_TOL_TICK;
    axisCfg.jogIssuePeriodMs      = SEAT_JOG_ISSUE_MS;
    axisCfg.jogUseGridStep        = FALSE;
    axisCfg.restoreDuty           = SEAT_RESTORE_DUTY;
    axisCfg.restoreTimeoutMs      = SEAT_RESTORE_TIMEOUT_MS;
    axisCfg.restoreToleranceTicks = SEAT_RESTORE_TOL_TICK;

    PositionAxis_Init(&g_app.seatAxis, &axisCfg);
}

static void App_InitDoor(void)
{
    ServoConfig_t servoCfg;
    DoorActuatorConfig_t doorCfg;

    servoCfg.pwmPin           = SERVO_D5_PWM_PIN;
    servoCfg.pwmPeriodTicks   = SERVO_PWM_PERIOD_TICKS;
    servoCfg.minPulseTicks    = SERVO_PULSE_MIN_TICKS;
    servoCfg.centerPulseTicks = SERVO_PULSE_CENTER_TICKS;
    servoCfg.maxPulseTicks    = SERVO_PULSE_MAX_TICKS;
    servoCfg.initAngleDeg     = DOOR_INIT_ANGLE_DEG;

    Servo_Init(&g_app.servoDoor, &servoCfg);

    doorCfg.servo         = &g_app.servoDoor;
    doorCfg.closeAngleDeg = DOOR_CLOSE_ANGLE_DEG;
    doorCfg.openAngleDeg  = DOOR_OPEN_ANGLE_DEG;
    doorCfg.initAngleDeg  = DOOR_INIT_ANGLE_DEG;

    DoorActuator_Init(&g_app.door, &doorCfg);
}

static void App_OnStateChanged(uint8 newState)
{
    if (newState == SHARED_SYSTEM_STATE_SLEEP)
    {
        g_app.activeProfileIdx      = SHARED_PROFILE_INDEX_INVALID;
        g_app.rfidTransitionLatched = FALSE;
    }
    else if (newState == SHARED_SYSTEM_STATE_ACTIVATED)
    {
        /* Allow one more valid tag to request shutdown. */
        g_app.rfidTransitionLatched = FALSE;
    }
}

static void App_HandleStateTransition1ms(void)
{
    if (g_app.currentState != g_app.lastHandledState)
    {
        App_OnStateChanged(g_app.currentState);
        App_HandleStateEntry();
        g_app.lastHandledState = g_app.currentState;
    }
}

static void App_UpdateSystemState(uint8 newState)
{
    if (g_app.currentState != newState)
    {
        g_app.prevState    = g_app.currentState;
        g_app.currentState = newState;
    }
}

static sint32 App_ProfileToMirrorTick(uint8 mirrorAngle)
{
    sint32 tick = (sint32)mirrorAngle * MIRROR_TICK_PER_PROFILE_UNIT;

    if (tick < MIRROR_MIN_TICK) tick = MIRROR_MIN_TICK;
    if (tick > MIRROR_MAX_TICK) tick = MIRROR_MAX_TICK;
    return tick;
}

static sint32 App_ProfileToSeatTick(uint8 seatPos)
{
    sint32 tick = (sint32)seatPos * SEAT_TICK_PER_PROFILE_UNIT;

    if (tick < SEAT_MIN_TICK) tick = SEAT_MIN_TICK;
    if (tick > SEAT_MAX_TICK) tick = SEAT_MAX_TICK;
    return tick;
}

static uint8 App_MirrorTickToProfile(sint32 mirrorTick)
{
    sint32 value = mirrorTick / MIRROR_TICK_PER_PROFILE_UNIT;

    if (value < 0)   value = 0;
    if (value > 255) value = 255;

    return (uint8)value;
}

static uint8 App_SeatTickToProfile(sint32 seatTick)
{
    sint32 value = (seatTick / SEAT_TICK_PER_PROFILE_UNIT) + 128;

    if (value < 0)   value = 0;
    if (value > 255) value = 255;

    return (uint8)value;
}

static void App_ApplyProfileByIndex(uint8 profileIdx)
{
    sint32 mirrorTarget;
    sint32 seatTarget;

    if (profileIdx >= SHARED_PROFILE_TOTAL_COUNT)
    {
        return;
    }

    mirrorTarget = App_ProfileToMirrorTick(g_app.profileTable.profile[profileIdx].side_motor_angle);
    seatTarget   = App_ProfileToSeatTick(g_app.profileTable.profile[profileIdx].seat_motor_angle);

    PositionAxis_Stop(&g_app.mirrorAxis);
    PositionAxis_Stop(&g_app.seatAxis);

    PositionAxis_ClearResult(&g_app.mirrorAxis);
    PositionAxis_ClearResult(&g_app.seatAxis);

    (void)PositionAxis_StartRestore(&g_app.mirrorAxis, mirrorTarget);
    (void)PositionAxis_StartRestore(&g_app.seatAxis, seatTarget);
}

static void App_SaveCurrentPositionToActiveProfile(void)
{
    sint32 mirrorTick;
    sint32 seatTick;

    if (g_app.activeProfileIdx >= SHARED_PROFILE_TOTAL_COUNT)
    {
        return;
    }

    mirrorTick = PositionAxis_GetTick(&g_app.mirrorAxis);
    seatTick   = PositionAxis_GetTick(&g_app.seatAxis);

    g_app.profileTable.profile[g_app.activeProfileIdx].side_motor_angle =
        App_MirrorTickToProfile(mirrorTick);

    g_app.profileTable.profile[g_app.activeProfileIdx].seat_motor_angle =
        App_SeatTickToProfile(seatTick);
}

static void App_MergeReceivedProfileTable(const Shared_Profile_Table_t *profileTable)
{
    uint8 idx;

    if (profileTable == NULL_PTR)
    {
        return;
    }

    for (idx = 0U; idx < SHARED_PROFILE_TOTAL_COUNT; ++idx)
    {
        if (idx == g_app.activeProfileIdx)
        {
            g_app.profileTable.profile[idx].profile_id =
                profileTable->profile[idx].profile_id;
            g_app.profileTable.profile[idx].ambient_light =
                profileTable->profile[idx].ambient_light;
            g_app.profileTable.profile[idx].ac_on_threshold =
                profileTable->profile[idx].ac_on_threshold;
            g_app.profileTable.profile[idx].heater_on_threshold =
                profileTable->profile[idx].heater_on_threshold;
        }
        else
        {
            g_app.profileTable.profile[idx] = profileTable->profile[idx];
        }
    }
}

static void App_MergeReceivedHvacProfileTable(const Shared_Profile_Table_t *profileTable)
{
    uint8 idx;

    if (profileTable == NULL_PTR)
    {
        return;
    }

    for (idx = 0U; idx < SHARED_PROFILE_TOTAL_COUNT; ++idx)
    {
        g_app.profileTable.profile[idx].ambient_light =
            profileTable->profile[idx].ambient_light;
        g_app.profileTable.profile[idx].ac_on_threshold =
            profileTable->profile[idx].ac_on_threshold;
        g_app.profileTable.profile[idx].heater_on_threshold =
            profileTable->profile[idx].heater_on_threshold;
    }
}

/* -------------------------------------------------------------------------------------------------
 * Button jog
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleButtons1ms(void)
{
    boolean mirrorNeg;
    boolean mirrorPos;
    boolean seatNeg;
    boolean seatPos;
    PositionAxisMode_t mm;
    PositionAxisMode_t sm;

    mirrorNeg = App_ReadButton(BUTTON_D6_PORT,  BUTTON_D6_PIN_IDX);
    mirrorPos = App_ReadButton(BUTTON_D7_PORT,  BUTTON_D7_PIN_IDX);
    seatNeg   = App_ReadButton(BUTTON_D22_PORT, BUTTON_D22_PIN_IDX);
    seatPos   = App_ReadButton(BUTTON_D23_PORT, BUTTON_D23_PIN_IDX);

    mm = PositionAxis_GetMode(&g_app.mirrorAxis);
    sm = PositionAxis_GetMode(&g_app.seatAxis);

    switch (g_app.currentState)
    {
    case SHARED_SYSTEM_STATE_ACTIVATED:
        break;

    case SHARED_SYSTEM_STATE_SLEEP:
    case SHARED_SYSTEM_STATE_SETUP:
    case SHARED_SYSTEM_STATE_SHUTDOWN:
    case SHARED_SYSTEM_STATE_DENIED:
    case SHARED_SYSTEM_STATE_EMERGENCY:
    default:
        if ((mm == AXIS_MODE_JOG_POSITIVE) || (mm == AXIS_MODE_JOG_NEGATIVE))
        {
            PositionAxis_Stop(&g_app.mirrorAxis);
        }

        if ((sm == AXIS_MODE_JOG_POSITIVE) || (sm == AXIS_MODE_JOG_NEGATIVE))
        {
            PositionAxis_Stop(&g_app.seatAxis);
        }

        return;
    }

    /* restore 중이면 jog 금지 */
    if ((mm == AXIS_MODE_RESTORE_TO_ZERO) || (mm == AXIS_MODE_RESTORE_TO_TARGET))
    {
        /* do nothing */
    }
    else
    {
        if (mirrorPos && !mirrorNeg)
        {
            PositionAxis_StartJogPositive(&g_app.mirrorAxis);
        }
        else if (mirrorNeg && !mirrorPos)
        {
            PositionAxis_StartJogNegative(&g_app.mirrorAxis);
        }
        else
        {
            if ((mm == AXIS_MODE_JOG_POSITIVE) || (mm == AXIS_MODE_JOG_NEGATIVE))
            {
                PositionAxis_Stop(&g_app.mirrorAxis);
            }
        }
    }

    if ((sm == AXIS_MODE_RESTORE_TO_ZERO) || (sm == AXIS_MODE_RESTORE_TO_TARGET))
    {
        /* do nothing */
    }
    else
    {
        if (seatPos && !seatNeg)
        {
            PositionAxis_StartJogPositive(&g_app.seatAxis);
        }
        else if (seatNeg && !seatPos)
        {
            PositionAxis_StartJogNegative(&g_app.seatAxis);
        }
        else
        {
            if ((sm == AXIS_MODE_JOG_POSITIVE) || (sm == AXIS_MODE_JOG_NEGATIVE))
            {
                PositionAxis_Stop(&g_app.seatAxis);
            }
        }
    }
}

/* -------------------------------------------------------------------------------------------------
 * RFID / State handling
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleRfid1ms(void)
{
    App_Manager_Rfid_Input_t in;
    in.register_flag = FALSE;
    in.profile_table = &g_app.profileTable;
    uint32 nowMs = App_GetNowMs();

    App_Manager_Rfid_Run(nowMs, &in, &g_app.rfidOut);
    
    uint8 requestedProfileIdx;

    switch (g_app.currentState)
    {
    case SHARED_SYSTEM_STATE_SLEEP:
        switch (g_app.rfidOut.event)
        {
        case APP_MANAGER_RFID_EVENT_SUCCESS:
            if (g_app.rfidOut.uid_idx >= SHARED_PROFILE_NORMAL_COUNT)
            {
                UART_Printf("[RFID] success invalid idx=%u\r\n", g_app.rfidOut.uid_idx);
                break;
            }

            requestedProfileIdx = g_app.rfidOut.uid_idx;

            if (g_app.rfidTransitionLatched == FALSE)
            {
                g_app.activeProfileIdx      = requestedProfileIdx;
                g_app.txProfileIdxValue     = requestedProfileIdx;
                g_app.txProfileIdxRequested = TRUE;
                g_app.rfidTransitionLatched = TRUE;
                App_StatusLed_StartSuccess(nowMs);

                UART_Printf("[RFID] auth idx=%u\r\n", requestedProfileIdx);
            }
            else
            {
                UART_Printf("[RFID] auth ignored (latched)\r\n");
            }
            break;

        case APP_MANAGER_RFID_EVENT_FAIL:
            App_StatusLed_StartFail(nowMs);
            UART_Printf("[RFID] fail st=%u\r\n", g_app.currentState);
            break;

        case APP_MANAGER_RFID_EVENT_LOCKOUT:
            if (g_app.rfidTransitionLatched == FALSE)
            {
                g_app.txProfileIdxValue     = SHARED_PROFILE_INDEX_INVALID;
                g_app.txProfileIdxRequested = TRUE;
                g_app.rfidTransitionLatched = TRUE;
                App_StatusLed_StartLockout(nowMs);

                UART_Printf("[RFID] lockout -> denied req\r\n");
            }
            else
            {
                UART_Printf("[RFID] lockout ignored (latched)\r\n");
            }
            break;

        case APP_MANAGER_RFID_EVENT_NONE:
        default:
            break;
        }
        break;

    case SHARED_SYSTEM_STATE_ACTIVATED:
        switch (g_app.rfidOut.event)
        {
        case APP_MANAGER_RFID_EVENT_SUCCESS:
            if (g_app.rfidOut.uid_idx >= SHARED_PROFILE_NORMAL_COUNT)
            {
                UART_Printf("[RFID] success invalid idx=%u\r\n", g_app.rfidOut.uid_idx);
                break;
            }

            requestedProfileIdx = g_app.rfidOut.uid_idx;

            if (g_app.rfidTransitionLatched == FALSE)
            {
                g_app.txProfileIdxValue     = requestedProfileIdx;
                g_app.txProfileIdxRequested = TRUE;
                g_app.rfidTransitionLatched = TRUE;
                App_StatusLed_StartSuccess(nowMs);

                UART_Printf("[RFID] shutdown req idx=%u\r\n", requestedProfileIdx);
            }
            else
            {
                UART_Printf("[RFID] shutdown ignored (latched)\r\n");
            }
            break;

        case APP_MANAGER_RFID_EVENT_FAIL:
            App_StatusLed_StartFail(nowMs);
            UART_Printf("[RFID] fail st=%u\r\n", g_app.currentState);
            break;

        case APP_MANAGER_RFID_EVENT_LOCKOUT:
            App_StatusLed_StartLockout(nowMs);
            UART_Printf("[RFID] lockout st=%u\r\n", g_app.currentState);
            break;

        case APP_MANAGER_RFID_EVENT_NONE:
        default:
            break;
        }
        break;

    case SHARED_SYSTEM_STATE_SETUP:
    case SHARED_SYSTEM_STATE_SHUTDOWN:
    case SHARED_SYSTEM_STATE_DENIED:
    case SHARED_SYSTEM_STATE_EMERGENCY:
    default:
        if (g_app.rfidOut.event != APP_MANAGER_RFID_EVENT_NONE)
        {
            UART_Printf("[RFID] event ignored st=%u ev=%u\r\n",
                        g_app.currentState,
                        g_app.rfidOut.event);
        }
        break;
    }
}

static void App_HandleStateEntry(void)
{
    switch (g_app.currentState)
    {
    case SHARED_SYSTEM_STATE_SLEEP:
        DoorActuator_Close(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_SETUP:
        DoorActuator_Open(&g_app.door);
        App_ApplyProfileByIndex(g_app.activeProfileIdx);
        break;

    case SHARED_SYSTEM_STATE_ACTIVATED:
        DoorActuator_Open(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_SHUTDOWN:
        DoorActuator_Close(&g_app.door);
        App_SaveCurrentPositionToActiveProfile();
        g_app.txProfileTableRequested = TRUE;
        App_ApplyProfileByIndex(SHARED_PROFILE_INDEX_DEFAULT);
        UART_Printf("[STATE] shutdown -> save + default restore\r\n");
        break;

    case SHARED_SYSTEM_STATE_DENIED:
        DoorActuator_Close(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_EMERGENCY:
    default:
        DoorActuator_Open(&g_app.door);
        PositionAxis_Stop(&g_app.seatAxis);
        (void)PositionAxis_StartRestore(&g_app.seatAxis, SEAT_MIN_TICK);
        break;
    }
}

static void App_HandleStateSteady100ms(void)
{
    switch (g_app.currentState)
    {
    case SHARED_SYSTEM_STATE_ACTIVATED:
        App_SaveCurrentPositionToActiveProfile();
        break;

    case SHARED_SYSTEM_STATE_SLEEP:
    case SHARED_SYSTEM_STATE_SETUP:
    case SHARED_SYSTEM_STATE_SHUTDOWN:
    case SHARED_SYSTEM_STATE_DENIED:
    case SHARED_SYSTEM_STATE_EMERGENCY:
    default:
        break;
    }
}

static void App_HandleStatusLed100ms(void)
{
    uint32 nowMs;

    nowMs = App_GetNowMs();

    switch (g_app.statusLedMode)
    {
    case STATUS_LED_MODE_SUCCESS:
    case STATUS_LED_MODE_FAIL:
        if (nowMs >= g_app.statusLedExpireMs)
        {
            g_app.statusLedMode = STATUS_LED_MODE_OFF;
            App_StatusLed_SetAllOff();
        }
        break;

    case STATUS_LED_MODE_LOCKOUT_BLINK:
        if (nowMs >= g_app.statusLedExpireMs)
        {
            g_app.statusLedMode = STATUS_LED_MODE_OFF;
            g_app.statusLedBlinkOn = FALSE;
            App_StatusLed_SetAllOff();
        }
        else if (nowMs >= g_app.statusLedNextBlinkMs)
        {
            g_app.statusLedBlinkOn = (g_app.statusLedBlinkOn == FALSE) ? TRUE : FALSE;
            g_app.statusLedNextBlinkMs = nowMs + STATUS_LED_BLINK_PERIOD_MS;

            App_StatusLed_SetYellow(FALSE);
            App_StatusLed_SetRed(g_app.statusLedBlinkOn);
        }
        break;

    case STATUS_LED_MODE_OFF:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------------------------------
 * Debug
 * ------------------------------------------------------------------------------------------------- */
static void App_UpdateDebug(void)
{
    g_dbgCurrentState     = g_app.currentState;
    g_dbgPrevState        = g_app.prevState;
    g_dbgActiveProfileIdx = g_app.activeProfileIdx;
    g_dbgMirrorMode       = PositionAxis_GetMode(&g_app.mirrorAxis);
    g_dbgSeatMode         = PositionAxis_GetMode(&g_app.seatAxis);
    g_dbgMirrorResult     = PositionAxis_GetResult(&g_app.mirrorAxis);
    g_dbgSeatResult       = PositionAxis_GetResult(&g_app.seatAxis);
    g_dbgMirrorTick       = PositionAxis_GetTick(&g_app.mirrorAxis);
    g_dbgSeatTick         = PositionAxis_GetTick(&g_app.seatAxis);
    g_dbgDoorState        = DoorActuator_GetState(&g_app.door);
    g_dbgDoorAngle        = DoorActuator_GetAngle(&g_app.door);
}

static void App_StatusLed_SetYellow(boolean on)
{
#if STATUS_LED_ACTIVE_HIGH
    if (on != FALSE)
    {
        IfxPort_setPinHigh(STATUS_LED_YELLOW_PORT, STATUS_LED_YELLOW_PIN_IDX);
    }
    else
    {
        IfxPort_setPinLow(STATUS_LED_YELLOW_PORT, STATUS_LED_YELLOW_PIN_IDX);
    }
#else
    if (on != FALSE)
    {
        IfxPort_setPinLow(STATUS_LED_YELLOW_PORT, STATUS_LED_YELLOW_PIN_IDX);
    }
    else
    {
        IfxPort_setPinHigh(STATUS_LED_YELLOW_PORT, STATUS_LED_YELLOW_PIN_IDX);
    }
#endif
}

static void App_StatusLed_SetRed(boolean on)
{
#if STATUS_LED_ACTIVE_HIGH
    if (on != FALSE)
    {
        IfxPort_setPinHigh(STATUS_LED_RED_PORT, STATUS_LED_RED_PIN_IDX);
    }
    else
    {
        IfxPort_setPinLow(STATUS_LED_RED_PORT, STATUS_LED_RED_PIN_IDX);
    }
#else
    if (on != FALSE)
    {
        IfxPort_setPinLow(STATUS_LED_RED_PORT, STATUS_LED_RED_PIN_IDX);
    }
    else
    {
        IfxPort_setPinHigh(STATUS_LED_RED_PORT, STATUS_LED_RED_PIN_IDX);
    }
#endif
}

static void App_StatusLed_SetAllOff(void)
{
    App_StatusLed_SetYellow(FALSE);
    App_StatusLed_SetRed(FALSE);
}

static void App_StatusLed_StartSuccess(uint32 now_ms)
{
    g_app.statusLedMode = STATUS_LED_MODE_SUCCESS;
    g_app.statusLedExpireMs = now_ms + STATUS_LED_SUCCESS_HOLD_MS;
    g_app.statusLedBlinkOn = FALSE;
    g_app.statusLedNextBlinkMs = 0U;

    App_StatusLed_SetRed(FALSE);
    App_StatusLed_SetYellow(TRUE);
}

static void App_StatusLed_StartFail(uint32 now_ms)
{
    g_app.statusLedMode = STATUS_LED_MODE_FAIL;
    g_app.statusLedExpireMs = now_ms + STATUS_LED_FAIL_HOLD_MS;
    g_app.statusLedBlinkOn = FALSE;
    g_app.statusLedNextBlinkMs = 0U;

    App_StatusLed_SetYellow(FALSE);
    App_StatusLed_SetRed(TRUE);
}

static void App_StatusLed_StartLockout(uint32 now_ms)
{
    g_app.statusLedMode = STATUS_LED_MODE_LOCKOUT_BLINK;
    g_app.statusLedExpireMs = now_ms + STATUS_LED_LOCKOUT_HOLD_MS;
    g_app.statusLedNextBlinkMs = now_ms + STATUS_LED_BLINK_PERIOD_MS;
    g_app.statusLedBlinkOn = TRUE;

    App_StatusLed_SetYellow(FALSE);
    App_StatusLed_SetRed(TRUE);
}

/* -------------------------------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------------------------------- */
void App_Init(void)
{
    App_Manager_Rfid_Input_t rfidIn;
    uint32 nowMs;

    (void)memset(&g_app, 0, sizeof(g_app));

    App_InitButtons();
    App_InitStatusLeds();
    App_InitProfileTableDefault();
    App_InitMirrorAxis();
    App_InitSeatAxis();
    App_InitDoor();

    initMultican();

    g_app.currentState          = SHARED_SYSTEM_STATE_SLEEP;
    g_app.prevState             = SHARED_SYSTEM_STATE_SLEEP;
    g_app.lastHandledState      = 0xFFU;
    g_app.activeProfileIdx      = SHARED_PROFILE_INDEX_INVALID;
    g_app.rfidTransitionLatched = FALSE;
    g_app.statusLedMode         = STATUS_LED_MODE_OFF;
    g_app.statusLedExpireMs     = 0U;
    g_app.statusLedNextBlinkMs  = 0U;
    g_app.statusLedBlinkOn      = FALSE;

    /* 초기화 시점에 SS_PROFILE_TABLE 1회 수신 시도, 20초 타임아웃 */
    (void)App_PollProfileTableAtInit(20000U);

    rfidIn.register_flag = FALSE;
    rfidIn.profile_table = &g_app.profileTable;

    nowMs = App_GetNowMs();

    App_Manager_Rfid_Init(nowMs);
    App_Manager_Rfid_Run(nowMs, &rfidIn, &g_app.rfidOut);

    UART_Printf("[APP] init done\r\n");
}

void AppTask1ms(void)
{
    PositionAxis_Task1ms(&g_app.mirrorAxis);
    PositionAxis_Task1ms(&g_app.seatAxis);

    App_HandleButtons1ms();
}

void AppTask10ms(void)
{
    App_HandleStateTransition1ms();
    App_HandleRfid1ms();
    App_UpdateDebug();
}

void AppTask100ms(void)
{
    App_HandleCanRx1ms();
    App_HandleStateSteady100ms();
    App_HandleStatusLed100ms();
    App_HandleCanTx10ms();
}

void AppTask1000ms(void)
{
    UART_Printf("[DBG] st=%u prev=%u prof=%u seat=%ld mirror=%ld door=%d\r\n",
                g_app.currentState,
                g_app.prevState,
                g_app.activeProfileIdx,
                (long)PositionAxis_GetTick(&g_app.seatAxis),
                (long)PositionAxis_GetTick(&g_app.mirrorAxis),
                (int)DoorActuator_GetState(&g_app.door));

    /* TODO:
     * 주기 CAN task 자리
     * - heartbeat
     * - profile table periodic tx
     */
    g_app.txProfileTableRequested = TRUE;
}

void AppScheduling(void)
{
    uint64 elapsed_ms, elapsed_ms1, elapsed_ms10, elapsed_ms100, elapsed_ms1000;
    uint64 now_us = Shared_Util_Time_GetNowUs();
    if (stSchedulingInfo.u8nuScheduling1msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling1msFlag = 0U;
        AppTask1ms();
        elapsed_ms1 = Shared_Util_Time_GetNowUs() - now_us;
    }

    if (stSchedulingInfo.u8nuScheduling10msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling10msFlag = 0U;
        AppTask10ms();
        elapsed_ms10 = Shared_Util_Time_GetNowUs() - now_us;
    }

    if (stSchedulingInfo.u8nuScheduling100msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling100msFlag = 0U;
        AppTask100ms();
        elapsed_ms100 = Shared_Util_Time_GetNowUs() - now_us;
    }

    if (stSchedulingInfo.u8nuScheduling1000msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling1000msFlag = 0U;
        AppTask1000ms();
        elapsed_ms1000 = Shared_Util_Time_GetNowUs() - now_us;
    }
    elapsed_ms = Shared_Util_Time_GetNowUs() - now_us;
    if (max_elapsed_ms < elapsed_ms) {
        max_elapsed_ms = elapsed_ms;
    }
    if (max_elapsed_ms1 < elapsed_ms1) {
        max_elapsed_ms1 = elapsed_ms1;
    }
    if (max_elapsed_ms10 < elapsed_ms10) {
        max_elapsed_ms10 = elapsed_ms10;
    }
    if (max_elapsed_ms100 < elapsed_ms100) {
        max_elapsed_ms100 = elapsed_ms100;
    }
    if (max_elapsed_ms1000 < elapsed_ms1000) {
        max_elapsed_ms1000 = elapsed_ms1000;
    }
}

/* -------------------------------------------------------------------------------------------------
 * CAN RX
 * - 단일 RX object에서 받은 메시지를 상위에서 ID 기준으로 파싱
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleCanRx1ms(void)
{
    uint32       rx_data[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32       rx_id;
    const uint8 *rx_bytes;

    if (receiveCanMessage(rx_data) == FALSE)
    {
        return;
    }

    rx_id    = g_multican.rxMsg.id;
    rx_bytes = (const uint8 *)rx_data;

    switch (rx_id)
    {
        case SHARED_CAN_MSG_ID_SS_STATE:
        {
            Shared_Can_State_t state_msg;

            (void)memset(&state_msg, 0, sizeof(state_msg));
            (void)memcpy(&state_msg,
                         rx_bytes,
                         sizeof(state_msg));

            /* SS 시스템 상태 반영 */
            App_UpdateSystemState(state_msg.current_state);

            UART_Printf("[RX] SS_STATE current_state=%u\r\n",
                        state_msg.current_state);
            break;
        }

        case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
        case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        {
            Shared_Profile_Table_t profile_table_msg;

            (void)memset(&profile_table_msg, 0, sizeof(profile_table_msg));
            (void)memcpy(&profile_table_msg,
                         rx_bytes,
                         sizeof(profile_table_msg));

            if (rx_id == SHARED_CAN_MSG_ID_SS_PROFILE_TABLE)
            {
                /* Keep local active motor positions, but refresh the rest of the table. */
                App_MergeReceivedProfileTable(&profile_table_msg);
            }
            else
            {
                App_MergeReceivedHvacProfileTable(&profile_table_msg);
            }

            break;
        }

        case SHARED_CAN_MSG_ID_HH_PROFILE_IDX:
        {
            Shared_Can_Profile_Idx_t profile_idx_msg;

            (void)memset(&profile_idx_msg, 0, sizeof(profile_idx_msg));
            (void)memcpy(&profile_idx_msg,
                         rx_bytes,
                         sizeof(profile_idx_msg));

            /* HH에서 선택한 프로필 인덱스 반영 */
            if ((profile_idx_msg.profile_index < SHARED_PROFILE_TOTAL_COUNT) ||
                (profile_idx_msg.profile_index == SHARED_PROFILE_INDEX_INVALID))
            {
                g_app.activeProfileIdx = profile_idx_msg.profile_index;
                App_ApplyProfileByIndex(g_app.activeProfileIdx);
                UART_Printf("[RX] HH_PROFILE_IDX profile_index=%u\r\n",
                            profile_idx_msg.profile_index);
            }
            else
            {
                UART_Printf("[RX] HH_PROFILE_IDX ignored invalid idx=%u\r\n",
                            profile_idx_msg.profile_index);
            }
            break;
        }

        default:
        {
            UART_Printf("[RX] Unknown CAN ID=0x%03X\r\n", rx_id);
            break;
        }
    }
}

/* -------------------------------------------------------------------------------------------------
 * CAN TX
 * - 10ms 주기 함수에서 송신 처리
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleCanTx10ms(void)
{
    uint32 tx_data[MAXIMUM_CAN_DATA_PAYLOAD];

    if (g_app.txProfileIdxRequested == TRUE)
    {
        (void)memset(tx_data, 0, sizeof(tx_data));
        ((uint8 *)tx_data)[0] = g_app.txProfileIdxValue;

        transmitCanMessage(SHARED_CAN_MSG_ID_AB_PROFILE_IDX, tx_data);

        UART_Printf("[TX] AB_PROFILE_IDX profile_index=%u\r\n",
                    g_app.txProfileIdxValue);

        g_app.txProfileIdxRequested = FALSE;
    }

    /* 추후 프로필 테이블 송신 추가
     * - active/shutdown 시점에 현재 테이블을 다른 ECU로 공유할 때 사용
     * - Shared_Profile_Table_t 크기가 40B라는 전제에서 1 frame 전송
     */
    else if (g_app.txProfileTableRequested == TRUE)
    {
        (void)memset(tx_data, 0, sizeof(tx_data));

        (void)memcpy((uint8 *)tx_data,
                     &g_app.profileTable,
                     sizeof(Shared_Profile_Table_t));

        transmitCanMessage(SHARED_CAN_MSG_ID_AB_PROFILE_TABLE, tx_data);

        //UART_Printf("[TX] AB_PROFILE_TABLE sent\r\n");

        g_app.txProfileTableRequested = FALSE;
    }
}

/* -------------------------------------------------------------------------------------------------
 * INIT CAN POLLING
 * - 초기화 시점에 profile table을 일정 시간 동안 폴링으로 수신
 * - 수신 성공 시 g_app.profileTable 갱신 후 TRUE 반환
 * ------------------------------------------------------------------------------------------------- */
static boolean App_PollProfileTableAtInit(uint32 timeout_ms)
{
    uint32       rx_data[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32       rx_id;
    const uint8 *rx_bytes;
    uint32       start_ms;

    start_ms = App_GetNowMs();

    while ((App_GetNowMs() - start_ms) < timeout_ms)
    {
        if (receiveCanMessage(rx_data) == FALSE)
        {
            continue;
        }

        rx_id    = g_multican.rxMsg.id;
        rx_bytes = (const uint8 *)rx_data;

        switch (rx_id)
        {
            case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
            case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
            {
                Shared_Profile_Table_t profile_table_msg;

                (void)memset(&profile_table_msg, 0, sizeof(profile_table_msg));
                (void)memcpy(&profile_table_msg,
                             rx_bytes,
                             sizeof(profile_table_msg));

                if (rx_id == SHARED_CAN_MSG_ID_SS_PROFILE_TABLE)
                {
                    (void)memcpy(&g_app.profileTable,
                                 &profile_table_msg,
                                 sizeof(Shared_Profile_Table_t));
                }
                else
                {
                    App_MergeReceivedHvacProfileTable(&profile_table_msg);
                }

                return TRUE;
            }

            case SHARED_CAN_MSG_ID_SS_STATE:
            {
                Shared_Can_State_t state_msg;

                (void)memset(&state_msg, 0, sizeof(state_msg));
                (void)memcpy(&state_msg,
                             rx_bytes,
                             sizeof(state_msg));

                App_UpdateSystemState(state_msg.current_state);

                UART_Printf("[INIT][RX] SS_STATE current_state=%u\r\n",
                            state_msg.current_state);
                break;
            }

            default:
            {
                /* init 단계에서는 profile table 외 메시지는 무시 */
                break;
            }
        }
    }

    UART_Printf("[INIT][RX] PROFILE_TABLE timeout -> use default\r\n");
    return FALSE;
}
