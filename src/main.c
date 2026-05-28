
/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "iostm8s003.h"
#include "eeprom.h"

#define LED_PORT       GPIOB
#define LED_PIN        GPIO_PIN_5
#define REL_OPEN_PORT  GPIOD
#define REL_OPEN_PIN   GPIO_PIN_3
#define REL_CLOSE_PORT GPIOD
#define REL_CLOSE_PIN  GPIO_PIN_2
#define BTN_PROG_PORT  GPIOD
#define BTN_PROG_PIN   GPIO_PIN_4
#define CTRL_IN_PORT   GPIOC
#define CTRL_IN_PIN    GPIO_PIN_5

#define LED_ON()        GPIO_WriteLow(LED_PORT, LED_PIN)
#define LED_OFF()       GPIO_WriteHigh(LED_PORT, LED_PIN)
#define LED_TOGGLE()    GPIO_WriteReverse(LED_PORT, LED_PIN)
#define REL_OPEN_ON()   GPIO_WriteHigh(REL_OPEN_PORT, REL_OPEN_PIN)
#define REL_OPEN_OFF()  GPIO_WriteLow(REL_OPEN_PORT, REL_OPEN_PIN)
#define REL_CLOSE_ON()  GPIO_WriteHigh(REL_CLOSE_PORT, REL_CLOSE_PIN)
#define REL_CLOSE_OFF() GPIO_WriteLow(REL_CLOSE_PORT, REL_CLOSE_PIN)
#define BTN_PROG_VAL()  GPIO_ReadInputPin(BTN_PROG_PORT, BTN_PROG_PIN)
#define CTRL_IN_VAL()   GPIO_ReadInputPin(CTRL_IN_PORT, CTRL_IN_PIN)

typedef enum
{
   STATE_STOP = 0,
   STATE_MOTOR_WORKING = 1
} state_t;

typedef enum
{
   NEXT_STATE_OPEN = 1,
   NEXT_STATE_CLOSE = 2
} next_state_t;

typedef enum
{
   CTRL_IN_STATE_OFF = 0,
   CTRL_IN_STATE_ON = 1
} ctrl_in_state_t;

/* Private variables ---------------------------------------------------------*/

static volatile state_t         state = STATE_STOP;
static volatile next_state_t    next_state = NEXT_STATE_OPEN;
static volatile uint16_t        motor_timer = 0;
static volatile ctrl_in_state_t ctrl_in_state = CTRL_IN_STATE_OFF;

/* Private functions prototypes ----------------------------------------------*/

void init_peripherals(void);
void motor_close(void);
void motor_open(void);
void motor_timer_isr(void);
void led_isr(void);
void ctrl_in_isr(void);
bool ctrl_in_is_pressed(void);

/**************************************************************************** */
/* MAIN LOOP                                                                  */
/**************************************************************************** */

void main(void)
{
   init_peripherals();

   enableInterrupts();

   // state = read_next_state_from_ee();

   while (true)
   {
      switch (state)
      {
         case STATE_STOP:
         {
            if (ctrl_in_is_pressed())
            {
               if (next_state == NEXT_STATE_CLOSE)
               {
                  motor_close();
                  next_state = NEXT_STATE_OPEN;
                  state = STATE_MOTOR_WORKING;
               }
               else if (next_state == NEXT_STATE_OPEN)
               {
                  motor_open();
                  next_state = NEXT_STATE_CLOSE;
                  state = STATE_MOTOR_WORKING;
               }
            }
         }
         break;

         case STATE_MOTOR_WORKING:
         {
            if (ctrl_in_is_pressed())
            {
               REL_CLOSE_OFF();
               REL_OPEN_OFF();
               motor_timer = 0;
               state = STATE_STOP;
            }
         }
         break;
      }
   }
}

/* Interrupt handling functions ----------------------------------------------*/

void t4_isr(void)
{
   static uint8_t tim_100ms = 0;

   TIM4_ClearITPendingBit(TIM4_IT_UPDATE);

   if (++tim_100ms == 100)
   {
      tim_100ms = 0;
      motor_timer_isr();
   }

   ctrl_in_isr();

   led_isr();
}

/* Private functions ---------------------------------------------------------*/

void init_peripherals(void)
{
   // set system clock to 16Mhz
   CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);

   GPIO_Init(LED_PORT, LED_PIN, GPIO_MODE_OUT_PP_HIGH_FAST);
   GPIO_Init(REL_CLOSE_PORT, REL_CLOSE_PIN, GPIO_MODE_OUT_PP_LOW_FAST);
   GPIO_Init(REL_OPEN_PORT, REL_OPEN_PIN, GPIO_MODE_OUT_PP_LOW_FAST);
   GPIO_Init(BTN_PROG_PORT, BTN_PROG_PIN, GPIO_MODE_IN_PU_NO_IT);
   GPIO_Init(CTRL_IN_PORT, CTRL_IN_PIN, GPIO_MODE_IN_PU_NO_IT);

   TIM4_TimeBaseInit(TIM4_PRESCALER_128, 123); // ~1ms int
   TIM4_ITConfig(TIM4_IT_UPDATE, ENABLE);
   TIM4_Cmd(ENABLE);

   FLASH_Unlock(FLASH_MEMTYPE_DATA);
}

void motor_close(void)
{
   REL_OPEN_OFF();
   REL_CLOSE_ON();
   motor_timer = 50;
}

void motor_open(void)
{
   REL_CLOSE_OFF();
   REL_OPEN_ON();
   motor_timer = 50;
}

void motor_timer_isr(void)
{
   if (motor_timer != 0)
   {
      if (--motor_timer == 0)
      {
         REL_CLOSE_OFF();
         REL_OPEN_OFF();
      }
   }
}

void led_isr(void)
{
   static uint16_t cnt = 0;

   ++cnt;

   if (cnt == 100) LED_OFF();
   else if (cnt == 2000)
   {
      cnt = 0;
      LED_ON();
   }
}

void ctrl_in_isr(void)
{
   static bool    wait_for_port_reset = false;
   static uint8_t timer = 100;

   if (wait_for_port_reset)
   {
      if (!CTRL_IN_VAL()) timer = 100;
      else if (--timer == 0) wait_for_port_reset = false;

      return;
   }

   if (ctrl_in_state == CTRL_IN_STATE_ON) return;

   if (CTRL_IN_VAL()) timer = 255;
   else if (--timer == 0)
   {
      ctrl_in_state = CTRL_IN_STATE_ON;
      wait_for_port_reset = true;
   }
}

bool ctrl_in_is_pressed(void)
{
   if (ctrl_in_state == CTRL_IN_STATE_ON)
   {
      ctrl_in_state = CTRL_IN_STATE_OFF;
      return true;
   }

   return false;
}

/* eeprom --------------------------------------------------------------------*/
