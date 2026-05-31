#include "eeprom.h"
#include "iostm8s003.h"

@eeprom uint8_t  ee_next_state;
@eeprom uint8_t  ee_logic;
@eeprom uint16_t ee_close_time;
@eeprom uint16_t ee_open_time;

void save_next_state_to_ee(uint8_t state)
{
   ee_next_state = state;
}

uint8_t read_next_state_from_ee(void)
{
   return ee_next_state;
}

void save_logic_to_ee(uint8_t logic)
{
   ee_logic = logic;
}

uint8_t read_logic_from_ee(void)
{
   return ee_logic;
}

void save_close_time_to_ee(uint16_t time)
{
   ee_close_time = time;
}

uint16_t read_close_time_from_ee(void)
{
   return ee_close_time;
}

void save_open_time_to_ee(uint16_t time)
{
   ee_open_time = time;
}

uint16_t read_open_time_from_ee(void)
{
   return ee_open_time;
}