//////////////////////////////////////////////////////////////////////////////
///  @file softrock33.c
///  @copy William R Cooke 2023
///  @brief  Controls SoftRock33 signal generator.
//////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#define F_CPU 16000000
#include <util/delay.h>

#include <avr/interrupt.h>
#include "avrlib/systick.h"
#include "avrlib/gpio.h"
#include "avrlib/button.h"
#include "avrlib/softspi.h"
#include "avrlib/lcd_44780.h"
#include "avrlib/encoder.h"


#define MASTER_CLOCK     25000000L
#define COUNTER_LENGTH   
static void DDS_init(void);
static void DDS_write_word(uint16_t wd);
static void DDS_write_frequency(uint32_t hz);
static void DDS_write_phase( uint16_t deg );

static uint8_t read_encoder(void);
static void int_to_string(int32_t);
static uint8_t intstr[11];  // enough for 9 digits plus sign plus null

// Translate keypad scan codes to characters
//    7  8  9  >
//    4  5  6  <
//    1  2  3  -
//    *  0  #  +

static const uint8_t keytable[] =
{
        '*', '1','4','7','0','2','5','8','#','3','6','9','r','s','?','B'
};

// Place to put incoming keypad string
// For up to 99mhz we need 8 chars plus a null
#define KP_STRING_LENGTH    (8+1)
static uint8_t keypad_string[KP_STRING_LENGTH];
static uint8_t kp_string_index = 0;


// ///////////////////////   Screen Layout  ///////////////////////////////////
//
//  | current freq       time           |
//  | input 1            input 2        |
//
//

typedef enum MODE
{
        MODE_NORMAL,
        MODE_WAIT,
        MODE_SWEEP,
        MODE_LAST
} Mode_t;

typedef enum INPUT_STATE
{
        INPUT_STATE_SINGLE,
        INPUT_STATE_SINGLE_PAUSE,
        INPUT_STATE_F1,
        INPUT_STATE_F2,
        INPUT_STATE_TIME,
        INPUT_STATE_STORE,
        INPUT_STATE_RECALL,
        INPUT_STATE_UNDEFINED
} InputState_t;

static Mode_t current_mode;
static uint8_t is_sweeping;
static InputState_t input_state = INPUT_STATE_SINGLE;


// frequency control
static uint32_t frequency;
static uint32_t frequency_1;
static uint32_t frequency_2;
static uint32_t seconds;
static float hz_per_ms;

static void run(void);

// 9833 Control Word (address 00)
// D15  Address 0
// D14  Address 0
// D13  B28   1 = full 28 bit write, 0 = low or high 14 bits of freq reg
// D12  HLB   1 = write MSBs, 0 = write LSBs
// D11  Fsel  Select Freq Reg 0 (0) or 1 (1)
// D10  Psel  Select Phase Reg 0 (0) or 1 (1)
// D9   Reserved
// D8   Reset 1 to RESET, 0 to run
// D7   SLEEP1  1: internal MCLK disabled, 0 clock enabled
// D6   SLEEP12 1: power down DAC, 0: DAC is active
// D5   OPBITEN 1: disable DAC output, MSB or MSB/2.  0: enable DAC output
// D4   Reserved
// D3   DIV2    If OPBITEN = 1 (sq wave out) this divides by 2
// D2   Reserved
// D1   Mode    1: bypass SIN ROM, triangle out (OPBITEN 0) 0: use ROM
// D0   Reserved

static void keypad_clear(void)
{
        for(int i = 0; i < (8); i++) // KP_STRING_LENGTH - 1); i++)
        {
                keypad_string[i] = ' ';
        }
        keypad_string[KP_STRING_LENGTH - 1] = '\000';
        kp_string_index = 0;
}

static void keypad_add(uint8_t ch)
{
        for(int i = 1; i < KP_STRING_LENGTH - 1; i++)
        {
                keypad_string[i-1] = keypad_string[i];
        }
        keypad_string[KP_STRING_LENGTH - 2] = ch;
}

static void keypadd_remove(void)
{
        for (int i = KP_STRING_LENGTH - 1; i > 0; i--)
        {
                keypad_string[i] == keypad_string[i-1];
        }
        keypad_string[0] = ' ';
}

        
int main(int argc, char** argv)
{
        GPIO_pin_mode(GPIO_PIN_D2, GPIO_PIN_MODE_INPUT);
        
        for(int i = 1; i < 1000; i++);
        //SOFTSPI_init(1,2,3);
        DDS_init();

        uint8_t enc = read_encoder();
        run();
        

        return 0;
}


