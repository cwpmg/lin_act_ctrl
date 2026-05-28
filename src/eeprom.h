#ifndef _EEPROM_H
#define _EEPROM_H

#include "stm8s.h"

void    save_next_state_to_ee(uint8_t state);
uint8_t read_next_state_from_ee(void);

#endif // !_EEPROM_H
