/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_RC522_Fsm.h"
#include "Base_QSPI1.h"
#include "Base_RC522.h"
/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static boolean RC522_Fsm_IsTimeElapsed(uint32 nowMs, uint32 baseMs, uint32 periodMs);
static boolean RC522_Fsm_IsSameUid(const Rc522_Uid *lhs, const Rc522_Uid *rhs);
static void    RC522_Fsm_ClearUid(Rc522_Uid *uid);
static void    RC522_Fsm_ClearCandidate(RC522_Context_t *context);

static void RC522_Fsm_EnterPolling(RC522_Context_t *context, uint32 nowMs);
static void RC522_Fsm_EnterVerifyUid(RC522_Context_t *context, const Rc522_Uid *uid, uint32 nowMs);
static void RC522_Fsm_EnterFeedbackSuccess(RC522_Context_t *context, const Rc522_Uid *uid, uint32 nowMs);
static void RC522_Fsm_EnterFeedbackFail(RC522_Context_t *context, uint32 nowMs);
static void RC522_Fsm_EnterWaitTagRemoved(RC522_Context_t *context, uint32 nowMs);
static void RC522_Fsm_EnterLockout(RC522_Context_t *context, uint32 nowMs);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void RC522_Fsm_Init(RC522_Context_t *context, uint32 nowMs)
{
    if (context == NULL_PTR)
    {
        return;
    }
    QSPI1_Init();
    RC522_Init();
    context->state                 = RC522_STATE_POLLING;
    context->ledPattern            = RC522_LED_OFF;
    context->matchCount            = 0U;
    context->failStreak            = 0U;
    context->blinkRemainingToggles = 0U;

    context->stateEnterTickMs      = nowMs;
    context->lastPollTickMs        = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
    context->successEvent          = FALSE;
    context->failEvent             = FALSE;

    RC522_Fsm_ClearUid(&context->candidateUid);
    RC522_Fsm_ClearUid(&context->confirmedUid);
}


void RC522_Fsm_Reset(RC522_Context_t *context, uint32 nowMs)
{
    RC522_Fsm_Init(context, nowMs);
}


