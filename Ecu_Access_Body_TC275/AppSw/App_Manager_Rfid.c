/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Driver_Mfrc522.h"
#include "App_Manager_Rfid.h"
#include "IfxPort.h"
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* 인증 실패 누적 허용 횟수 */
#define APP_MANAGER_RFID_FAIL_LIMIT            3U

/* 연속 실패 제한 상태(lockout) 유지 시간 */
#define APP_MANAGER_RFID_LOCKOUT_MS            5000U

/* 성공/실패 피드백 상태 유지 시간 */
#define APP_MANAGER_RFID_SUCCESS_FEEDBACK_MS   500U
#define APP_MANAGER_RFID_FAIL_FEEDBACK_MS      500U

/* 카드 순간 검출 노이즈를 줄이기 위한 연속 polling 확인 횟수 */
#define APP_MANAGER_RFID_POLL_CONFIRM_COUNT    3U

/* 간단한 내부 등록 카드 DB 최대 개수 */
#define APP_MANAGER_RFID_DB_MAX_CARDS          16U

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * RFID 처리 상태 정의
 *
 * IDLE
 *   - RFID 기능 비활성 상태
 *
 * POLLING
 *   - 카드 존재 여부만 가볍게 polling 하는 상태
 *
 * VERIFY_UID
 *   - 실제 UID를 읽어서 등록/검증 판단을 수행하는 상태
 *
 * FEEDBACK_SUCCESS
 *   - 성공 이벤트를 외부에 알린 뒤, 일정 시간 성공 상태를 유지하는 상태
 *
 * FEEDBACK_FAIL
 *   - 실패 이벤트를 외부에 알린 뒤, 일정 시간 실패 상태를 유지하는 상태
 *
 * WAIT_TAG_REMOVED
 *   - 같은 카드가 계속 붙어 있을 때 중복 인식을 막기 위해 태그 제거를 기다리는 상태
 *
 * LOCKOUT
 *   - 인증 실패 누적 횟수 초과 시 일정 시간 인증을 막는 상태
 */
typedef enum
{
    APP_MANAGER_RFID_STATE_IDLE = 0,
    APP_MANAGER_RFID_STATE_POLLING,
    APP_MANAGER_RFID_STATE_VERIFY_UID,
    APP_MANAGER_RFID_STATE_FEEDBACK_SUCCESS,
    APP_MANAGER_RFID_STATE_FEEDBACK_FAIL,
    APP_MANAGER_RFID_STATE_WAIT_TAG_REMOVED,
    APP_MANAGER_RFID_STATE_LOCKOUT
} App_Manager_Rfid_State_t;

/*
 * 내부 UID 저장용 DB 엔트리
 *
 * used
 *   - 해당 슬롯 사용 여부
 *
 * uid
 *   - 등록된 카드 UID 정보
 */
typedef struct
{
    boolean      used;
    Mfrc522_Uid  uid;
} App_Manager_Rfid_DbEntry_t;

/*
 * RFID 매니저 내부 컨텍스트
 *
 * state
 *   - 현재 FSM 상태
 *
 * current_uid
 *   - 최근 읽은 UID
 *
 * has_current_uid
 *   - current_uid의 유효 여부
 *
 * detect_count
 *   - 카드 존재 연속 감지 횟수
 *
 * fail_count
 *   - 인증 실패 누적 횟수
 *
 * state_deadline_ms
 *   - success/fail feedback 종료 시각
 *
 * lockout_deadline_ms
 *   - lockout 종료 시각
 *
 * init_ok
 *   - MFRC522 드라이버 초기화 성공 여부
 */
typedef struct
{
    App_Manager_Rfid_State_t state;

    Mfrc522_Uid current_uid;
    boolean     has_current_uid;

    uint8       detect_count;
    uint8       fail_count;

    uint32      state_deadline_ms;
    uint32      lockout_deadline_ms;

    boolean     init_ok;
} App_Manager_Rfid_Context_t;

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
/* RFID 매니저 런타임 컨텍스트 */
static App_Manager_Rfid_Context_t g_app_manager_rfid_context;

