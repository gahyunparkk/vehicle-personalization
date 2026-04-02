#ifndef BASE_RC522_FSM_H_
#define BASE_RC522_FSM_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_RC522.h"
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define RC522_FSM_MATCH_THRESHOLD             3U
#define RC522_FSM_FAIL_LOCKOUT_THRESHOLD      3U
#define RC522_FSM_LOCKOUT_MS                  5000U
#define RC522_FSM_POLL_PERIOD_MS              50U
#define RC522_FSM_BLINK_INTERVAL_MS           200U
#define RC522_FSM_SUCCESS_BLINK_TOGGLES       4U   /* 2 blinks: ON/OFF x2 */
#define RC522_FSM_FAIL_BLINK_TOGGLES          6U   /* 3 blinks: ON/OFF x3 */

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions---------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum
{
    RC522_STATE_POLLING = 0,
    RC522_STATE_VERIFY_UID,
    RC522_STATE_FEEDBACK_SUCCESS,
    RC522_STATE_FEEDBACK_FAIL,
    RC522_STATE_WAIT_TAG_REMOVED,
    RC522_STATE_LOCKOUT
} RC522_State_t;

typedef enum
{
    RC522_LED_OFF = 0,
    RC522_LED_SUCCESS,
    RC522_LED_FAIL,
    RC522_LED_LOCKOUT
} RC522_LedPattern_t;

typedef struct
{
    RC522_State_t      state;
    RC522_LedPattern_t ledPattern;

    Rc522_Uid          candidateUid;
    Rc522_Uid          confirmedUid;

    uint8              matchCount;
    uint8              failStreak;
    uint8              blinkRemainingToggles;

    uint32             stateEnterTickMs;
    uint32             lastPollTickMs;
    uint32             lastBlinkTickMs;

    boolean            ledOn;
    boolean            successEvent;
    boolean            failEvent;
} RC522_Context_t;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void RC522_Fsm_Init(RC522_Context_t *context, uint32 nowMs);
void RC522_Fsm_Reset(RC522_Context_t *context, uint32 nowMs);
void RC522_Fsm_Step(RC522_Context_t *context, uint32 nowMs);

boolean RC522_Fsm_ConsumeSuccessEvent(RC522_Context_t *context);
boolean RC522_Fsm_ConsumeFailEvent(RC522_Context_t *context);

RC522_State_t RC522_Fsm_GetState(const RC522_Context_t *context);
RC522_LedPattern_t RC522_Fsm_GetLedPattern(const RC522_Context_t *context);
boolean RC522_Fsm_IsLedOn(const RC522_Context_t *context);
boolean RC522_Fsm_IsLockedOut(const RC522_Context_t *context);

const Rc522_Uid *RC522_Fsm_GetConfirmedUid(const RC522_Context_t *context);

#endif /* BASE_RC522_FSM_H_ */
