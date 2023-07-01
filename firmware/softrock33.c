//////////////////////////////////////////////////////////////////////////////
///  @file softrock33.c
///  @copy William R Cooke 2023
///  @brief  Controls SoftRock33 signal generator.
//////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#define F_CPU 16000000
#include <util/delay.h>


#include "avrlib/gpio.h"
#include "avrlib/softspi.h"

#define MASTER_CLOCK     25000000L
#define COUNTER_LENGTH   
static void DDS_init(void);
static void DDS_write_word(uint16_t wd);
static void DDS_write_frequency(uint32_t hz);
static void DDS_write_phase( uint16_t deg );

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
        for(int i = 1; i < 1000; i++);
        //SOFTSPI_init(1,2,3);
        DDS_init();

        return 0;
}


void DDS_init(void)
{
        SOFTSPI_init(GPIO_PIN_B5, GPIO_PIN_B3, -1);
        // ss on pb2
        SOFTSPI_set_interface(0, GPIO_PIN_C5, 16, SPI_MODE_2_MSB_FIRST, 0);
        DDRB |= 0x08;  // b3 output
        
        DDS_write_word(0x2100);  // B28 and RESET
        DDS_write_word(0x2100);
        _delay_us(5);
        DDS_write_frequency(6250000L);  // 10 Mhz
        DDS_write_phase(0);
        DDS_write_word(0x2000);  // B2b, out of reset
        _delay_us(50);
        uint32_t freq = 10737418L; //1073742L;  // Set freq to 100 KHz
        //DDS_write_word( (uint16_t)(freq & 0x3fff) | 0x4000);
        //DDS_write_word( (uint16_t)((freq >> 14) & 0x3fff) | 0x4000);

        while(1);

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