/* 등록된 카드 UID를 저장하는 간단한 내부 DB */
static App_Manager_Rfid_DbEntry_t g_app_manager_rfid_db[APP_MANAGER_RFID_DB_MAX_CARDS];


/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* UID 관련 내부 유틸 함수 */
static void    App_Manager_Rfid_ClearUid(Mfrc522_Uid *uid);
static void    App_Manager_Rfid_CopyUid(Mfrc522_Uid *dst, const Mfrc522_Uid *src);
static boolean App_Manager_Rfid_IsUidEqual(const Mfrc522_Uid *a, const Mfrc522_Uid *b);

/* DB/컨텍스트/FSM 관련 내부 함수 */
static void    App_Manager_Rfid_ResetDb(void);
static void    App_Manager_Rfid_ResetContext(uint32 now_ms);
static void    App_Manager_Rfid_SetState(App_Manager_Rfid_State_t next_state, uint32 now_ms);

/* 카드 감지/UID 읽기 래퍼 함수 */
static boolean App_Manager_Rfid_IsCardPresent(void);
static boolean App_Manager_Rfid_ReadCardUid(Mfrc522_Uid *out_uid);

/* 내부 DB 조회/등록 함수 */
static sint8   App_Manager_Rfid_DbContains(const Mfrc522_Uid *uid);
static boolean App_Manager_Rfid_DbRegister(const Mfrc522_Uid *uid);

/* 외부 출력 구조체 생성 함수 */
static App_Manager_Rfid_Output_t App_Manager_Rfid_MakeOutput(App_Manager_Rfid_Event_t event,
                                                             const Mfrc522_Uid       *uid,
                                                             boolean                  uid_valid,
                                                             sint8                    uid_idx);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/*
 * RFID 매니저 초기화
 *
 * 수행 내용
 * 1. 내부 UID DB 초기화
 * 2. 런타임 컨텍스트 초기화
 * 3. MFRC522 드라이버 초기화
 *
 * 입력
 * - now_ms : 현재 시스템 시간(ms)
 *
 * 비고
 * - 드라이버 초기화 성공 시에만 이후 Run 함수가 정상 동작한다.
 */
void App_Manager_Rfid_Init(uint32 now_ms)
{
    /* 등록 DB 초기화 */
    App_Manager_Rfid_ResetDb();

    /* 내부 컨텍스트 초기화 */
    App_Manager_Rfid_ResetContext(now_ms);

    /* MFRC522 드라이버 초기화 수행 */
    g_app_manager_rfid_context.init_ok = Base_Driver_Mfrc522_Init();

    /* 초기화가 성공하면 IDLE 상태로 시작 */
    if (g_app_manager_rfid_context.init_ok == TRUE)
    {
        App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
    }
}

/*
 * RFID 매니저 주기 실행 함수
 *
 * 입력
 * - now_ms : 현재 시스템 시간(ms)
 * - input  : 외부 입력 구조체
 *            enable_flag   : RFID 기능 활성 여부
 *            register_flag : 현재 UID를 등록 모드로 처리할지 여부
 *
 * 출력
 * - 이벤트 정보와 UID 정보를 담은 출력 구조체
 *
 * 동작 개요
 * - enable_flag가 꺼져 있으면 비활성화 상태 유지
 * - 카드가 연속 검출되면 UID를 읽어 등록/검증 수행
 * - 성공/실패 결과를 피드백 상태로 일정 시간 유지
 * - 같은 카드 재인식을 막기 위해 태그 제거를 기다림
 * - 실패 누적 시 lockout 상태로 진입
 */
