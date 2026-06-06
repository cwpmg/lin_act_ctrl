
/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "iostm8s003.h"
#include "eeprom.h"

/* IO pins ports -------------------------------------------------------------*/

#define LED_PORT       GPIOB
#define LED_PIN        GPIO_PIN_5
#define LED_RED_PORT   GPIOD
#define LED_RED_PIN    GPIO_PIN_5
#define LED_GREEN_PORT GPIOD
#define LED_GREEN_PIN  GPIO_PIN_6
#define REL_OPEN_PORT  GPIOD
#define REL_OPEN_PIN   GPIO_PIN_3
#define REL_CLOSE_PORT GPIOD
#define REL_CLOSE_PIN  GPIO_PIN_2
#define BTN_PROG_PORT  GPIOD
#define BTN_PROG_PIN   GPIO_PIN_4
#define CTRL_IN_PORT   GPIOC
#define CTRL_IN_PIN    GPIO_PIN_5

#define LED_ON()           GPIO_WriteLow(LED_PORT, LED_PIN)
#define LED_OFF()          GPIO_WriteHigh(LED_PORT, LED_PIN)
#define LED_TOGGLE()       GPIO_WriteReverse(LED_PORT, LED_PIN)
#define LED_RED_ON()       GPIO_WriteHigh(LED_RED_PORT, LED_RED_PIN)
#define LED_RED_OFF()      GPIO_WriteLow(LED_RED_PORT, LED_RED_PIN)
#define LED_RED_TOGGLE()   GPIO_WriteReverse(LED_RED_PORT, LED_RED_PIN)
#define LED_GREEN_ON()     GPIO_WriteHigh(LED_GREEN_PORT, LED_GREEN_PIN)
#define LED_GREEN_OFF()    GPIO_WriteLow(LED_GREEN_PORT, LED_GREEN_PIN)
#define LED_GREEN_TOGGLE() GPIO_WriteReverse(LED_GREEN_PORT, LED_GREEN_PIN)
#define REL_OPEN_ON()      GPIO_WriteHigh(REL_OPEN_PORT, REL_OPEN_PIN)
#define REL_OPEN_OFF()     GPIO_WriteLow(REL_OPEN_PORT, REL_OPEN_PIN)
#define REL_CLOSE_ON()     GPIO_WriteHigh(REL_CLOSE_PORT, REL_CLOSE_PIN)
#define REL_CLOSE_OFF()    GPIO_WriteLow(REL_CLOSE_PORT, REL_CLOSE_PIN)
#define BTN_PROG_VAL()     GPIO_ReadInputPin(BTN_PROG_PORT, BTN_PROG_PIN)
#define CTRL_IN_VAL()      GPIO_ReadInputPin(CTRL_IN_PORT, CTRL_IN_PIN)

/* Private defines -----------------------------------------------------------*/

#define MOTOR_RUN_TIME_MIN 40 // *100ms
#define MOTOR_RUN_TIME_MAX 900

#define BTN_PROG_PRESS_SHORT_TIME_MIN 150 // *1ms
#define BTN_PROG_PRESS_SHORT_TIME_MAX 1000
#define BTN_PROG_PRESS_LONG_TIME      3000
#define BTN_PROG_RELEASE_TIME         150

#define BLINK_TIME          250
#define BLINK_FAST_ON_TIME  70
#define BLINK_FAST_OFF_TIME 140
#define BLINK_SLOW_ON_TIME  70
#define BLINK_SLOW_OFF_TIME 2000

/* Private typedefs ----------------------------------------------------------*/

typedef enum
{
   STATE_STOP = 0,
   STATE_MOTOR_WORKING = 1,
   STATE_DELAY = 2,
   STATE_PROG_OPTIONS = 3
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
   BTN_PROG_RELEASED = 0,
   BTN_PROG_PRESSED_SHORT = 1,
   BTN_PROG_PRESSED_LONG = 2
} btn_prog_state_t;

typedef enum
{
   LOGIC_OPEN_STOP_CLOSE = 0,
   LOGIC_OPEN_CLOSE = 1,
   LOGIC_OPEN_CLOSE_WO_INTS = 2
} logic_t;

