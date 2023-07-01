//////////////////////////////////////////////////////////////////////////////
///
///  \file ad9833.h
///
///  \copy copyright (c) 2023 William R Cooke
///
///  @brief Control Analog Devices AD9833 DDS chip.
///
//////////////////////////////////////////////////////////////////////////////

#ifndef AD9833_H
#define AD9833_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
        
#include "device_config.h"
#include "gpio.h"
#include "softspi.h"

#define SOFTSPI_VERSION_MAJOR     0
#define SOFTSPI_VERSION_MINOR     1
#define SOFTSPI_VERSION_BUILD     0
#define SOFTSPI_VERSION_DATE      (20230623L)


        typedef enum AD9833_WaveMode
        {
                AD9833_SIN_MODE      = 0,
                AD9833_RAMP_MODE     = 1,
                AD9833_SQR_FULL      = 2,
                AD9833_SQR_HALF      = 3
        } AD9833_WaveMode_t;

        typedef enum AD9833_SleepMode
        {
                AD9833_SLEEP_STOP    = 0,
                AD9833_SLEEP_DAC_OFF = 1,
                AD9833_SLEEP_STOP_DAC_OFF  = 2,
                AD9833_SLEEP_,
                TODO
        } AD9833_SleepMode_t;
        

//////////////////////////////////////////////////////////////////////////////
/// @function AD9833_init
/// @brief   Initializes AD9833 
/// @return
/////////////////////////////////////////////////////////////////////////////
        int AD9833_init();

  ////////////////////////////////////////////////////////////////////////////
  /// @fn AD9833_write_word
  /// @brief Write a raw 16 bit word to the chip.
  /// @param[in] wd  The word to write.
  ////////////////////////////////////////////////////////////////////////////
 void AD9833_write_word(uint16_t wd);
    
        void AD9833_set_frequency(int which, uint32_t freq);

        void AD9833_set_phase(int which, uint32_t phase);

        void AD9833_select_freq(int which);

        void AD9833_select_phase(int which);

        void AD9833_reset(int res);

        void AD9833_sleep(int sleepMode);

        void AD9833_set_wave_mode(AD9833_WaveMode_t md);

        void AD9833_run_mode(AD9833_SleepMode_t md);

        

        void AD9833_update(void);

        
#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // #ifndef AD9833_H