void  App_Manager_Rfid_Run(uint32 now_ms,
                           const App_Manager_Rfid_Input_t *input,
                           App_Manager_Rfid_Output_t *out)
{
    Mfrc522_Uid               uid;
    boolean                   enable_flag;
    boolean                   register_flag;

    /* 기본 출력값은 "이벤트 없음" */
    *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_NONE, NULL_PTR, FALSE, -1);

    /* 입력이 NULL일 가능성에 대비한 기본값 */
    enable_flag   = FALSE;
    register_flag = FALSE;

    /* 입력 포인터가 유효하면 외부 플래그 값을 가져온다 */
    if (input != NULL_PTR)
    {
        enable_flag   = input->enable_flag;
        register_flag = input->register_flag;
    }


    /* 드라이버 초기화 실패 시 어떠한 동작도 하지 않고 즉시 반환 */
    if (g_app_manager_rfid_context.init_ok == FALSE)
    {
        return;
    }

    /* 현재 상태에 따라 상태별 처리 수행 */
    switch (g_app_manager_rfid_context.state)
    {
    case APP_MANAGER_RFID_STATE_IDLE:
        /*
         * RFID 기능 비활성 기본 상태
         * enable_flag가 들어오면 polling 시작
         */
        if (enable_flag != FALSE)
        {
            g_app_manager_rfid_context.detect_count = 0U;
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_POLLING, now_ms);
        }
        break;

    case APP_MANAGER_RFID_STATE_POLLING:
        /*
         * 카드 존재 여부만 가볍게 확인하는 상태
         *
         * 목적
         * - 카드가 순간적으로 튀는 노이즈를 무시
         * - 연속 APP_MANAGER_RFID_POLL_CONFIRM_COUNT회 감지될 때만
         *   실제 UID 읽기 상태로 진입
         */
        if (enable_flag == FALSE)
        {
            /* 외부에서 기능을 끄면 초기화 후 IDLE 복귀 */
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (App_Manager_Rfid_IsCardPresent() == TRUE)
        {
            /* 카드가 검출되면 연속 감지 카운트 증가 */
            g_app_manager_rfid_context.detect_count++;

            /* 연속 확인 횟수에 도달하면 UID 검증 단계로 진입 */
            if (g_app_manager_rfid_context.detect_count >= APP_MANAGER_RFID_POLL_CONFIRM_COUNT)
            {
                g_app_manager_rfid_context.detect_count = 0U;
                App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_VERIFY_UID, now_ms);
            }
        }
        else
        {
            /* 중간에 카드 감지가 끊기면 연속 감지 카운트 초기화 */
            g_app_manager_rfid_context.detect_count = 0U;
        }
        break;

    case APP_MANAGER_RFID_STATE_VERIFY_UID:
        /*
         * UID 실제 읽기 및 등록/검증 수행 상태
         *
         * 처리 우선순위
         * 1. 이미 등록된 UID인가?
         * 2. 등록 모드인가?
         * 3. 둘 다 아니면 인증 실패
         */
        if (enable_flag == FALSE)
        {
            /* 외부 비활성화 요청 시 즉시 종료 */
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (App_Manager_Rfid_ReadCardUid(&uid) == TRUE)
        {
            /* UID 읽기에 성공하면 current_uid에 저장 */
            App_Manager_Rfid_CopyUid(&g_app_manager_rfid_context.current_uid, &uid);
            g_app_manager_rfid_context.has_current_uid = TRUE;

            /* 이미 등록된 카드라면 인증 성공 */
            sint8 idx = App_Manager_Rfid_DbContains(&uid);
            if (idx != -1)
            {
                /* 성공 시 실패 누적 카운트 초기화 */
                g_app_manager_rfid_context.fail_count = 0U;

                /* 성공 피드백 상태 진입 */
                App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_FEEDBACK_SUCCESS, now_ms);
                g_app_manager_rfid_context.state_deadline_ms = now_ms + APP_MANAGER_RFID_SUCCESS_FEEDBACK_MS;

                /* 외부로 success 이벤트 전달 */
                *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_SUCCESS, &uid, TRUE, idx);
            }
            /* 등록 모드라면 UID 신규 등록 시도 */
            else if (register_flag == TRUE)
            {
                if (App_Manager_Rfid_DbRegister(&uid) == TRUE)
                {
                    /* 등록 성공도 success로 처리 */
                    g_app_manager_rfid_context.fail_count = 0U;

                    App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_FEEDBACK_SUCCESS, now_ms);
                    g_app_manager_rfid_context.state_deadline_ms = now_ms + APP_MANAGER_RFID_SUCCESS_FEEDBACK_MS;

                    *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_SUCCESS, &uid, TRUE, idx);
                }
                else
                {
                    /*
                     * 등록 실패
                     * - DB가 가득 찬 경우
                     * - 내부 오류
                     * 등의 상황으로 해석 가능
                     */
                    g_app_manager_rfid_context.fail_count++;

                    /* 실패 누적이 기준 이상이면 lockout */
                    if (g_app_manager_rfid_context.fail_count >= APP_MANAGER_RFID_FAIL_LIMIT)
                    {
                        App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_LOCKOUT, now_ms);
                        g_app_manager_rfid_context.lockout_deadline_ms = now_ms + APP_MANAGER_RFID_LOCKOUT_MS;

                        *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_LOCKOUT, &uid, TRUE, -1);
                    }
                    else
                    {
                        /* 아직 lockout 기준 미만이면 일반 실패 처리 */
                        App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_FEEDBACK_FAIL, now_ms);
                        g_app_manager_rfid_context.state_deadline_ms = now_ms + APP_MANAGER_RFID_FAIL_FEEDBACK_MS;

                        *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_FAIL, &uid, TRUE, -1);
                    }
                }
            }
            else
            {
                /* 미등록 카드이고 등록 모드도 아니면 인증 실패 */
                g_app_manager_rfid_context.fail_count++;

                /* 실패 누적이 기준 이상이면 lockout 진입 */
                if (g_app_manager_rfid_context.fail_count >= APP_MANAGER_RFID_FAIL_LIMIT)
                {
                    App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_LOCKOUT, now_ms);
                    g_app_manager_rfid_context.lockout_deadline_ms = now_ms + APP_MANAGER_RFID_LOCKOUT_MS;

                    *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_LOCKOUT, &uid, TRUE, -1);
                }
                else
                {
                    /* 일반 실패 피드백 상태로 전이 */
                    App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_FEEDBACK_FAIL, now_ms);
                    g_app_manager_rfid_context.state_deadline_ms = now_ms + APP_MANAGER_RFID_FAIL_FEEDBACK_MS;

                    *out = App_Manager_Rfid_MakeOutput(APP_MANAGER_RFID_EVENT_FAIL, &uid, TRUE, -1);
                }
            }
        }
        else
        {
            /*
             * 카드 존재는 감지되었지만 UID 읽기에 실패한 경우
             * - RF 상태가 불안정하거나
             * - 충돌/타이밍 문제 등으로 읽지 못한 상황일 수 있음
             *
             * 현재 UID를 비우고 다시 polling 상태로 복귀
             */
            g_app_manager_rfid_context.has_current_uid = FALSE;
            App_Manager_Rfid_ClearUid(&g_app_manager_rfid_context.current_uid);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_POLLING, now_ms);
        }

        break;

    case APP_MANAGER_RFID_STATE_FEEDBACK_SUCCESS:
        /*
         * 성공 피드백 유지 상태
         * 일정 시간 유지 후 태그 제거 대기 상태로 이동
         */
        if (enable_flag == FALSE)
        {
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (now_ms >= g_app_manager_rfid_context.state_deadline_ms)
        {
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_WAIT_TAG_REMOVED, now_ms);
        }
        break;

    case APP_MANAGER_RFID_STATE_FEEDBACK_FAIL:
        /*
         * 실패 피드백 유지 상태
         * 일정 시간 유지 후 태그 제거 대기 상태로 이동
         */
        if (enable_flag == FALSE)
        {
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (now_ms >= g_app_manager_rfid_context.state_deadline_ms)
        {
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_WAIT_TAG_REMOVED, now_ms);
        }
        break;

    case APP_MANAGER_RFID_STATE_WAIT_TAG_REMOVED:
        /*
         * 같은 카드를 계속 대고 있는 동안은 재인식을 막는 상태
         *
         * 카드가 완전히 제거되면 다음 인식을 허용하기 위해
         * polling 상태로 돌아간다.
         */
        if (enable_flag == FALSE)
        {
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (App_Manager_Rfid_IsCardPresent() == FALSE)
        {
            /* 카드가 제거되면 UID/감지 카운트 초기화 후 polling 복귀 */
            g_app_manager_rfid_context.has_current_uid = FALSE;
            g_app_manager_rfid_context.detect_count    = 0U;
            App_Manager_Rfid_ClearUid(&g_app_manager_rfid_context.current_uid);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_POLLING, now_ms);
        }

        break;

    case APP_MANAGER_RFID_STATE_LOCKOUT:
        /*
         * 연속 실패 제한 상태
         *
         * lockout 시간 동안은 인증을 받지 않고,
         * 시간이 지나면 다시 polling 상태로 복귀한다.
         */
        if (enable_flag == FALSE)
        {
            App_Manager_Rfid_ResetContext(now_ms);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        }
        else if (now_ms >= g_app_manager_rfid_context.lockout_deadline_ms)
        {
            /* lockout 해제 시 내부 상태 초기화 */
            g_app_manager_rfid_context.fail_count      = 0U;
            g_app_manager_rfid_context.detect_count    = 0U;
            g_app_manager_rfid_context.has_current_uid = FALSE;
            App_Manager_Rfid_ClearUid(&g_app_manager_rfid_context.current_uid);
            App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_POLLING, now_ms);
        }

        break;

    default:
        /*
         * 예외 상태 방어 코드
         * 알 수 없는 상태에 빠지면 안전하게 초기 상태로 복귀
         */
        App_Manager_Rfid_ResetContext(now_ms);
        App_Manager_Rfid_SetState(APP_MANAGER_RFID_STATE_IDLE, now_ms);
        break;
    }

    return;
}

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 내부 등록 DB 전체 초기화
 *
 * 모든 슬롯을 미사용 상태로 만들고,
 * 저장된 UID 데이터를 0으로 초기화한다.
 */