typedef enum
{
   PO_STATE_START = 0,
   PO_STATE_BLINK = 1,
   PO_STATE_WAIT_FOR_BTN = 2,
   PO_STATE_EXIT = 3
} po_state_t;

typedef enum
{
   LT_STATE_START = 0,
   LT_STATE_WAIT_FOR_BTN = 1,
   LT_STATE_OPENING = 2,
   LT_STATE_DELAY = 3,
   LT_STATE_CLOSING = 4,
   LT_STATE_EXIT = 5
} lt_state_t;

typedef enum
{
   LED = 1,
   LED_RED = 2,
   LED_GREEN = 3
} leds_t;

typedef enum
{
   LED_BLINK_SLOW = 0,
   LED_BLINK_FAST = 1
} led_blink_type_t;

/* Private variables ---------------------------------------------------------*/

static volatile logic_t      logic;
static volatile motor_time_t close_time;
static volatile motor_time_t open_time;
static volatile motor_time_t learn_timer;
static volatile next_state_t next_state;

static volatile state_t          state = STATE_STOP;
static volatile ctrl_in_state_t  ctrl_in_state = CTRL_IN_STATE_OFF;
static volatile btn_prog_state_t btn_prog_state = BTN_PROG_RELEASED;

static volatile uint16_t     led_on_time = BLINK_SLOW_ON_TIME;
static volatile uint16_t     led_off_time = BLINK_SLOW_OFF_TIME;
static volatile uint16_t     tick_timer = 0;
static volatile motor_time_t motor_timer = 0;

static volatile bool stop_led_isr_blink = false;
static volatile bool led_active = false;
static volatile bool led_red_active = false;
static volatile bool led_green_active = false;

/* Private functions prototypes ----------------------------------------------*/

void init_peripherals(void);
void restore_data_from_ee(void);
void motor_stop(void);
void motor_run(void);
void motor_timer_isr(void);
void led_isr(void);
void ctrl_in_isr(void);
bool ctrl_in_is_pressed(void);
void btn_prog_isr(void);
void blink(uint8_t times);
void program_options_handling(void);
void reset_buttons(void);
void leds_off(void);
void restore_slow_blinking(void);
void leds_blink_set(leds_t led_nb, led_blink_type_t blink_type);

btn_prog_state_t btn_prog_get_state(void);

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

            if (btn_prog_get_state() == BTN_PROG_PRESSED_LONG) state = STATE_PROG_OPTIONS;
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

         case STATE_PROG_OPTIONS:
         {
            program_options_handling();
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
      ++learn_timer;
   }

   ctrl_in_isr();

   btn_prog_isr();

   led_isr();

   if (tick_timer != 0) --tick_timer;
}

/* Private functions ---------------------------------------------------------*/

void init_peripherals(void)
{
   // set system clock to 16Mhz
   CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);

   GPIO_Init(LED_PORT, LED_PIN, GPIO_MODE_OUT_PP_HIGH_FAST);
   GPIO_Init(LED_RED_PORT, LED_RED_PIN, GPIO_MODE_OUT_PP_LOW_FAST);
   GPIO_Init(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_MODE_OUT_PP_LOW_FAST);
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

   restore_slow_blinking();
}

void motor_stop(void)
{
   REL_CLOSE_OFF();
   REL_OPEN_OFF();
   motor_timer = 0;
   led_on_time = BLINK_SLOW_ON_TIME;
   led_off_time = BLINK_SLOW_OFF_TIME;
}

void motor_run(void)
{
   if (next_state == NEXT_STATE_CLOSE)
   {
      next_state = NEXT_STATE_OPEN;
      save_next_state_to_ee(next_state);
      leds_blink_set(LED_RED, LED_BLINK_FAST);
      motor_timer = close_time;
      REL_CLOSE_ON();
   }
   else if (next_state == NEXT_STATE_OPEN)
   {
      next_state = NEXT_STATE_CLOSE;
      save_next_state_to_ee(next_state);
      leds_blink_set(LED_GREEN, LED_BLINK_FAST);
      motor_timer = open_time;
      REL_OPEN_ON();
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
         reset_buttons();
         led_on_time = BLINK_SLOW_ON_TIME;
         led_off_time = BLINK_SLOW_OFF_TIME;
      }
   }
}

