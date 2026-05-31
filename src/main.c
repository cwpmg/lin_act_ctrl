
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

#define MOTOR_RUN_TIME_MIN 40
#define MOTOR_RUN_TIME_MAX 600

typedef enum
{
   STATE_STOP = 0,
   STATE_MOTOR_WORKING = 1,
   STATE_DELAY = 2
} state_t;

typedef enum
{
   NEXT_STATE_OPEN = 0,
   NEXT_STATE_CLOSE = 1
} next_state_t;

typedef enum
{
   CTRL_IN_STATE_OFF = 0,
   CTRL_IN_STATE_ON = 1
} ctrl_in_state_t;

typedef enum
{
   LOGIC_OPEN_STOP_CLOSE = 0,
   LOGIC_OPEN_CLOSE = 1,
   LOGIC_OPEN_CLOSE_WO_INTS = 2
} logic_t;

/* Private variables ---------------------------------------------------------*/

static volatile state_t         state = STATE_STOP;
static volatile next_state_t    next_state;
static volatile uint16_t        motor_timer = 0;
static volatile ctrl_in_state_t ctrl_in_state = CTRL_IN_STATE_OFF;
static volatile uint16_t        tick_timer = 0;
static volatile logic_t         logic;
static volatile uint16_t        close_time;
static volatile uint16_t        open_time;

/* Private functions prototypes ----------------------------------------------*/

void init_peripherals(void);
void restore_data_from_ee(void);
void motor_close(void);
void motor_open(void);
void motor_stop(void);
void motor_run(void);
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

   restore_data_from_ee();

   enableInterrupts();

   while (true)
   {
      switch (state)
      {
         case STATE_STOP:
         {
            if (ctrl_in_is_pressed()) motor_run();
         }
         break;

         case STATE_MOTOR_WORKING:
         {
            if (ctrl_in_is_pressed())
            {
               if (logic == LOGIC_OPEN_STOP_CLOSE)
               {
                  motor_stop();
                  state = STATE_STOP;
               }
               else if (logic == LOGIC_OPEN_CLOSE)
               {
                  motor_stop();
                  tick_timer = 1000;
                  state = STATE_DELAY;
               }
            }
         }
         break;

         case STATE_DELAY:
         {
            if (tick_timer == 0) motor_run();
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

   if (tick_timer != 0) --tick_timer;
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

void restore_data_from_ee(void)
{
   next_state = read_next_state_from_ee();
   if (next_state > NEXT_STATE_CLOSE)
   {
      next_state = NEXT_STATE_OPEN;
      save_next_state_to_ee(next_state);
   }

   logic = read_logic_from_ee();
   if (logic > LOGIC_OPEN_CLOSE_WO_INTS)
   {
      logic = LOGIC_OPEN_STOP_CLOSE;
      save_logic_to_ee(logic);
   }

   close_time = read_close_time_from_ee();
   if ((close_time < MOTOR_RUN_TIME_MIN) || (close_time > MOTOR_RUN_TIME_MAX))
   {
      close_time = MOTOR_RUN_TIME_MIN;
      save_close_time_to_ee(close_time);
   }

   open_time = read_open_time_from_ee();
   if ((open_time < MOTOR_RUN_TIME_MIN) || (open_time > MOTOR_RUN_TIME_MAX))
   {
      open_time = MOTOR_RUN_TIME_MIN;
      save_open_time_to_ee(open_time);
   }
}

void motor_close(void)
{
   REL_CLOSE_ON();
   next_state = NEXT_STATE_OPEN;
   motor_timer = close_time;
}

void motor_open(void)
{
   REL_OPEN_ON();
   next_state = NEXT_STATE_CLOSE;
   motor_timer = open_time;
}

void motor_stop(void)
{
   REL_CLOSE_OFF();
   REL_OPEN_OFF();
   motor_timer = 0;
}

void motor_run(void)
{
   if (next_state == NEXT_STATE_CLOSE)
   {
      motor_close();
   }
   else if (next_state == NEXT_STATE_OPEN)
   {
      motor_open();
   }

   state = STATE_MOTOR_WORKING;
}

void motor_timer_isr(void)
{
   if (motor_timer != 0)
   {
      if (--motor_timer == 0)
      {
         REL_CLOSE_OFF();
         REL_OPEN_OFF();
         state = STATE_STOP;
      }
   }
}

void led_isr(void)
{
   static uint16_t cnt = 0;

   ++cnt;

   if (cnt == 80) LED_OFF();
   else if (cnt == 1000)
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