static void App_Manager_Rfid_ResetDb(void)
{
    uint8 idx;

    for (idx = 0U; idx < APP_MANAGER_RFID_DB_MAX_CARDS; ++idx)
    {
        g_app_manager_rfid_db[idx].used = FALSE;
        App_Manager_Rfid_ClearUid(&g_app_manager_rfid_db[idx].uid);
    }

    g_app_manager_rfid_db[0].used = 1;
    g_app_manager_rfid_db[1].used = 1;

    g_app_manager_rfid_db[0].uid.size = 4;
    g_app_manager_rfid_db[1].uid.size = 4;

    g_app_manager_rfid_db[0].uid.uid[0] = 0x86; // big-endian
    g_app_manager_rfid_db[0].uid.uid[1] = 0xb8;
    g_app_manager_rfid_db[0].uid.uid[2] = 0x24;
    g_app_manager_rfid_db[0].uid.uid[3] = 0x07;

    g_app_manager_rfid_db[1].uid.uid[0] = 0x52; // big-endian
    g_app_manager_rfid_db[1].uid.uid[1] = 0xda;
    g_app_manager_rfid_db[1].uid.uid[2] = 0x24;
    g_app_manager_rfid_db[1].uid.uid[3] = 0x07;

}

/*
 * UID 구조체 초기화
 *
 * size, sak, uid 배열 전체를 0으로 만든다.
 */