void led_isr(void)
{
   static uint16_t cnt = 0;

   if (stop_led_isr_blink)
   {
      cnt = 0;
      return;
   }

   ++cnt;

   if (cnt == led_on_time)
   {
      LED_OFF();
      LED_GREEN_OFF();
      LED_RED_OFF();
   }

   if (cnt >= led_off_time)
   {
      cnt = 0;
      if (led_active) LED_ON();
      if (led_green_active) LED_GREEN_ON();
      if (led_red_active) LED_RED_ON();
   }
}

void leds_off(void)
{
   LED_OFF();
   LED_GREEN_OFF();
   LED_RED_OFF();
   stop_led_isr_blink = true;
}

void leds_blink_set(leds_t led_nb, led_blink_type_t blink_type)
{
   stop_led_isr_blink = true;

   led_active = false;
   led_green_active = false;
   led_red_active = false;

   if (led_nb == LED) led_active = true;
   else if (led_nb == LED_RED) led_red_active = true;
   else if (led_nb == LED_GREEN) led_green_active = true;

   if (blink_type == LED_BLINK_SLOW)
   {
      led_on_time = BLINK_SLOW_ON_TIME;
      led_off_time = BLINK_SLOW_OFF_TIME;
   }
   else if (blink_type == LED_BLINK_FAST)
   {
      led_on_time = BLINK_FAST_ON_TIME;
      led_off_time = BLINK_FAST_OFF_TIME;
   }

   stop_led_isr_blink = false;
}

