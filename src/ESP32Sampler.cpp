//=======================================================================
/** @file ESP32Sampler.cpp
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

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "AudioTools.h"
#include "ESP32Sampler.h"

// singleton, so instantiate here
ESPSampler &Sampler = Sampler.getInstance();

//
ESPSampler & ESPSampler::getInstance() {
  static ESPSampler instance;
  return instance;
}

// return a default config
SamplerConfig & ESPSampler::defaultConfig(){

  static SamplerConfig defcfg = {
    .pin              = SMP_DEFAULTPIN,
    .vmax             = SMP_DEFAULT_VMAX,
    .samplefrequency  = SMP_DEFAULT_FREQUENCY,
    .numsamples       = SMP_DEFAULT_SAMPLES,
    .mode             = SMP_DEFAULT_MODE,
    .multisample      = 2,
    .extrabuffers     = 0
  };
  return defcfg;      

}

// set configuration parameters, simply copy
void ESPSampler::setConfig(const SamplerConfig & newConfig)
{
  Config = newConfig;
}

// set configuration parameters, simply copy
SamplerConfig & ESPSampler::getConfig()
{
  static SamplerConfig rconfig = Config;
  return rconfig;
}


// Prepare for taking samples. Get data from our config.
// init the ADC, set Timer, set Buffer
//  we multi sample to smooth, so this reduces the actual frequency
// returns the time it shoud take to collect our samples.
void ESPSampler::Begin() {

  //  hardware / dma  frequency depends on multisampling.
  size_t sfreq = Config.samplefrequency * Config.multisample;
  time_t dmasz = Config.numsamples * Config.multisample * 2; // 2 bytes per sample
  _bufsz = dmasz < SMP_MAXBUFFERSIZE ? dmasz : SMP_MAXBUFFERSIZE;
  
  //  msec needed for filing buffers. 
  CollectTime = 1000 * Config.numsamples / Config.samplefrequency;

  // prep ADC
  adc1_channel_t channel = adc1_setup(Config.pin);

  AnalogConfig AConfig = _adc.defaultConfig(RX_MODE);
  AConfig.channels = 1;
  AConfig.buffer_size = _bufsz ; // max
  AConfig.buffer_count = 1 + (dmasz / _bufsz);
  AConfig.sample_rate = sfreq;
  AConfig.use_apll = 0;  // required to get right sample rate. Somehow.
  AConfig.setInputPin1(Config.pin);
  
  log_i("[ADC1] Sampler on ADC channel %d, %.3f Khz, %d samples = %.1f mSec", 
        channel,float(Config.samplefrequency)/1000, Config.numsamples, (float)CollectTime);
  log_i("[ADC1] I2S: %d buffers of %d bytes, raw frequency %.3f Khz", 
        AConfig.buffer_count, _bufsz, (float)sfreq/1000 );
  // start
  _running = true;
  _adc.begin(AConfig);
}

void ESPSampler::End() { _adc.end();  _running = false; }

//
// read samples, until we are satisfied.
// translate # bytes,  adjust wih caibration
time_t ESPSampler::Collect(sample_t *Buffer, size_t len) {

  size_t numread = 0,cnt = 0,c=0;
  sample_t *rawptr;
  uint32_t avg = 0,mavg = 0;

  time_t t0 = micros();  

  while (numread < len) {
    if (cnt == 0) {
      // collect a new buffer from DMA, always a full buffer
      cnt = _adc.readBytes(_samplebuffer, _bufsz);
      rawptr = (sample_t *)_samplebuffer;
    }
    mavg += esp_adc_cal_raw_to_voltage((uint32_t)(*rawptr++ & 0xFFF),&_adc_chars);
    cnt-= sizeof(sample_t);
    // multisample complete ? place in target buffer
    if ( ++c == Config.multisample) {
      mavg /= c; 
      Buffer[numread++] = (sample_t) mavg;
      avg += mavg;
      c=0; mavg = 0;
    }
  }
  // Return AC ? assume average is 0 level
  if (Config.mode == SMODE_AC) {
    AcZero = avg/numread;
    for (cnt=0;cnt<numread; cnt++) Buffer[cnt] -= AcZero;
  }
  // return how much time we needed
  return (micros()- t0)/1000; 
}

// read average for some time
//
sample_t ESPSampler::Measure(gpio_num_t pin, time_t duration) {
  
  uint32_t rawadc = 0; 
  if (_running) {
    log_e("Can't analuge measure ADC -I2S is running");
    return 0;
  }

  adc1_channel_t channel = adc1_setup(pin);
  int i=0;
  do {
    rawadc += esp_adc_cal_raw_to_voltage(adc1_get_raw(channel),&_adc_chars);
    i++;
    vTaskDelay(1);
    duration --;
  } while (duration > 0);
  
  return (sample_t) (rawadc / i);
}


// efuse  Ref mVolts to vrefpin
bool ESPSampler::routeVref(gpio_num_t vrefPin) {

  pinMode(vrefPin,OUTPUT);
  if (adc_vref_to_gpio(ADC_UNIT_2,vrefPin) == ESP_OK) {
      log_i("ADC2 Vref routed to GPIO %d", vrefPin);
      return true;
  } else {
      log_e("Failed to route ADC2 Vref to GPIO %d",vrefPin);
      return false;
  }
}

/**
setup the ADC to prepare

0dB attenuaton (ADC_ATTEN_0db) gives full-scale voltage approx 1.1V
2.5dB attenuation (ADC_ATTEN_2_5db) gives full-scale voltage 1.5V
6dB attenuation (ADC_ATTEN_6db) gives full-scale voltage 2.2V
11dB attenuation (ADC_ATTEN_11db) gives full-scale voltage 3.9V (see note below)
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
 
**/
adc1_channel_t ESPSampler::adc1_setup(gpio_num_t pin) {

  adc_atten_t adc_atten;
  adc1_channel_t channel;
  pinMode(pin,INPUT);
  check_efuse();
  adc1_config_width(ADC_WIDTH_12Bit);

  float db;
  if (Config.vmax <=1.1) {
    adc_atten = ADC_ATTEN_0db; 
    db=0;
  } else if (Config.vmax <=1.5) { 
    adc_atten = ADC_ATTEN_2_5db;  
    db=2.5;
  } else if (Config.vmax <=2.2)  { 
    adc_atten = ADC_ATTEN_6db ; 
    db=6;
  } else {
    adc_atten = ADC_ATTEN_11db; 
    db=11;
  }

switch (pin) {
    case GPIO_NUM_32:
      channel = ADC1_CHANNEL_4;
      break;
    case GPIO_NUM_33:
      channel = ADC1_CHANNEL_5;
      break;
    case GPIO_NUM_34:
      channel = ADC1_CHANNEL_6;
      break;
    case GPIO_NUM_35:
      channel = ADC1_CHANNEL_7;
      break;
    case GPIO_NUM_36:
      channel = ADC1_CHANNEL_0;
      break;
    case GPIO_NUM_37:
      channel = ADC1_CHANNEL_1;
      break;
    case GPIO_NUM_38:
      channel = ADC1_CHANNEL_2;
      break;
    case GPIO_NUM_39:
      channel = ADC1_CHANNEL_3;
      break;
  }

  // then setup our channel
  log_i("[ADC1] channel %d for %.1f V max with %.2f dB attenuation",channel,Config.vmax,db);
  adc1_config_channel_atten(channel, adc_atten);    // 
  
  // set /get data for calibration
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, adc_atten, ADC_WIDTH_12Bit, 1100, &_adc_chars);

  log_i("[ADC1] Characteristics: Vref = %d, CoeffA = %d, CoeffB = %d", _adc_chars.vref, _adc_chars.coeff_a, _adc_chars.coeff_b);

  // init
  (void) adc1_get_raw(channel);
  
  return channel;
}

//
// helpers
//
void ESPSampler::check_efuse(void)
{
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
     if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
     log_i("[ADC1] eFuse Two Point: Supported");
    } else {
     log_i("[ADC1] eFuse Vref: Supported");
    }
  } else {
    log_i("[ADC1] eFuse Vref: NOT supported, using default coefficients");
  }
}