void RC522_Fsm_Step(RC522_Context_t *context, uint32 nowMs)
{
    Rc522_Status st;
    Rc522_Uid    uid;

    if (context == NULL_PTR)
    {
        return;
    }

    switch (context->state)
    {
    case RC522_STATE_POLLING:
    {
        if (RC522_Fsm_IsTimeElapsed(nowMs, context->lastPollTickMs, RC522_FSM_POLL_PERIOD_MS) == FALSE)
        {
            break;
        }

        context->lastPollTickMs = nowMs;

        st = RC522_ReadUid(&uid);
        if (st == RC522_STATUS_OK)
        {
            RC522_Fsm_EnterVerifyUid(context, &uid, nowMs);
        }
        break;
    }

    case RC522_STATE_VERIFY_UID:
    {
        if (RC522_Fsm_IsTimeElapsed(nowMs, context->lastPollTickMs, RC522_FSM_POLL_PERIOD_MS) == FALSE)
        {
            break;
        }

        context->lastPollTickMs = nowMs;

        st = RC522_ReadUid(&uid);
        if (st != RC522_STATUS_OK)
        {
            if (context->failStreak < 255U)
            {
                context->failStreak++;
            }

            RC522_Fsm_EnterFeedbackFail(context, nowMs);
            break;
        }

        if (RC522_Fsm_IsSameUid(&context->candidateUid, &uid) == FALSE)
        {
            if (context->failStreak < 255U)
            {
                context->failStreak++;
            }

            RC522_Fsm_EnterFeedbackFail(context, nowMs);
            break;
        }

        if (context->matchCount < 255U)
        {
            context->matchCount++;
        }

        if (context->matchCount >= RC522_FSM_MATCH_THRESHOLD)
        {
            context->failStreak = 0U;
            RC522_Fsm_EnterFeedbackSuccess(context, &uid, nowMs);
        }

        break;
    }

    case RC522_STATE_FEEDBACK_SUCCESS:
    {
        if (RC522_Fsm_IsTimeElapsed(nowMs, context->lastBlinkTickMs, RC522_FSM_BLINK_INTERVAL_MS) == FALSE)
        {
            break;
        }

        context->lastBlinkTickMs = nowMs;
        context->ledOn           = (boolean)!context->ledOn;

        if (context->blinkRemainingToggles > 0U)
        {
            context->blinkRemainingToggles--;
        }

        if (context->blinkRemainingToggles == 0U)
        {
            context->ledOn = FALSE;
            RC522_Fsm_EnterWaitTagRemoved(context, nowMs);
        }

        break;
    }

    case RC522_STATE_FEEDBACK_FAIL:
    {
        if (RC522_Fsm_IsTimeElapsed(nowMs, context->lastBlinkTickMs, RC522_FSM_BLINK_INTERVAL_MS) == FALSE)
        {
            break;
        }

        context->lastBlinkTickMs = nowMs;
        context->ledOn           = (boolean)!context->ledOn;

        if (context->blinkRemainingToggles > 0U)
        {
            context->blinkRemainingToggles--;
        }

        if (context->blinkRemainingToggles == 0U)
        {
            context->ledOn = FALSE;

            if (context->failStreak >= RC522_FSM_FAIL_LOCKOUT_THRESHOLD)
            {
                RC522_Fsm_EnterLockout(context, nowMs);
            }
            else
            {
                RC522_Fsm_EnterWaitTagRemoved(context, nowMs);
            }
        }

        break;
    }

    case RC522_STATE_WAIT_TAG_REMOVED:
    {
        if (RC522_Fsm_IsTimeElapsed(nowMs, context->lastPollTickMs, RC522_FSM_POLL_PERIOD_MS) == FALSE)
        {
            break;
        }

        context->lastPollTickMs = nowMs;

        st = RC522_ReadUid(&uid);
        if (st != RC522_STATUS_OK)
        {
            RC522_Fsm_EnterPolling(context, nowMs);
        }

        break;
    }

    case RC522_STATE_LOCKOUT:
    {
        context->ledPattern = RC522_LED_LOCKOUT;
        context->ledOn      = TRUE;

        if (RC522_Fsm_IsTimeElapsed(nowMs, context->stateEnterTickMs, RC522_FSM_LOCKOUT_MS) != FALSE)
        {
            context->failStreak = 0U;
            RC522_Fsm_EnterWaitTagRemoved(context, nowMs);
        }

        break;
    }

    default:
    {
        RC522_Fsm_EnterPolling(context, nowMs);
        break;
    }
    }
}

boolean RC522_Fsm_ConsumeSuccessEvent(RC522_Context_t *context)
{
    boolean ret;

    if (context == NULL_PTR)
    {
        return FALSE;
    }

    ret = context->successEvent;
    context->successEvent = FALSE;

    return ret;
}

boolean RC522_Fsm_ConsumeFailEvent(RC522_Context_t *context)
{
    boolean ret;

    if (context == NULL_PTR)
    {
        return FALSE;
    }

    ret = context->failEvent;
    context->failEvent = FALSE;

    return ret;
}

RC522_State_t RC522_Fsm_GetState(const RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return RC522_STATE_POLLING;
    }

    return context->state;
}

RC522_LedPattern_t RC522_Fsm_GetLedPattern(const RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return RC522_LED_OFF;
    }

    if (context->ledOn == FALSE)
    {
        return RC522_LED_OFF;
    }

    return context->ledPattern;
}

boolean RC522_Fsm_IsLedOn(const RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return context->ledOn;
}

boolean RC522_Fsm_IsLockedOut(const RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return FALSE;
    }

    return (boolean)(context->state == RC522_STATE_LOCKOUT);
}

const Rc522_Uid *RC522_Fsm_GetConfirmedUid(const RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return NULL_PTR;
    }

    return &context->confirmedUid;
}

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/
static boolean RC522_Fsm_IsTimeElapsed(uint32 nowMs, uint32 baseMs, uint32 periodMs)
{
    return (boolean)((uint32)(nowMs - baseMs) >= periodMs);
}

