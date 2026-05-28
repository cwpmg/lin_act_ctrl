#include "eeprom.h"
#include "iostm8s003.h"

@eeprom uint8_t ee_next_state = 7;

void save_next_state_to_ee(uint8_t state)
{
   ee_next_state = state;
}

uint8_t read_next_state_from_ee(void)
{
   return ee_next_state;
}