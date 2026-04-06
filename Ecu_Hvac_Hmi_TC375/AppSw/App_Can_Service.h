#ifndef APP_CAN_SERVICE_H_
#define APP_CAN_SERVICE_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Manager_System.h"
#include "Platform_Types.h"
#include "Shared_Can_Message.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /*********************************************************************************************************************/
  /*------------------------------------------------Function Prototypes------------------------------------------------*/
  /*********************************************************************************************************************/

  /* CAN RX/TX adapter */
  boolean App_Can_Service_ReadFrame(Shared_Can_Frame_t *rx_frame);
  boolean App_Can_Service_WriteFrame(const Shared_Can_Frame_t *tx_frame);

  /* CAN RX routing */
  void App_Can_Service_HandleRxFrame(const Shared_Can_Frame_t *rx_frame);

  /* CAN TX builders */
  boolean App_Can_Service_BuildStateFrame(Shared_System_State_t current_state,
                                          Shared_Can_Frame_t *tx_frame);

  boolean App_Can_Service_BuildProfileIdxFrame(uint8 profile_idx,
                                               Shared_Can_Frame_t *tx_frame);

  boolean App_Can_Service_BuildTempFrame(sint8 temperature,
                                         Shared_Can_Frame_t *tx_frame);

  boolean App_Can_Service_BuildProfileTableFrame(const Shared_Profile_Table_t *profile_table,
                                                 Shared_Can_Frame_t *tx_frame);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_SERVICE_H_ */
