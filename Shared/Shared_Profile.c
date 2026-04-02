#include "Shared_Profile.h"

static uint8 currentProfile = 0;
static UserProfile_t profiles[MAX_PROFILES];

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

UserProfile_t getProfile(uint8 prof)
{
  return profiles[prof - 1];
}

void updateProfile(uint8 prof, UserProfile_t up)
{
  profiles[prof - 1] = up;
}