static boolean RC522_Fsm_IsSameUid(const Rc522_Uid *lhs, const Rc522_Uid *rhs)
{
    uint8 i;

    if ((lhs == NULL_PTR) || (rhs == NULL_PTR))
    {
        return FALSE;
    }

    if (lhs->size != rhs->size)
    {
        return FALSE;
    }

    for (i = 0U; i < lhs->size; ++i)
    {
        if (lhs->uid[i] != rhs->uid[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void RC522_Fsm_ClearUid(Rc522_Uid *uid)
{
    uint8 i;

    if (uid == NULL_PTR)
    {
        return;
    }

    uid->size = 0U;
    uid->sak  = 0U;

    for (i = 0U; i < (uint8)sizeof(uid->uid); ++i)
    {
        uid->uid[i] = 0U;
    }
}

static void RC522_Fsm_ClearCandidate(RC522_Context_t *context)
{
    if (context == NULL_PTR)
    {
        return;
    }

    RC522_Fsm_ClearUid(&context->candidateUid);
    context->matchCount = 0U;
}

static void RC522_Fsm_EnterPolling(RC522_Context_t *context, uint32 nowMs)
{
    RC522_Fsm_ClearCandidate(context);

    context->state                 = RC522_STATE_POLLING;
    context->ledPattern            = RC522_LED_OFF;
    context->blinkRemainingToggles = 0U;

    context->stateEnterTickMs      = nowMs;
    context->lastPollTickMs        = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
}

static void RC522_Fsm_EnterVerifyUid(RC522_Context_t *context, const Rc522_Uid *uid, uint32 nowMs)
{
    context->state                 = RC522_STATE_VERIFY_UID;
    context->ledPattern            = RC522_LED_OFF;
    context->candidateUid          = *uid;
    context->matchCount            = 1U;
    context->blinkRemainingToggles = 0U;

    context->stateEnterTickMs      = nowMs;
    context->lastPollTickMs        = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
}

static void RC522_Fsm_EnterFeedbackSuccess(RC522_Context_t *context, const Rc522_Uid *uid, uint32 nowMs)
{
    context->state                 = RC522_STATE_FEEDBACK_SUCCESS;
    context->ledPattern            = RC522_LED_SUCCESS;
    context->confirmedUid          = *uid;
    context->blinkRemainingToggles = RC522_FSM_SUCCESS_BLINK_TOGGLES;

    context->stateEnterTickMs      = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
    context->successEvent          = TRUE;
}

static void RC522_Fsm_EnterFeedbackFail(RC522_Context_t *context, uint32 nowMs)
{
    RC522_Fsm_ClearCandidate(context);

    context->state                 = RC522_STATE_FEEDBACK_FAIL;
    context->ledPattern            = RC522_LED_FAIL;
    context->blinkRemainingToggles = RC522_FSM_FAIL_BLINK_TOGGLES;

    context->stateEnterTickMs      = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
    context->failEvent             = TRUE;
}

static void RC522_Fsm_EnterWaitTagRemoved(RC522_Context_t *context, uint32 nowMs)
{
    RC522_Fsm_ClearCandidate(context);

    context->state                 = RC522_STATE_WAIT_TAG_REMOVED;
    context->ledPattern            = RC522_LED_OFF;
    context->blinkRemainingToggles = 0U;

    context->stateEnterTickMs      = nowMs;
    context->lastPollTickMs        = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = FALSE;
}

static void RC522_Fsm_EnterLockout(RC522_Context_t *context, uint32 nowMs)
{
    RC522_Fsm_ClearCandidate(context);

    context->state                 = RC522_STATE_LOCKOUT;
    context->ledPattern            = RC522_LED_LOCKOUT;
    context->blinkRemainingToggles = 0U;

    context->stateEnterTickMs      = nowMs;
    context->lastPollTickMs        = nowMs;
    context->lastBlinkTickMs       = nowMs;

    context->ledOn                 = TRUE;
}