void DDS_init(void)
{
        LCD_44780_init2();
        LCD_44780_clear();
        LCD_44780_write_string("Hello again, DDS!");
        SYSTICK_init(CLK_DIV_64);
        BUTTON_init();
        
        //SOFTSPI_init(GPIO_PIN_B5, GPIO_PIN_B3, -1);
        SOFTSPI_init2();
        // ss on pb2
        SOFTSPI_set_interface(0, GPIO_PIN_C5, 16, SPI_MODE_2_MSB_FIRST, 0);
        DDRB |= 0x08;  // b3 output
        ENCODER_init();
        ENCODER_set_count(0,0);
        keypad_clear();
        
        KEYPAD_init();
        
        DDS_write_word(0x2100);  // B28 and RESET
        DDS_write_frequency(60000L);  // 60 KHz
        DDS_write_phase(0);
        DDS_write_word(0x2000);  // Keep B2B set, take out of reset
        _delay_ms(2000);
        LCD_44780_clear();
        LCD_44780_write_string("60000 Hz");
        sei();  // turn on interrupts


        current_mode = MODE_NORMAL;
        is_sweeping = 0;
        
        int32_t cnt = 0;
        while(1)
        {
                _delay_ms(1000);
                int_to_string(cnt);
                //   LCD_44780_clear();
                //  LCD_44780_write_string(intstr);
                cnt++;
                
                int32_t e = ENCODER_get_count(0);
                if (e < 0)
                {
                        e = 0;
                        ENCODER_set_count(0,0);
                }
                //   int buttons = BUTTON_waiting();
                int_to_string(e);
                //int_to_string(SYSTICK_get_milliseconds());
                //LCD_44780_clear();
                cli();
                LCD_44780_goto(0,1);
                sei();
                // LCD_44780_write_string(intstr);
                int buttons = BUTTON_waiting();
                //LCD_44780_clear();
                //  int_to_string(buttons);
                //  LCD_44780_write_string(intstr);
                
                if(buttons )
                {
                        int b = BUTTON_get_button();
                        // set freq
                        //LCD_44780_clear();
                        //         LCD_44780_write_string("PRESS:");
                        int_to_string(b * 10000);
                        //          LCD_44780_write_string(intstr);
                }
                int waiting = KEYPAD_waiting();
                int_to_string(waiting);

                cli();
                LCD_44780_goto(0,1);
                //  LCD_44780_write_string("Key");
                   LCD_44780_write_string(keypad_string);
                //  LCD_44780_write_string("END");
                sei();
        }
                

}

void DDS_write_word(uint16_t wd)
{
        SOFTSPI_write(0,wd);
}

void DDS_write_frequency(uint32_t hz)
{
        uint32_t n = (uint32_t)((uint64_t) hz * (1L << 28) / MASTER_CLOCK);
        DDS_write_word( (uint16_t)(n & 0x3fff) | 0x4000);
        DDS_write_word( (uint16_t)((n >> 14) & 0x3fff) | 0x4000);

}

void DDS_write_phase(uint16_t deg)
{
        deg %= 360;
        uint16_t ph = (uint16_t)(4096L * deg / 360);
        // write it to phase 0 reg
        ph |= 0xc000; // Set D15, D14 to select ph0
        DDS_write_word(ph);
}


//uint8_t read_encoder(void)
//{
//        uint8_t rtn = 0;
//        // pd2 and pd3

//        rtn = GPIO_read_pin(GPIO_PIN_D2) << 1;
//        rtn |= GPIO_read_pin(GPIO_PIN_D3);
//        return rtn;
//}

static void int_to_string(int32_t num)
{
        int idx = 10;
        uint8_t sign = ' ';
        if(num < 0)
        {
                sign = '-';
                num = -num;
        }
        
        intstr[idx] = '\0'; // null
        while(idx > 0)
        {
                idx--;
                intstr[idx] = (uint8_t) (num % 10) + 0x30;
                num /= 10;
        }
        intstr[0] = sign;
}


typedef enum KEY_RESULT
{
        KEY_RESULT_NONE,
        KEY_RESULT_DIGIT,
        KEY_RESULT_ENTER,
        KEY_RESULT_MODE,
        KEY_RESULT_STORE,
        KEY_RESULT_RECALL,
        KEY_RESULT_MAX
}KeyResult_t;


// process a key
    
