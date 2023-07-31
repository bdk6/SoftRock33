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
#define MAX_OUTPUT_FREQ  11000000L
#define COUNTER_LENGTH   
static void DDS_init(void);
static void DDS_write_word(uint16_t wd);
static void DDS_write_frequency(uint32_t hz);
static void DDS_write_phase( uint16_t deg );

static uint8_t read_encoder(void);
static void int_to_string(int32_t);
//static uint8_t intstr[11];  // enough for 9 digits plus sign plus null
static uint8_t intstr[9];   // 8 digits plus null

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

static void keypad_remove(void)
{
        for (int i = KP_STRING_LENGTH - 1; i > 0; i--)
        {
                keypad_string[i] = keypad_string[i-1];
        }
        keypad_string[0] = ' ';
}


static void show_int_at( int x, int y, int32_t val)
{
        int_to_string(val);
        cli();
        LCD_44780_goto(x,y);
        LCD_44780_write_string(intstr);
        sei();
}


int main(int argc, char** argv)
{
        GPIO_pin_mode(GPIO_PIN_D2, GPIO_PIN_MODE_INPUT);
        
        for(int i = 1; i < 1000; i++);
        //SOFTSPI_init(1,2,3);
        LCD_44780_init2();
        LCD_44780_clear();
        LCD_44780_write_string("SoftRock 33");
        SYSTICK_init(CLK_DIV_64);
        BUTTON_init();
        SOFTSPI_init2();
        
        DDS_init();

        _delay_ms(2000);
        run();
        

        return 0;
}


void DDS_init(void)
{
        //SYSTICK_init(CLK_DIV_64);
        //BUTTON_init();
        
        //SOFTSPI_init(GPIO_PIN_B5, GPIO_PIN_B3, -1);
        //SOFTSPI_init2();
        
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
 
        current_mode = MODE_NORMAL;
        is_sweeping = 0;
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



// Save for later!
//static void int_to_string(int32_t num)
//{
//        int idx = 10;
//        uint8_t sign = ' ';
//        if(num < 0)
//        {
//                sign = '-';
//                num = -num;
//        }
//        for(int i = 0; i < idx; i++)
//        {
//                intstr[i] = ' ';
//        }
//        intstr[idx] = '\0'; // null
//        while(idx > 0)
//        {
//                idx--;
//                intstr[idx] = (uint8_t) (num % 10) + 0x30;
//                num /= 10;
//                if(num == 0)
//                {
//                        break;
//                }
//                
//        }
//        intstr[0] = sign;
//}



static void int_to_string(int32_t num)
{
        int idx = 8;
        intstr[idx] = '\0';
        for(int i = 0; i < idx; i++)
        {
                intstr[i] = ' ';
        }
        while(idx > 0)
        {
                idx--;
                intstr[idx] = (uint8_t) (num % 10) + 0x30;
                num /= 10;
                if(num == 0)
                {
                        break;
                }
        }
}

static int isdigit(uint8_t ch)
{
        int rtn = 0;
        if (ch >= '0' && ch <= '9')
        {
                rtn = 1;
        }
        return rtn;
}

static int32_t string_to_int(uint8_t* str)
{
        int32_t rtn = 0;
        while((*str != 0) && (!isdigit(*str)))
        {
                str++;
        }
        while(isdigit(*str))
        {
                rtn *= 10;
                rtn += *str - '0';
                str++;
        }
        return rtn;
}

typedef enum KEY_RESULT
{
        KEY_RESULT_NONE,
        KEY_RESULT_DIGIT,
        KEY_RESULT_ENTER,
        KEY_RESULT_MODE,
        KEY_RESULT_STORE,
        KEY_RESULT_RECALL,
        KEY_RESULT_DELETE,
        KEY_RESULT_MAX
}KeyResult_t;


// process a key
    
static KeyResult_t  process_key(int ky)
{
  KeyResult_t rtn = KEY_RESULT_NONE;
  if(ky >= 0)
  {
  uint8_t ch = keytable[ky];
  if(ch == '*')   // mode
  {
          rtn = KEY_RESULT_MODE;
  }
  else if (ch == '#') // enter
  {
          rtn = KEY_RESULT_ENTER;
  }
  else if (ch == 'B') // backspace
  {
          // delete one
          keypad_remove();
          rtn = KEY_RESULT_DELETE;
  }
  else if (ch == '?')  // I dunno
  {
          rtn = KEY_RESULT_NONE;
  }
  else if (ch == 's')   // store
  {
          rtn = KEY_RESULT_STORE;
  }
  else if (ch == 'r')  // recall
  {
          rtn = KEY_RESULT_RECALL;
  }
  else    // add digits
  {
          keypad_add(ch);
          rtn = KEY_RESULT_DIGIT;
  }
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
  cli();
  LCD_44780_clear();
  sei();
        
  while(1)
  {
    int32_t enc = ENCODER_get_count(0);
    if (enc < 0)
    {
      enc = 0;
      ENCODER_set_count(0,enc);
    }
    if (!is_sweeping && input_state == INPUT_STATE_SINGLE)
    {
      DDS_write_frequency(enc);
      // update display
    }
    show_int_at(0,0,enc);
        
    int b = BUTTON_get_button();
    int ky = KEYPAD_get_key();
    if(ky >= 0)
    {
            show_int_at(0,1,ky);
    }
    
    KeyResult_t key_result = process_key(ky);

    // TODO:  For now, we separate keypad and encoder
    // inputs.  Keypad entry must use ENTER key and
    // encoder entry must use button.  If keypad entry
    // is used, set freq to it and also set encoder count
    // to keypad number.  Later, we will integrate the two.
    // maybe ...
    switch(input_state)
    {
    case INPUT_STATE_SINGLE:
      if (b == 0)
      {
        input_state = INPUT_STATE_SINGLE_PAUSE;
      }
      else if (key_result == KEY_RESULT_ENTER)
      {
              int32_t f = string_to_int(keypad_string);
        if (f < MAX_OUTPUT_FREQ)
        {
                DDS_write_frequency(f);
                ENCODER_set_count(0,f);
                keypad_clear();
        }
        else
        {
                // TODO warn!
        }
      }
      else if (key_result == KEY_RESULT_MODE)
      {
        input_state = INPUT_STATE_F1;
        //int32_t e = ENCODER_get_count(0);
      
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
              int32_t f = ENCODER_get_count(0);
              DDS_write_frequency(f);
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
    cli();
    LCD_44780_goto(9,1);
    switch(input_state)
    {
    case INPUT_STATE_SINGLE:
            LCD_44780_write_string("Input   ");
            break;
    case INPUT_STATE_SINGLE_PAUSE:
            LCD_44780_write_string("Pause   ");
            break;
    case INPUT_STATE_F1:
            LCD_44780_write_string("F1      ");
            break;
    case INPUT_STATE_F2:
            LCD_44780_write_string("F2      ");
            break;
    case INPUT_STATE_TIME:
            LCD_44780_write_string("TIME    ");
            break;
    }
    sei();

    cli();
    LCD_44780_goto(8,0);
    LCD_44780_write_string(keypad_string);
    sei();
  }
}

                

