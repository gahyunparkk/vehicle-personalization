#include "App_Scheduler.h"

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Driver_Stm.h"

#include "App_Manager_System.h"
#include "App_Manager_Temp.h"
#include "MCMCAN_FD.h"
#include "Shared_Profile.h"
#include "Shared_Util_Time.h"

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Run_1ms(void);
static void App_Scheduler_Run_10ms(void);
static void App_Scheduler_Run_100ms(void);
static void App_Scheduler_Run_1s(void);
static void App_Scheduler_Run_10s(void);

/* task functions */
static void App_Scheduler_Task_CanRx(void);
static void App_Scheduler_Task_Temp(void);
static void App_Scheduler_Task_System(void);
static void App_Scheduler_Task_CanTx(void);

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/
static uint32                      g_app_scheduler_now_ms = 0u;
static sint16                      g_app_scheduler_local_temperature_x10 = 250; /* default 25.0C */
static App_Manager_System_Input_t  g_app_scheduler_system_input;
static App_Manager_System_Output_t g_app_scheduler_system_output;

/*********************************************************************************************************************/
/*------------------------------------------------Functions----------------------------------------------------------*/
/*********************************************************************************************************************/
void App_Scheduler_Init(void)
{
    Base_Driver_Stm_Init();

    App_Manager_System_Init();
    App_Manager_Temp_Init();
    initMcmcan();

    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;
}

void App_Scheduler_Run(void)
{
    Base_Driver_Stm_SchedulingFlag_t flags;

    Base_Driver_Stm_GetAndClearSchedulingFlags(&flags);

    if (flags.scheduling_1ms_flag != 0u)
    {
        App_Scheduler_Run_1ms();
    }

    if (flags.scheduling_10ms_flag != 0u)
    {
        App_Scheduler_Run_10ms();
    }

    if (flags.scheduling_100ms_flag != 0u)
    {
        App_Scheduler_Run_100ms();
    }

    if (flags.scheduling_1s_flag != 0u)
    {
        App_Scheduler_Run_1s();
    }

    if (flags.scheduling_10s_flag != 0u)
    {
        App_Scheduler_Run_10s();
    }
}

static void App_Scheduler_Run_1ms(void)
{
    /* 아주 짧고 자주 돌아야 하는 작업 */
    /* 예: watchdog service, debounce tick */
}

static void App_Scheduler_Run_10ms(void)
{
    /*
     * 메인 제어 루프
     * 1. CAN 수신
     * 2. 온도센서 FSM 처리
     * 3. 시스템 상태 관리
     * 4. CAN 송신
     */
    g_app_scheduler_now_ms = Shared_Util_Time_GetNowMs();

    App_Scheduler_Task_CanRx();
    App_Scheduler_Task_Temp();
    App_Scheduler_Task_System();
    App_Scheduler_Task_CanTx();
}

static void App_Scheduler_Run_100ms(void)
{
    /* 중간 주기 작업 */
    /* 예: 상태 로그, diagnostic flag update */
}

static void App_Scheduler_Run_1s(void)
{
    /*
     * DS18B20 같은 센서는 변환 요청과 읽기를 분리하는 편이 좋음
     * 여기서 주기적으로 측정 요청을 걸고,
     * 실제 FSM 진행/완료 판정은 10ms task에서 수행
     */
    (void)App_Manager_Temp_RequestUpdate();
}

static void App_Scheduler_Run_10s(void)
{
    /* 매우 느린 주기 작업 */
    /* 예: 통계, 유지보수용 상태 점검 */
}

/*********************************************************************************************************************/
/*------------------------------------------------Task Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * CAN 수신 태스크
 * - 다른 ECU에서 보낸 메시지를 읽어 system input에 반영
 * - 인증 결과, shutdown 요청, active profile index 등 입력값 갱신
 */
static void App_Scheduler_Task_CanRx(void)
{
    uint32 rx_data[16];

    /* 기본값 유지 */
    g_app_scheduler_system_input.auth_event_valid     = FALSE;
    g_app_scheduler_system_input.shutdown_request     = FALSE;
    g_app_scheduler_system_input.active_profile_index = SHARED_PROFILE_INDEX_INVALID;

    /*
     * TODO:
     * 실제 프로젝트의 CAN RX API로 교체
     *
     * 예시:
     * if (receiveCanMessage(rx_data) != FALSE)
     * {
     *     switch (g_mcmcan.rxMsg.messageId)
     *     {
     *         case MSG_xxx_AUTH_RESULT:
     *             g_app_scheduler_system_input.auth_event_valid = TRUE;
     *             break;
     *
     *         case MSG_xxx_SHUTDOWN_REQ:
     *             g_app_scheduler_system_input.shutdown_request = TRUE;
     *             break;
     *
     *         case MSG_xxx_ACTIVE_PROFILE:
     *             g_app_scheduler_system_input.active_profile_index = (uint8)rx_data[0];
     *             break;
     *
     *         default:
     *             break;
     *     }
     * }
     */
    (void)rx_data;
}

/*
 * 온도센서 태스크
 * - 센서 FSM을 계속 진행
 * - 유효한 최신 온도값을 캐시에 저장
 */
static void App_Scheduler_Task_Temp(void)
{
    sint16 temperature_x10;

    App_Manager_Temp_Run();

    /*
     * TODO:
     * 실제 Temp 모듈 API에 맞게 교체
     *
     * 예시:
     * if (App_Manager_Temp_GetLatestTemp_X10(&temperature_x10) == TRUE)
     * {
     *     g_app_scheduler_local_temperature_x10 = temperature_x10;
     * }
     */
    if (App_Manager_Temp_GetLatestTemp_X10(&temperature_x10) == TRUE)
    {
        g_app_scheduler_local_temperature_x10 = temperature_x10;
    }
}

/*
 * 시스템 상태 관리 태스크
 * - CAN 입력 + 로컬 온도 입력 기반으로 시스템 FSM 실행
 */
static void App_Scheduler_Task_System(void)
{
    App_Manager_System_Run(g_app_scheduler_now_ms,
                           g_app_scheduler_local_temperature_x10,
                           &g_app_scheduler_system_input,
                           &g_app_scheduler_system_output);
}

/*
 * CAN 송신 태스크
 * - 시스템 상태 관리 결과를 다른 ECU에 전파
 * - 상태정보, 온도값, 프로필테이블 변경사항 등을 송신
 */
static void App_Scheduler_Task_CanTx(void)
{
    uint32 tx_data[16] = {0};

    /*
     * TODO:
     * 실제 프로젝트의 CAN TX API / 메시지 ID에 맞게 교체
     *
     * 예시 1) 상태 정보 송신
     * tx_data[0] = (uint32)g_app_scheduler_system_output.system_state;
     * tx_data[1] = (uint32)g_app_scheduler_system_output.selected_profile_index;
     * sendCanMessage(MSG_STATE_INFO, tx_data, 8u);
     *
     * 예시 2) 온도 송신
     * tx_data[0] = (uint16)g_app_scheduler_local_temperature_x10;
     * sendCanMessage(MSG_TEMP_VALUE, tx_data, 8u);
     *
     * 예시 3) 프로필 테이블 변경 송신
     * if (g_app_scheduler_system_output.profile_table_updated == TRUE)
     * {
     *     sendCanMessage(MSG_PROFILE_TABLE, (uint32 *)g_app_scheduler_system_output.profile_table_bytes, 32u);
     * }
     */

    (void)tx_data;
}
