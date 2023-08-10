//////////////////////////////////////////////////////////////////////////////
///  @file softrock33.c
///  @copy William R Cooke 2023
///  @brief  Controls SoftRock33 signal generator.
//////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <ctype.h>


#define F_CPU 16000000
#include <util/delay.h>

#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "avrlib/systick.h"
#include "avrlib/gpio.h"
#include "avrlib/button.h"
#include "avrlib/softspi.h"
#include "avrlib/lcd_44780.h"
#include "avrlib/encoder.h"
#include "avrlib/keypad.h"

// DDS input clock frequency
#define MASTER_CLOCK     25000000L
// DDS max output frequency + 1
#define MAX_OUTPUT_FREQ  11000000L
#define COUNTER_LENGTH

static void DDS_init(void);
static void DDS_write_word(uint16_t wd);
static void DDS_write_frequency(uint32_t hz);
static void DDS_write_phase( uint16_t deg );


// Storage for int_to_str function
static uint8_t intstr[9];   // 8 digits plus null

// Table to convert from keypad scan code to char
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

typedef enum INPUT_STATE
{
        INPUT_STATE_TRACK,
        INPUT_STATE_TRACK_PAUSE,
        INPUT_STATE_F1,
        INPUT_STATE_F2,
        INPUT_STATE_TIME,
        INPUT_STATE_SWEEP,
        INPUT_STATE_STORE,
        INPUT_STATE_RECALL,
        INPUT_STATE_UNDEFINED
} InputState_t;

//static Mode_t current_mode;
//static uint8_t is_sweeping;
//static InputState_t input_state = INPUT_STATE_TRACK;


// frequency control
//static uint32_t frequency;
//static uint32_t sweep_F1;
//static uint32_t sweep_F2;
//static uint32_t sweep_ms;
//static uint32_t seconds;
//     //static float hz_per_ms;
//static int32_t millihz_per_ms;

typedef struct SETTINGS
{
        InputState_t state;
        uint32_t frequency;
        uint32_t sweep_F1;
        uint32_t sweep_F2;
        uint32_t sweep_ms;
        uint32_t millhz_per_ms;
} settings_t;


settings_t current;
// EEPROM storage for settings
// 0-9 for sto/rcl -- 10 for "current"
settings_t saved_settings[11] __attribute__((section(".eeprom")));

// For use when STORING settings
InputState_t saved_state;

//static uint8_t read_encoder(void);
static void int_to_string(int32_t);

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
        keypad_string[KP_STRING_LENGTH-1] = '\0';
}


static void show_int_at( int x, int y, int32_t val)
{
        int_to_string(val);
        cli();
        LCD_44780_goto(x,y);
        LCD_44780_write_string(intstr);
        sei();
}


 
// wont work as is, not enough resolution for all circumstances
int32_t calc_millihz_per_millisec( int32_t f1, int32_t f2, int32_t time)
{
  int32_t ms = time * 1000;
  int32_t millihz = (f2 - f1) * 1000;
  int32_t millihz_per_ms = millihz / ms;
  if ( millihz_per_ms == 0)
  {
    millihz_per_ms = 1;
  }
  return millihz_per_ms;
}


int main(int argc, char** argv)
{
        GPIO_pin_mode(GPIO_PIN_D2, GPIO_PIN_MODE_INPUT);
        
        //      for(int i = 1; i < 1000; i++);
        LCD_44780_init2();
        LCD_44780_clear();
        LCD_44780_write_string("SoftRock 33");
        SYSTICK_init(CLK_DIV_64);
        BUTTON_init();
        SOFTSPI_init2();
        
        DDS_init();
        current.state = INPUT_STATE_TRACK;

        // eeprom test src dst n
        //eeprom_write_block(&current, 0, sizeof(settings_t));

        _delay_ms(2000);
        run();
        
        return 0;
}


