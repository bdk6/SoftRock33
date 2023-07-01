//////////////////////////////////////////////////////////////////////////////
///
///  \file ad9833.c
///
///  \copy copyright (c) 2023 William R Cooke
///
///  @brief Control Analog Devices AD9833 DDS chip.
///
//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <avr/io.h>
#include "ad9833.h"
#include "softspi.h"
  
#ifdef __cplusplus
extern "C" {
#endif

// Control Reg
// D13   12   11  10   9    8   7    6   5    4   3    2   1    0
// [B28][HLB][Fs][Ps][rsv][RES][S1][S12][OP][rsv][D2][rsv][MD][rsv]
// B28: 1 allows full 28 bit freq writes, 0 half writes
// HLB: 1 allows MSB writes of freq, 0 allows LSB writes of freq
// Fs:  Freq select, selects which freq register to use, 0 or 1
// Ps:  Phase select, selects which phase register to use, 0 or 1
// RES: RESET 1 resets operation, 0 runs.  Does not clear phase acc or regs
// S1: Sleep 1: 1 disable Master clock, leaving output steady
// S12: Sleep 12: 1 powers down DAC
// OP:  OPBITEN 0 selects DAC output sin or ramp, 1 selects  SQUARE wave
// D2:  DIV2 If sqr wave selected, chooses either MSB (1) or MSB/2 (0)
// MD:  When OPBITEN = 0, 1 = ramp, 0 = sin.  Set to 0 if OPBITEN = 1

#define CNTL_B28      (1 << 13)
#define CNTL_HLB      (1 << 12)
#define CNTL_FS       (1 << 11)
#define CNTL_PS       (1 << 10)
#define CNTL_RESET    (1 << 8)
#define CNTL_SLEEP1   (1 << 7)
#define CNTL_SLEEP12  (1 << 6)
#define CNTL_OPBITEN  (1 << 5)
#define CNTL_DIV2     (1 << 3)
#define CNTL_MODE     (1 << 1)
        

static uint16_t controlReg;
        static uint32_t frequency_0;
        static uint32_t frequency_1;
        static uint16_t phase_0;
        static uint16_t phase_1;

//////////////////////////////////////////////////////////////////////////////
/// @function AD9833_init
/// @brief   Initializes AD9833 
/// @return
/////////////////////////////////////////////////////////////////////////////
int AD9833_init()
{
        controlReg = CNTL_B28 | CNTL_RESET;
        AD9833_write_word(controlReg);
        
  


////////////////////////////////////////////////////////////////////////////
/// @fn AD9833_write_word
/// @brief Write a raw 16 bit word to the chip.
/// @param[in] wd  The word to write.
////////////////////////////////////////////////////////////////////////////
void AD9833_write_word(uint16_t wd)
{
        // TODO
}


#ifdef __cplusplus
}
#endif  // __cplusplus

