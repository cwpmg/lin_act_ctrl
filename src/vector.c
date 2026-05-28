/*	INTERRUPT VECTORS TABLE FOR STM8S103
 *	Copyright (c) 2008 by COSMIC Software
 */

#include "main.h"

extern void _stext(); /* startup routine */

@interrupt void TRAP_IRQHandler(void)
{
   while (1)
   {
      nop();
   }
}

@interrupt void TIM4_IRQHandler(void)
{
   t4_isr();
}

#pragma section const {vector }

void (*const @vector _vectab[32])() = {
    _stext,          /* RESET       */
    TRAP_IRQHandler, /* TRAP        */
    0,               /* TLI         */
    0,               /* AWU         */
    0,               /* CLK         */
    0,               /* EXTI0       */
    0,               /* EXTI1       */
    0,               /* EXTI2       */
    0,               /* EXTI3       */
    0,               /* EXTI4       */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* SPI         */
    0,               /* TIMER 1 OVF */
    0,               /* TIMER 1 CAP */
    0,               /* TIMER 2 OVF */
    0,               /* TIMER 2 CAP */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* UART1 TX    */
    0,               /* UART1 RX    */
    0,               /* I2C         */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* ADC1        */
    TIM4_IRQHandler, /* TIMER 4 OVF */
    0,               /* EEPROM ECC  */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* Reserved    */
    0,               /* Reserved    */
};