static KeyResult_t  process_key(uint8_t ky)
{
  KeyResult_t rtn = 0;
  uint8_t ch = keytable[ky];
  if(ky == '*')   // mode
  {
          rtn = KEY_RESULT_MODE;
  }
  else if (ky == '#') // enter
  {
          rtn = KEY_RESULT_ENTER;
  }
  else if (ky == 'b') // backspace
  {
          // delete one
  }
  else if (ky == '?')  // I dunno
  {
  }
  else if (ky == 's')   // store
  {
          rtn = KEY_RESULT_STORE;
  }
  else if (ky == 'r')  // recall
  {
          rtn = KEY_RESULT_RECALL;
  }
  else    // add digits
  {
          keypad_add(ky);
  }


  return rtn;
}


// input state:
//     single
//     single pause
//     sweep f1
//     sweep f2
//     sweep time
//     storing    [st][digit]
//     recalling  [rcl][digit]

static void run(void)
{
        /*
        while(1)
        {
                // sweeping?
                if (is_sweeping != 0)
                {
                }
                // process keys
                // process encoder
                
        }
        
        // sweep?
        // process keys
        int ky = KEYPAD_get_key();
        int key_result = 0;
        if(ky >= 0)
        {
                key_result = process_key( (uint8_t) ky);
        }
        
        //    get rtn
        // process encoder
        int32_t enc = ENCODER_get_count(0);
        if (enc < 0)
        {
                enc = 0;
                ENCODER_set_count(0,enc);
        }
        
        
        //    get rtn
        // process button
        int b = BUTTON_get_button();
        if(b == 0 || key_result == KEY_ENTER)  // button 0
        {
                // First two only for button press
                if (input_state == INPUT_STATE_NORMAL && b == 0)
                {
                        input_state = INPUT_STATE_NORMAL_PAUSE;
                }
                else if (input_state == INPUT_STATE_NORMAL_PAUSE && b == 0)
                {
                        DDS_write_frequency( (uint32_t) enc);
                        input_state = INPUT_STATE_NORMAL;
                        // update display
                        cli();
                        LCD_44780_goto(0,0);
                        int_to_string(enc);
                        LCD_44780_write_string(intstr);
                        sei();
                }
                // These for button or ENTER key (#)
                else if (input_state == INPUT_STATE_F1)
                {
                        // set f1
                        input_state = INPUT_STATE_F2;
                }
                else if (input_state == INPUT_STATE_F2)
                {
                        // set f2
                        input_state = INPUT_STATE_TIME;
                }
                else if (input_state == INPUT_STATE_TIME)
                {
                        // set time
                        // start sweep
                        is_sweeping = 1;
                        INPUT_STATE = INPUT_STATE_NORMAL;
                }

                // These only for ENTER key (#)
                else if ( input_state == xxx && key_result == KEY_ENTER)
                {
                }
                
        */
  int32_t enc = ENCODER_get_count(0);
  if (enc < 0)
  {
    enc = 0;
    ENCODER_set_count(0,enc);
  }
  if (!is_sweeping && input_state == INPUT_STATE_SINGLE)
  {
    write_frequency(enc);
    // update display
  }
        
  int b = BUTTON_get_button();
  int ky = KEYPAD_get_key();
  KeyResult_t key_result = process_key(ky);
  //////////////////  Alternate ///////////////////////
  switch(input_state)
  {
  case INPUT_STATE_SINGLE:
    if (b == 0)
    {
      input_state = INPUT_STATE_SINGLE_PAUSE;
    }
    else if (key_result == KEY_RESULT_MODE)
    {
      input_state = INPUT_STATE_F1;
    }
    else if (key_result == KEY_RESULT_STORE)
    {
    }
    else if (key_result == KEY_RESULT_RECALL)
    {
    }
    break;
    case INPUT_STATE_SINGLE_PAUSE:
    if (b == 0 || key_result == KEY_RESULT_ENTER)
    {
      // set freq
      input_state = INPUT_STATE_SINGLE;
    }
    break;
  case INPUT_STATE_F1:
    if (b == 0 || key_result == KEY_RESULT_ENTER)
    {
      // set f1
      input_state == INPUT_STATE_F2;
    }
    break;

  case INPUT_STATE_F2:
    if (b == 0 || key_result == KEY_RESULT_ENTER)
    {
      // set f2
      input_state == INPUT_STATE_TIME;
    }
    break;

  case INPUT_STATE_TIME:
    if (b == 0 || key_result == KEY_RESULT_ENTER)
    {
      // check for valid time
      // set time
      // calculate and start sweeping
      input_state == INPUT_STATE_SINGLE;
    }
    break;
  default:
    break;
  }
}

                

