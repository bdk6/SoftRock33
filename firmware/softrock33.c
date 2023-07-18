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


int main(int argc, char** argv)
{
        GPIO_pin_mode(GPIO_PIN_D2, GPIO_PIN_MODE_INPUT);
        
        for(int i = 1; i < 1000; i++);
        //SOFTSPI_init(1,2,3);
        DDS_init();

        uint8_t enc = read_encoder();
        

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
        
        DDS_write_word(0x2100);  // B28 and RESET
        DDS_write_frequency(60000L);  // 60 KHz
        DDS_write_phase(0);
        DDS_write_word(0x2000);  // Keep B2B set, take out of reset
        _delay_ms(2000);
        LCD_44780_clear();
        LCD_44780_write_string("60000 Hz 2");
        sei();  // turn on interrupts

        int32_t cnt = 0;
        while(1)
        {
                _delay_ms(1000);
                   int_to_string(cnt);
                 LCD_44780_clear();
                LCD_44780_write_string(intstr);
                cnt++;
                int32_t e = ENCODER_get_count(0);
                int_to_string(e);
                //int_to_string(SYSTICK_get_milliseconds());
                //LCD_44780_clear();
                LCD_44780_goto(0,1);
                // LCD_44780_write_string(intstr);
                int buttons = BUTTON_waiting();
                //LCD_44780_clear();
                int_to_string(buttons);
                LCD_44780_write_string(intstr);
                
                if(buttons )
                {
                        int b = BUTTON_get_button();
                        // set freq
                        //LCD_44780_clear();
                        LCD_44780_write_string("PRESS:");
                        int_to_string(b * 10000);
                        LCD_44780_write_string(intstr);
                }
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


uint8_t read_encoder(void)
{
        uint8_t rtn = 0;
        // pd2 and pd3

        rtn = GPIO_read_pin(GPIO_PIN_D2) << 1;
        rtn |= GPIO_read_pin(GPIO_PIN_D3);
        return rtn;
}

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

        
                

