#ifndef PROFILE_H_
#define PROFILE_H_

#include "Ifx_Types.h"

#define MAX_PROFILES 5

#ifndef USERPROFILE_T_
#define USERPROFILE_T_
/*
 * <Profile Structure>
 * Seat Encoder Motor           : 2 Bytes
 * Handle Encoder Motor         : 2 Bytes
 * Side Mirror Encoder Motor    : 2 Bytes
 * Heating Threshold Temperature: 1 Byte
 * Cooling Threshold Temperature: 1 Byte
 * Hue Value for Ambient Light  : 1 Bytes
 * Ambient Light Mode           : 1 Bytes
 * Bit Padding                  : 6 Bytes
 * Total                        : 16 Bytes
 */
typedef struct
{
  uint16 seatPosition;
  uint16 handlePosition;
  uint16 mirrorAngle;
  uint8 heatingTemp;
  uint8 coolingTemp;
  uint8 amb_hue;
  uint8 amb_mode;
  uint8 reserved[6];
} UserProfile_t;
#endif

#endif