void restore_slow_blinking(void)
{
   led_active = false;

   if (next_state == NEXT_STATE_OPEN) led_red_active = true;
   else led_green_active = true;

   led_on_time = BLINK_SLOW_ON_TIME;
   led_off_time = BLINK_SLOW_OFF_TIME;
   stop_led_isr_blink = false;
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

void btn_prog_isr(void)
{
   static bool     wait_for_release = false;
   static uint16_t btn_prog_isr_cnt = 0;

   if (wait_for_release)
   {
      if (!BTN_PROG_VAL()) btn_prog_isr_cnt = BTN_PROG_RELEASE_TIME;
      else if (--btn_prog_isr_cnt == 0) wait_for_release = false;
   }
   else
   {
      if (!BTN_PROG_VAL())
      {
         if (++btn_prog_isr_cnt == BTN_PROG_PRESS_LONG_TIME)
         {
            btn_prog_state = BTN_PROG_PRESSED_LONG;
            wait_for_release = true;
            btn_prog_isr_cnt = BTN_PROG_RELEASE_TIME;
         }
      }
      else
      {
         if ((btn_prog_isr_cnt >= BTN_PROG_PRESS_SHORT_TIME_MIN) && (btn_prog_isr_cnt <= BTN_PROG_PRESS_SHORT_TIME_MAX))
         {
            btn_prog_state = BTN_PROG_PRESSED_SHORT;
            wait_for_release = true;
            btn_prog_isr_cnt = BTN_PROG_RELEASE_TIME;
         }
         else
         {
            btn_prog_isr_cnt = 0;
         }
      }
   }
}

btn_prog_state_t btn_prog_get_state(void)
{
   btn_prog_state_t state;

   state = btn_prog_state;

   if (state > BTN_PROG_RELEASED)
   {
      btn_prog_state = BTN_PROG_RELEASED;
   }

   return state;
}

void reset_buttons(void)
{
   btn_prog_state = BTN_PROG_RELEASED;
   ctrl_in_state = CTRL_IN_STATE_OFF;
}

void blink(uint8_t times)
{
   while (times != 0)
   {
      tick_timer = BLINK_TIME;
      while (tick_timer != 0);
      LED_ON();
      tick_timer = BLINK_TIME;
      while (tick_timer != 0);
      LED_OFF();
      --times;
   }
}

void learn_open_close_time(void)
{
   static uint8_t lt_state = LT_STATE_START;

   motor_time_t open_time_temp = 0;
   motor_time_t close_time_temp = 0;

   while (true)
   {
      switch (lt_state)
      {
         case LT_STATE_START:
         {
            leds_blink_set(LED, LED_BLINK_FAST);
            tick_timer = 6000;
            lt_state = LT_STATE_WAIT_FOR_BTN;
         }
         break;

         case LT_STATE_WAIT_FOR_BTN:
         {
            if (btn_prog_get_state() == BTN_PROG_PRESSED_SHORT)
            {
               REL_OPEN_ON();
               lt_state = LT_STATE_OPENING;
               learn_timer = 0;
               break;
            }

            if (tick_timer == 0) lt_state = LT_STATE_EXIT;
         }
         break;

         case LT_STATE_OPENING:
         {
            if (learn_timer > MOTOR_RUN_TIME_MAX)
            {
               REL_OPEN_OFF();
               lt_state = LT_STATE_EXIT;
               break;
            }

            if (btn_prog_get_state() == BTN_PROG_PRESSED_SHORT)
            {
               REL_OPEN_OFF();
               open_time_temp = learn_timer;
               tick_timer = 1000;

               if (open_time_temp < MOTOR_RUN_TIME_MIN) lt_state = LT_STATE_EXIT;
               else lt_state = LT_STATE_DELAY;
            }
         }
         break;

         case LT_STATE_DELAY:
         {
            if (tick_timer == 0)
            {
               REL_CLOSE_ON();
               lt_state = LT_STATE_CLOSING;
               learn_timer = 0;
            }
         }
         break;

         case LT_STATE_CLOSING:
         {
            if (learn_timer > MOTOR_RUN_TIME_MAX)
            {
               REL_CLOSE_OFF();
               lt_state = LT_STATE_EXIT;
               break;
            }

            if (btn_prog_get_state() == BTN_PROG_PRESSED_SHORT)
            {
               REL_CLOSE_OFF();
               close_time_temp = learn_timer;

               if (close_time_temp >= MOTOR_RUN_TIME_MIN)
               {
                  close_time = close_time_temp;
                  open_time = open_time_temp;
                  save_open_time_to_ee(open_time);
                  save_close_time_to_ee(close_time);
                  next_state = NEXT_STATE_OPEN;
                  save_next_state_to_ee(next_state);
               }

               lt_state = LT_STATE_EXIT;
            }
         }
         break;

         case LT_STATE_EXIT:
         {
            leds_off();
            lt_state = LT_STATE_START;
            return;
         }
         break;
      }
   }
}

void program_options_handling(void)
{
   static uint8_t po_state = 0;
   static uint8_t opt_number = 0;

   switch (po_state)
   {
      case PO_STATE_START:
      {
         leds_off();
         opt_number = 0;

         LED_ON();
         tick_timer = 2000;
         while (tick_timer != 0);
         LED_OFF();
         tick_timer = 2000;
         while (tick_timer != 0);

         po_state = PO_STATE_BLINK;
         reset_buttons();
      }
      break;

      case PO_STATE_BLINK:
      {
         blink(1);

         if (++opt_number == 5)
         {
            po_state = PO_STATE_EXIT;
            break;
         }

         tick_timer = 2000;
         po_state = PO_STATE_WAIT_FOR_BTN;
      }
      break;

      case PO_STATE_WAIT_FOR_BTN:
      {
         if (btn_prog_get_state() == BTN_PROG_PRESSED_SHORT)
         {
            switch (opt_number)
            {
               case 1:
               {
                  logic = LOGIC_OPEN_STOP_CLOSE;
                  save_logic_to_ee(logic);
               }
               break;

               case 2:
               {
                  logic = LOGIC_OPEN_CLOSE;
                  save_logic_to_ee(logic);
               }
               break;

               case 3:
               {
                  logic = LOGIC_OPEN_CLOSE_WO_INTS;
                  save_logic_to_ee(logic);
               }
               break;

               case 4:
               {
                  learn_open_close_time();
               }
               break;
            }

            blink(opt_number);
            po_state = PO_STATE_EXIT;
            break;
         }

         if (tick_timer == 0) po_state = PO_STATE_BLINK;
      }
      break;

      case PO_STATE_EXIT:
      {
         tick_timer = 1000;
         while (tick_timer != 0);
         LED_ON();
         tick_timer = 2000;
         while (tick_timer != 0);
         LED_OFF();
         tick_timer = 1000;
         while (tick_timer != 0);

         state = STATE_STOP;
         po_state = PO_STATE_START;
         reset_buttons();
         restore_slow_blinking();
      }
      break;
   }
}
