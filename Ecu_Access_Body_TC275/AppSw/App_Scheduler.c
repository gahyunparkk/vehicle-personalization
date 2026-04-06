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
 * Motion / actuator tuning
 * ------------------------------------------------------------------------------------------------- */
#define MIRROR_MIN_TICK              (0)
#define MIRROR_MAX_TICK              (300)
#define MIRROR_JOG_STEP_TICK         (10)
#define MIRROR_JOG_DUTY              (550U)
#define MIRROR_JOG_TIMEOUT_MS        (300U)
#define MIRROR_JOG_TOL_TICK          (3)
#define MIRROR_JOG_ISSUE_MS          (20U)
#define MIRROR_RESTORE_DUTY          (700U)
#define MIRROR_RESTORE_TIMEOUT_MS    (6000U)
#define MIRROR_RESTORE_TOL_TICK      (5)

#define SEAT_MIN_TICK                (-500)
#define SEAT_MAX_TICK                (500)
#define SEAT_JOG_STEP_TICK           (15)
#define SEAT_JOG_DUTY                (380U)
#define SEAT_JOG_TIMEOUT_MS          (300U)
#define SEAT_JOG_TOL_TICK            (3)
#define SEAT_JOG_ISSUE_MS            (20U)
#define SEAT_RESTORE_DUTY            (650U)
#define SEAT_RESTORE_TIMEOUT_MS      (6000U)
#define SEAT_RESTORE_TOL_TICK        (5)

#define DOOR_CLOSE_ANGLE_DEG         (0)
#define DOOR_OPEN_ANGLE_DEG          (90)
#define DOOR_INIT_ANGLE_DEG          (0)

#define MIRROR_TICK_PER_PROFILE_UNIT (1)
#define SEAT_TICK_PER_PROFILE_UNIT   (2)

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
    uint8                  activeProfileIdx;

    Shared_Profile_Table_t profileTable;
    App_Manager_Rfid_Output_t rfidOut;

    boolean                setupRestoreIssued;
    boolean                shutdownSaveIssued;
    boolean                shutdownParkIssued;
    boolean                emergencySeatIssued;
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

/* -------------------------------------------------------------------------------------------------
 * Local Helper prototypes
 * ------------------------------------------------------------------------------------------------- */
static uint32  App_GetNowMs(void);

static boolean App_ReadButton(Ifx_P *port, uint8 pinIdx);
static void    App_InitButtons(void);
static void    App_InitProfileTableDefault(void);
static void    App_InitMirrorAxis(void);
static void    App_InitSeatAxis(void);
static void    App_InitDoor(void);

static void    App_OnStateChanged(uint8 newState);

static sint32  App_ProfileToMirrorTick(uint8 mirrorAngle);
static sint32  App_ProfileToSeatTick(uint8 seatPos);
static uint8   App_MirrorTickToProfile(sint32 mirrorTick);
static uint8   App_SeatTickToProfile(sint32 seatTick);

static void    App_ApplyProfileByIndex(uint8 profileIdx);
static void    App_SaveCurrentPositionToActiveProfile(void);
static boolean App_IsRfidEnabledState(uint8 state);

static void    App_HandleButtons1ms(void);
static void    App_HandleState100ms(void);
static void    App_UpdateDebug(void);

/* CAN placeholders */
static void    App_HandleCanRx1ms(void);
static void    App_HandleCanTx10ms(void);

/* -------------------------------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------------------------------- */
static uint32 App_GetNowMs(void)
{
    uint32 tickPerMs = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1U);
    uint32 stmTick   = IfxStm_get(BSP_DEFAULT_TIMER);

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
    g_app.setupRestoreIssued  = FALSE;
    g_app.shutdownSaveIssued  = FALSE;
    g_app.shutdownParkIssued  = FALSE;
    g_app.emergencySeatIssued = FALSE;

    (void)newState;
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
    sint32 tick = ((sint32)seatPos - 128) * SEAT_TICK_PER_PROFILE_UNIT;

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

