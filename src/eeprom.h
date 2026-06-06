#ifndef _EEPROM_H
#define _EEPROM_H

#include "main.h"

void         save_next_state_to_ee(uint8_t state);
uint8_t      read_next_state_from_ee(void);
void         save_logic_to_ee(uint8_t logic);
uint8_t      read_logic_from_ee(void);
void         save_close_time_to_ee(motor_time_t time);
void         save_open_time_to_ee(motor_time_t time);
motor_time_t read_close_time_from_ee(void);
motor_time_t read_open_time_from_ee(void);

#endif // !_EEPROM_H
