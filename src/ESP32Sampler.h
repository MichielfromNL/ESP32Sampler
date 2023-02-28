//=======================================================================
/** @file ESP32Sampler.h
 *  @brief Singleton class for analogue samples from ESP 32's ADC & I2S
 *  @author Michiel Steltman
 *  @copyright Copyright (C) 2011  Minchiel Steltman
 *
 * Implements a singleton class for getting analogue samples fomr an ESP32
 * Uses the fantastic audiotools olibrary from Phil SChatzman
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//=======================================================================
#include "AudioTools.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>

#pragma once

// make sure to use 16 bits
typedef int16_t  sample_t;

#define SMP_DEFAULT_VMAX         3.3           
#define SMP_DEFAULT_FREQUENCY    44100
#define SMP_DEFAULT_SAMPLES      1024
#define SMP_MAXBUFFERSIZE        1024
#define SMP_DEFAULTPIN           GPIO_NUM_34  // ADC1 channel 6
#define SMP_DEFAULTCHANNEL       ADC1_CHANNEL_6

// mode determines if we get real voltage, or AC voltage (0 v is average)
// 
enum SamplerMode { SMODE_DC, SMODE_AC };
#define SMP_DEFAULT_MODE         SMODE_DC

// Config struct, used for custom configs.
// to some extent
//
struct SamplerConfig {
  gpio_num_t  pin;
  float       vmax;
  size_t      samplefrequency;
  size_t      numsamples;
  SamplerMode mode;
  size_t      multisample;
  size_t      extrabuffers;      
};

class ESPSampler  {

private:
    ESPSampler() = default;

public:
  static ESPSampler &getInstance(); // Accessor for singleton instance
  ESPSampler(const ESPSampler &)            = delete; // no copying
  ESPSampler &operator=(const ESPSampler &) = delete;

public:
  SamplerConfig & defaultConfig();
  SamplerConfig & getConfig();
  void      setConfig(const SamplerConfig & Config);

  void      Begin();
  void      End();
  time_t    Collect(sample_t *Buffer, size_t len);
  sample_t  Measure(gpio_num_t pin,time_t duration);
  bool      routeVref(gpio_num_t pin);

  sample_t  AcZero = 2048;
  time_t    CollectTime;
  
private:
  
  // helpers
  adc1_channel_t  adc1_setup(gpio_num_t pin);
  void            check_efuse(void);
  
  uint8_t         _samplebuffer[DEFAULT_BUFFER_SIZE];
  size_t          _bufsz; // calculated
  bool            _running = false;

  // configuration parameters 
  // Set defaults also here, so it will always work out of the box 
  SamplerConfig Config = {
    .pin              = SMP_DEFAULTPIN,
    .vmax             = SMP_DEFAULT_VMAX,
    .samplefrequency  = SMP_DEFAULT_FREQUENCY,
    .numsamples       = SMP_DEFAULT_SAMPLES,
    .mode             = SMP_DEFAULT_MODE,
    .multisample      = 2,
    .extrabuffers     = 0
  };
  // i2s and ADC structs
  AnalogAudioStream _adc;
  esp_adc_cal_characteristics_t _adc_chars;
};

extern ESPSampler &Sampler;
