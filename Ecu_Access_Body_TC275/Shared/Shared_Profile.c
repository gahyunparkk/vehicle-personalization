#include "Shared_Profile.h"

static uint8 currentProfile = 0;
static Shared_Profile_t profiles[SHARED_PROFILE_TOTAL_COUNT];

void Profile_init(uint8 prof)
{
  currentProfile = prof;
}

uint8 getCurrentProfile(void)
{
  return currentProfile;
}

void setCurrentProfile(uint8 prof)
{
  currentProfile = prof;
}

Shared_Profile_t getProfile(uint8 prof)
{
  return profiles[prof - 1];
}

void updateProfile(uint8 prof, Shared_Profile_t up)
{
  profiles[prof - 1] = up;
}