void DDS_init(void)
{
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
 
        //current_mode = MODE_NORMAL;
        //is_sweeping = 0;
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
//////////////////////////////////////////////////////////////////////////////
/// @fn process_key
/// @brief Converts key code to char and does some processing/editing.
/// @param[in] ky Raw key code
/// @return Type of key that was pressed
//////////////////////////////////////////////////////////////////////////////
static KeyResult_t  process_key(int ky)
{
  KeyResult_t rtn = KEY_RESULT_NONE;
  if(ky >= 0)
  {
    uint8_t ch = keytable[ky];
    switch(ch)
    {
    case '*':
            rtn = KEY_RESULT_MODE;
            break;
    case '#':
            rtn = KEY_RESULT_ENTER;
            break;
    case 'B':
            // backspace
            keypad_remove();
            rtn = KEY_RESULT_DELETE;
            break;
    case '?':
            rtn = KEY_RESULT_NONE;
            break;
    case 's':
            rtn = KEY_RESULT_STORE;
            break;
    case 'r':
            rtn = KEY_RESULT_RECALL;
            break;
    default:   // Better be a digit!
            keypad_add(ch);
            rtn = KEY_RESULT_DIGIT;
            break;
    }
  }
  return rtn;
}

static void show_message(uint8_t* msg)
{
        cli();
        LCD_44780_goto(9,0);
        LCD_44780_write_string("       ");
        LCD_44780_goto(9,0);
        LCD_44780_write_string(msg);
        sei();
}

// input state:
//     tracking
//     tracking pause
//     sweep f1
//     sweep f2
//     sweep time
//     sweeping
//     storing    [st][digit]
//     recalling  [rcl][digit]

//////////////////////////////////////////////////////////////////////////////
/// @fn run
/// @brief Main loop: gets all inputs, processes, sets frequencies, etc.
//////////////////////////////////////////////////////////////////////////////
static void run(void)
{
  cli();
  LCD_44780_clear();
  sei();
  uint32_t prev_ms = SYSTICK_get_milliseconds();
        
  while(1)
  {
          // Wait for clock tick (1 ms)
          uint32_t new_ms;
          while ( (new_ms = SYSTICK_get_milliseconds()) == prev_ms);
          prev_ms = new_ms;
          
    int32_t enc = ENCODER_get_count(0);
    if (enc < 0)
    {
      enc = MAX_OUTPUT_FREQ + enc; //0;
      ENCODER_set_count(0,enc);
    }
    if (enc >= MAX_OUTPUT_FREQ)
    {
      enc -= MAX_OUTPUT_FREQ;
      ENCODER_set_count(0,enc);
    }
    if ( /*!is_sweeping && */ current.state == INPUT_STATE_TRACK)
    {
      DDS_write_frequency(enc);
      // update display
    }
    show_int_at(0,0,enc);
        
    int b = BUTTON_get_button();
    int ky = KEYPAD_get_key();
    
    KeyResult_t key_result = process_key(ky);

    // TODO:  For now, we separate keypad and encoder
    // inputs.  Keypad entry must use ENTER key and
    // encoder entry must use button.  If keypad entry
    // is used, set freq to it and also set encoder count
    // to keypad number.  Later, we will integrate the two.
    // maybe ...
    switch(current.state)
    {
    case INPUT_STATE_TRACK:
    {
      if (b == 0)
      {
        current.state = INPUT_STATE_TRACK_PAUSE;
      }
      else if (key_result == KEY_RESULT_ENTER)
      {
        int32_t f = string_to_int(keypad_string);
        if (f < MAX_OUTPUT_FREQ)
        {
                current.frequency = f;
          DDS_write_frequency(f);
          ENCODER_set_count(0,f);
          keypad_clear();
          //show_message("valid");
        }
        else
        {
          // TODO warn!
          show_message("high");
          keypad_clear();
                
        }
      }
      else if (key_result == KEY_RESULT_MODE)
      {
        current.state = INPUT_STATE_F1;
        //int32_t e = ENCODER_get_count(0);
      
      }
      else if (key_result == KEY_RESULT_STORE)
      {
              current.state = INPUT_STATE_STORE;
      }
      else if (key_result == KEY_RESULT_RECALL)
      {
              current.state = INPUT_STATE_RECALL;
      }
      break;
    } 
    case INPUT_STATE_TRACK_PAUSE:
      if (b == 0 || key_result == KEY_RESULT_ENTER)
      {
        // set freq
        int32_t f = ENCODER_get_count(0);
        DDS_write_frequency(f);
        current.state = INPUT_STATE_TRACK;
      }
      // check mode button
      // sto and rcl?
      break;
      
    case INPUT_STATE_F1:
      if (b == 0 || key_result == KEY_RESULT_ENTER)
      {
        // set f1
              current.sweep_F1 = string_to_int(keypad_string);
              keypad_clear();
        current.state = INPUT_STATE_F2;
      }
      break;

    case INPUT_STATE_F2:
      if (b == 0 || key_result == KEY_RESULT_ENTER)
      {
        // set f2
              current.sweep_F2 = string_to_int(keypad_string);
              keypad_clear();
        current.state = INPUT_STATE_TIME;
      }
      break;

    case INPUT_STATE_TIME:
      if (b == 0 || key_result == KEY_RESULT_ENTER)
      {
        // check for valid time
        // set time
        // calculate and start sweeping
              current.sweep_ms = string_to_int(keypad_string) * 1000;
              keypad_clear();
        current.state = INPUT_STATE_SWEEP;
      }
      break;
    case INPUT_STATE_SWEEP:
            if (b == 0 || key_result == KEY_RESULT_ENTER)
            {
                    current.state = INPUT_STATE_F1;
                    keypad_clear();
            }
            else if (key_result == KEY_RESULT_MODE)
            {
                    current.state = INPUT_STATE_TRACK;
                    // set freq, display, whatever
                    ENCODER_set_count(0, current.frequency);
                    DDS_write_frequency(current.frequency);
            }
            else if (key_result == KEY_RESULT_STORE)
            {
                    saved_state = current.state;
                    current.state = INPUT_STATE_STORE;
            }
            else if (key_result == KEY_RESULT_RECALL)
            {
                    //saved_state = current.state;
                    //current.state = INPUT_STATE_RECALL;
                    // set to loaded value
            }
            break;
    case INPUT_STATE_STORE:
            if (key_result == KEY_RESULT_DIGIT)
            {
                    // get string value
                    uint32_t entry = string_to_int(keypad_string);
                    // clear string
                    keypad_clear();
                    // save it
                    current.state = saved_state;
                    eeprom_write_block(&current, &saved_settings[entry],
                                       sizeof(settings_t));
                    //current.state = saved_state;
            }
            break;
    case INPUT_STATE_RECALL:
            if (key_result == KEY_RESULT_DIGIT)
            {
                    uint32_t entry = string_to_int(keypad_string);
                    keypad_clear();
                    eeprom_read_block(&current, &saved_settings[entry],
                                      sizeof(settings_t));
                                      //current.state = saved_state;
                    DDS_write_frequency(current.frequency);  // TODO mode
                    ENCODER_set_count(0, current.frequency); // TODO mode
            }
            break;
            
    default:
      break;
    }
    cli();
    LCD_44780_goto(9,1);
    switch(current.state)
    {
    case INPUT_STATE_TRACK:
            LCD_44780_write_string("Track  ");
            break;
    case INPUT_STATE_TRACK_PAUSE:
            LCD_44780_write_string("Pause  ");
            break;
    case INPUT_STATE_F1:
            LCD_44780_write_string("F1     ");
            break;
    case INPUT_STATE_F2:
            LCD_44780_write_string("F2     ");
            break;
    case INPUT_STATE_TIME:
            LCD_44780_write_string("TIME   ");
            break;
    case INPUT_STATE_SWEEP:
            LCD_44780_write_string("SWEEP  ");
            break;
    case INPUT_STATE_STORE:
            LCD_44780_write_string("STORE  ");
            break;
    case INPUT_STATE_RECALL:
            LCD_44780_write_string("RECALL ");
            break;
    default:
            LCD_44780_write_string("ERROR ");
            break;
    }

    // Show keypad string lower left
    LCD_44780_goto(0,1);
    LCD_44780_write_string(keypad_string);
    sei();
  }
}

                