static void App_Manager_Rfid_ClearUid(Mfrc522_Uid *uid)
{
    uint8 idx;

    if (uid == NULL_PTR)
    {
        return;
    }

    uid->size = 0U;

    for (idx = 0U; idx < (uint8)sizeof(uid->uid); ++idx)
    {
        uid->uid[idx] = 0U;
    }
}

/*
 * UID 구조체 복사
 *
 * src의 size, sak, uid 배열을 dst로 복사한다.
 */
static void App_Manager_Rfid_CopyUid(Mfrc522_Uid *dst, const Mfrc522_Uid *src)
{
    uint8 idx;

    if ((dst == NULL_PTR) || (src == NULL_PTR))
    {
        return;
    }

    dst->size = src->size;

    for (idx = 0U; idx < (uint8)sizeof(dst->uid); ++idx)
    {
        dst->uid[idx] = src->uid[idx];
    }
}

/*
 * 두 UID 비교
 *
 * 비교 기준
 * - size가 같아야 함
 * - uid 배열의 유효 길이(size)만큼 모든 바이트가 같아야 함
 *
 * 반환
 * - TRUE  : 동일 UID
 * - FALSE : 다름
 */
static boolean App_Manager_Rfid_IsUidEqual(const Mfrc522_Uid *a, const Mfrc522_Uid *b)
{
    uint8 idx;

    if ((a == NULL_PTR) || (b == NULL_PTR))
    {
        return FALSE;
    }

    if (a->size != b->size)
    {
        return FALSE;
    }

    for (idx = 0U; idx < a->size; ++idx)
    {
        if (a->uid[idx] != b->uid[idx])
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * 런타임 컨텍스트 초기화
 *
 * 주의
 * - init_ok는 여기서 건드리지 않는다.
 * - 드라이버 초기화 결과는 별도로 유지되어야 하기 때문이다.
 */
static void App_Manager_Rfid_ResetContext(uint32 now_ms)
{
    App_Manager_Rfid_ClearUid(&g_app_manager_rfid_context.current_uid);

    g_app_manager_rfid_context.state               = APP_MANAGER_RFID_STATE_IDLE;
    g_app_manager_rfid_context.has_current_uid     = FALSE;
    g_app_manager_rfid_context.detect_count        = 0U;
    g_app_manager_rfid_context.fail_count          = 0U;
    g_app_manager_rfid_context.state_deadline_ms   = now_ms;
    g_app_manager_rfid_context.lockout_deadline_ms = now_ms;
}

/*
 * 상태 전이 함수
 *
 * 공통적으로 상태를 바꾸고,
 * state_deadline_ms를 now_ms로 기본 설정한다.
 *
 * success/fail/lockout처럼 별도 deadline이 필요한 상태는
 * 상태 전이 후 호출부에서 구체적인 종료 시각을 다시 설정한다.
 */
static void App_Manager_Rfid_SetState(App_Manager_Rfid_State_t next_state, uint32 now_ms)
{
    g_app_manager_rfid_context.state             = next_state;
    g_app_manager_rfid_context.state_deadline_ms = now_ms;
}

/*
 * 카드 존재 여부만 확인하는 함수
 *
 * 내부적으로 MFRC522 RequestA 명령을 사용하여
 * 태그가 필드 내에 존재하는지를 확인한다.
 *
 * 반환
 * - TRUE  : 카드 존재
 * - FALSE : 카드 없음 또는 오류
 */
static boolean App_Manager_Rfid_IsCardPresent(void)
{
    uint8          atqa[2];
    Mfrc522_Status st;
    Mfrc522_Uid    uid;
    atqa[0] = 0U;
    atqa[1] = 0U;

    st = Base_Driver_Mfrc522_ReadUid(&uid);

    return (st == MFRC522_STATUS_OK) ? TRUE : FALSE;
}

/*
 * 카드 UID 실제 읽기 함수
 *
 * 내부적으로 MFRC522 UID 읽기 API를 호출한다.
 *
 * 반환
 * - TRUE  : UID 읽기 성공
 * - FALSE : 실패
 */
static boolean App_Manager_Rfid_ReadCardUid(Mfrc522_Uid *out_uid)
{
    Mfrc522_Status st;

    if (out_uid == NULL_PTR)
    {
        return FALSE;
    }

    st = Base_Driver_Mfrc522_ReadUid(out_uid);

    return (st == MFRC522_STATUS_OK) ? TRUE : FALSE;
}

/*
 * UID가 내부 DB에 이미 등록되어 있는지 확인
 *
 * 반환
 * - idx   : 이미 등록됨
 * - -1    : 등록되지 않음
 */
static sint8 App_Manager_Rfid_DbContains(const Mfrc522_Uid *uid)
{
    uint8 idx;

    if (uid == NULL_PTR)
    {
        return FALSE;
    }

    for (idx = 0U; idx < APP_MANAGER_RFID_DB_MAX_CARDS; ++idx)
    {
        if ((g_app_manager_rfid_db[idx].used == TRUE) &&
            (App_Manager_Rfid_IsUidEqual(&g_app_manager_rfid_db[idx].uid, uid) == TRUE))
        {
            return idx;
        }
    }

    return -1;
}

/*
 * UID를 내부 DB에 등록
 *
 * 동작
 * - 이미 등록된 UID면 성공으로 간주
 * - 빈 슬롯이 있으면 그 슬롯에 저장
 * - 빈 슬롯이 없으면 실패
 *
 * 반환
 * - TRUE  : 등록 성공 또는 이미 등록됨
 * - FALSE : 등록 실패
 */
static boolean App_Manager_Rfid_DbRegister(const Mfrc522_Uid *uid)
{
    uint8 idx;

    if (uid == NULL_PTR)
    {
        return FALSE;
    }

    /* 이미 있는 UID면 중복 등록 대신 성공 처리 */
    if (App_Manager_Rfid_DbContains(uid) == TRUE)
    {
        return TRUE;
    }

    /* 빈 슬롯을 찾아 등록 */
    for (idx = 0U; idx < APP_MANAGER_RFID_DB_MAX_CARDS; ++idx)
    {
        if (g_app_manager_rfid_db[idx].used == FALSE)
        {
            g_app_manager_rfid_db[idx].used = TRUE;
            App_Manager_Rfid_CopyUid(&g_app_manager_rfid_db[idx].uid, uid);
            return TRUE;
        }
    }

    /* 빈 슬롯이 없으면 등록 실패 */
    return FALSE;
}

/*
 * 외부 출력 구조체 생성
 *
 * event
 *   - 외부로 전달할 이벤트 종류
 *
 * uid
 *   - 이벤트와 함께 전달할 UID
 *
 * uid_valid
 *   - uid 데이터 유효 여부
 *
 * 동작
 * - uid_valid가 TRUE이고 uid 포인터가 유효하면 UID 복사
 * - 아니면 uid 필드를 0으로 초기화
 */
static App_Manager_Rfid_Output_t App_Manager_Rfid_MakeOutput(App_Manager_Rfid_Event_t event,
                                                             const Mfrc522_Uid       *uid,
                                                             boolean                  uid_valid,
                                                             sint8                    uid_idx)
{
    App_Manager_Rfid_Output_t out;

    out.event     = event;
    out.uid_valid = uid_valid;
    out.uid_idx   = uid_idx;

    if ((uid_valid != FALSE) && (uid != NULL_PTR))
    {
        App_Manager_Rfid_CopyUid(&out.uid, uid);
    }
    else
    {
        App_Manager_Rfid_ClearUid(&out.uid);
    }

    return out;
}
