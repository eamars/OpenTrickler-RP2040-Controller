#ifndef CHARGE_PROFILES_RECORD_H_
#define CHARGE_PROFILES_RECORD_H_

#include "charge_profiles.h"

// The record in RAM, for the module's own settings' table (charge_profiles_settings.c)
eeprom_charge_profile_data_t *charge_profiles_record(void);

#endif  // CHARGE_PROFILES_RECORD_H_
