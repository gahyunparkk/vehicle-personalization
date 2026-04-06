#ifndef APP_MANAGER_RFID_H_
#define APP_MANAGER_RFID_H_

#include "Base_Driver_Mfrc522.h"
#include "Platform_Types.h"

/* 외부로 알릴 이벤트 */
typedef enum
{
    APP_MANAGER_RFID_EVENT_NONE = 0,
    APP_MANAGER_RFID_EVENT_SUCCESS,
    APP_MANAGER_RFID_EVENT_FAIL,
    APP_MANAGER_RFID_EVENT_LOCKOUT
} App_Manager_Rfid_Event_t;

/* 외부 입력: 기능 활성 여부 / 등록 모드 여부 */
typedef struct
{
    boolean enable_flag;
    boolean register_flag;
} App_Manager_Rfid_Input_t;

/* 외부 출력: 이벤트와 UID */
typedef struct
{
    App_Manager_Rfid_Event_t event;
    boolean                  uid_valid;
    uint8                    uid_idx;
    Mfrc522_Uid              uid;
} App_Manager_Rfid_Output_t;

void App_Manager_Rfid_Init(uint32 now_ms);
void App_Manager_Rfid_Run(uint32 now_ms,
                          const App_Manager_Rfid_Input_t *input,
                          App_Manager_Rfid_Output_t *out);
#endif