static boolean App_IsRfidEnabledState(uint8 state)
{
    if (state == SHARED_SYSTEM_STATE_SLEEP)     return TRUE;
    if (state == SHARED_SYSTEM_STATE_ACTIVATED) return TRUE;
    return FALSE;
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
 * CAN placeholders
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleCanRx1ms(void)
{
    uint32 rx_data[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32 rx_id;

    if (receiveCanMessage(rx_data) == FALSE)
    {
        return;
    }

    rx_id = g_multican.rxMsg.id;

    switch (rx_id)
    {
        case SHARED_CAN_MSG_ID_SS_STATE:
        {
            Shared_Can_State_t state_msg;

            (void)memset(&state_msg, 0, sizeof(Shared_Can_State_t));
            (void)memcpy(&state_msg,
                         (const uint8 *)rx_data,
                         sizeof(Shared_Can_State_t));

            // 상태 변경
            g_app.currentState = state_msg.current_state;

            UART_Printf("[RX] SS_STATE current_state=%u\r\n",
                        state_msg.current_state);
            break;
        }

        case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
        {
            Shared_Profile_Table_t profile_table_msg;

            (void)memset(&profile_table_msg, 0, sizeof(Shared_Profile_Table_t));
            (void)memcpy(&profile_table_msg,
                         (const uint8 *)rx_data,
                         sizeof(Shared_Profile_Table_t));

            // 프로필 테이블 저장
            /*memset(&g_app.profileTable,
                   (const uint8 *)profile_table_msg.profile,
                   sizeof(Shared_Profile_Table_t));*/


            UART_Printf("[RX] SS_PROFILE_TABLE received\r\n");
            break;
        }

        default:
        {
            break;
        }
    }
}

static void App_HandleCanTx10ms(void)
{
    uint32 tx_data[MAXIMUM_CAN_DATA_PAYLOAD];

    if ((g_app.rfidOut.event == APP_MANAGER_RFID_EVENT_SUCCESS) &&
        (g_app.rfidOut.uid_valid == TRUE))
    {
        (void)memset(tx_data, 0, sizeof(tx_data));

        ((uint8 *)tx_data)[0] = g_app.rfidOut.uid_idx;

        transmitCanMessage(SHARED_CAN_MSG_ID_AB_ACCESS_IDX, tx_data);

        UART_Printf("[TX] AB_ACCESS_IDX uid_idx=%u\r\n",
                    g_app.rfidOut.uid_idx);
    }

    /*
     * PROFILE_TABLE 송신은 "이 함수 안에 바로 쓸 수 있는 로컬 테이블 원본"이
     * 현재 스니펫에 없어서 일단 보류
     *
     * 나중에 테이블 원본 변수만 정해지면 아래 형태로 바로 추가:
     *
     * (void)memset(tx_data, 0, sizeof(tx_data));
     * (void)memcpy((uint8 *)tx_data,
     *              &your_profile_table,
     *              sizeof(Shared_Profile_Table_t));
     * transmitCanMessage(SHARED_CAN_MSG_ID_AB_PROFILE_TABLE, tx_data);
     */
}

/* -------------------------------------------------------------------------------------------------
 * State handling
 * ------------------------------------------------------------------------------------------------- */
static void App_HandleState100ms(void)
{
    switch (g_app.currentState)
    {
    case SHARED_SYSTEM_STATE_SLEEP:
        DoorActuator_Close(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_SETUP:
        DoorActuator_Open(&g_app.door);

        if (g_app.setupRestoreIssued == FALSE)
        {
            App_ApplyProfileByIndex(g_app.activeProfileIdx);
            g_app.setupRestoreIssued = TRUE;
        }
        break;

    case SHARED_SYSTEM_STATE_ACTIVATED:
        DoorActuator_Open(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_SHUTDOWN:
        DoorActuator_Close(&g_app.door);

        if (g_app.shutdownSaveIssued == FALSE)
        {
            App_SaveCurrentPositionToActiveProfile();
            /* TODO: 저장된 profile table CAN 송신 */
            g_app.shutdownSaveIssued = TRUE;
            break;
        }

        if (g_app.shutdownParkIssued == FALSE)
        {
            PositionAxis_Stop(&g_app.mirrorAxis);
            PositionAxis_Stop(&g_app.seatAxis);

            (void)PositionAxis_StartParkZero(&g_app.mirrorAxis);
            (void)PositionAxis_StartParkZero(&g_app.seatAxis);

            g_app.shutdownParkIssued = TRUE;
        }
        break;

    case SHARED_SYSTEM_STATE_DENIED:
        DoorActuator_Close(&g_app.door);
        break;

    case SHARED_SYSTEM_STATE_EMERGENCY:
    default:
        DoorActuator_Open(&g_app.door);

        if (g_app.emergencySeatIssued == FALSE)
        {
            PositionAxis_Stop(&g_app.seatAxis);
            (void)PositionAxis_StartRestore(&g_app.seatAxis, SEAT_MIN_TICK);
            g_app.emergencySeatIssued = TRUE;
        }
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

/* -------------------------------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------------------------------- */
void App_Init(void)
{
    App_Manager_Rfid_Input_t rfidIn;

    (void)memset(&g_app, 0, sizeof(g_app));

    App_InitButtons();
    App_InitProfileTableDefault();
    App_InitMirrorAxis();
    App_InitSeatAxis();
    App_InitDoor();

    initMultican();

    rfidIn.enable_flag   = FALSE;
    rfidIn.register_flag = FALSE;

    uint32 nowMs = App_GetNowMs();

    App_Manager_Rfid_Init(nowMs);
    App_Manager_Rfid_Run(nowMs, &rfidIn, &g_app.rfidOut);

    g_app.currentState     = SHARED_SYSTEM_STATE_SLEEP;
    g_app.prevState        = SHARED_SYSTEM_STATE_SLEEP;
    g_app.activeProfileIdx = SHARED_PROFILE_INDEX_DEFAULT;

    App_OnStateChanged(g_app.currentState);

    UART_Printf("[APP] init done\r\n");
}

void AppTask1ms(void)
{
    App_HandleCanRx1ms();

    PositionAxis_Task1ms(&g_app.mirrorAxis);
    PositionAxis_Task1ms(&g_app.seatAxis);

    App_HandleButtons1ms();
    App_UpdateDebug();
}

void AppTask10ms(void)
{
    App_Manager_Rfid_Input_t in;
    uint32 nowMs = App_GetNowMs();

    in.enable_flag   = App_IsRfidEnabledState(g_app.currentState);
    in.register_flag = FALSE;

    App_Manager_Rfid_Run(nowMs, &in, &g_app.rfidOut);

    if (g_app.rfidOut.event == APP_MANAGER_RFID_EVENT_SUCCESS)
    {
        if ((g_app.rfidOut.uid_idx >= 0) &&
            ((uint8)g_app.rfidOut.uid_idx < SHARED_PROFILE_NORMAL_COUNT))
        {
            g_app.activeProfileIdx = (uint8)g_app.rfidOut.uid_idx;
        }
        else
        {
            g_app.activeProfileIdx = SHARED_PROFILE_INDEX_DEFAULT;
        }

        // ★ TEST ONLY: 추후 수정해야 함
        g_app.currentState = (g_app.currentState == SHARED_SYSTEM_STATE_ACTIVATED) ? SHARED_SYSTEM_STATE_SLEEP : SHARED_SYSTEM_STATE_ACTIVATED;
        //g_app.currentState = SHARED_SYSTEM_STATE_SETUP;

        UART_Printf("[RFID] success idx=%d st=%u\r\n",
                    g_app.activeProfileIdx,
                    g_app.currentState);

        /* TODO: ACCESS_IDX CAN 송신 */
    }
    else if (g_app.rfidOut.event == APP_MANAGER_RFID_EVENT_FAIL)
    {
        UART_Printf("[RFID] fail st=%u\r\n", g_app.currentState);
    }
    else if (g_app.rfidOut.event == APP_MANAGER_RFID_EVENT_LOCKOUT)
    {
        UART_Printf("[RFID] lockout\r\n");
    }

    App_HandleCanTx10ms();
}

void AppTask100ms(void)
{
    App_HandleState100ms();
}

void AppTask1000ms(void)
{
    /*UART_Printf("[DBG] st=%u prev=%u prof=%u seat=%ld mirror=%ld door=%d\r\n",
                g_app.currentState,
                g_app.prevState,
                g_app.activeProfileIdx,
                (long)PositionAxis_GetTick(&g_app.seatAxis),
                (long)PositionAxis_GetTick(&g_app.mirrorAxis),
                (int)DoorActuator_GetState(&g_app.door));*/

    /* TODO:
     * 주기 CAN task 자리
     * - heartbeat
     * - profile table periodic tx
     */
}

void AppScheduling(void)
{
    if (stSchedulingInfo.u8nuScheduling1msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling1msFlag = 0U;
        AppTask1ms();
    }

    if (stSchedulingInfo.u8nuScheduling10msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling10msFlag = 0U;
        AppTask10ms();
    }

    if (stSchedulingInfo.u8nuScheduling100msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling100msFlag = 0U;
        AppTask100ms();
    }

    if (stSchedulingInfo.u8nuScheduling1000msFlag != 0U)
    {
        stSchedulingInfo.u8nuScheduling1000msFlag = 0U;
        AppTask1000ms();
    }
}
