/*
  ESP32 Sampler
  
  I wrote this easy-to-use ( I believe :-) wrapper around Phip Schatzman's awsome audiotools, for 
  simple sound-analysis for collecting sound samples and Machine-learning.
  
  reason is that setting up fast sound / signal sampling is still quite a bit of work, because of the I2S, ADC and other driver details 
  How to properly init de ESP32 ADC, which ADC to use, how to use ESP32 calibration, how to find and set
  the right parameters.

  The whole idea of sampling is to get a bunch of 16-bits ADC values in pretty accurate millivolts, with no CPU overhead as possible
  The beauty of this approach is that the ADC and I2S subsystems are ESP32 hardware and take no CPU. The only CPU cycles ar for copying the data 
  
  So I wrapped this all into a singleton class.
  It works for quite high frequencies, 44100 is easy , doc says up to hundreds of Kb/sec 

  The other important thing to do is use RTOS, so that you can  put a sampler task on a separate core.
  This sketch tells you how to do that

  "Classified" for ESP32 and Arduino by M. Steltman May 2023
  
  License
  -------
  Copyright 2023 Michiel Steltman

  The MIT license

*/
#include <Arduino.h>
#include "ESP32Sampler.h"

#define NUMSAMPLES 1024
#define SAMPLEFREQ 44100
#define COLLECTMS  ( 1000 * NUMSAMPLES / SAMPLEFREQ) // 
#define MULTISAMPLE 2
#define VCC 3.3
#define MSEC(x) (x * portTICK_PERIOD_MS)
#define COLLECTOR_TASK_PRIORITY   (tskIDLE_PRIORITY + 4)

// Samples MUST be 16 bits. 
// Although it is possible to have less bits my sampler does not support that out of the box
// Sample data is signed, with AC mode,  true AC +- millivolts are returned
typedef sample_t int16_t;

// connect analogue breakout e.g MAX 4466 to pin 34
const gpio_num_t micPin = GPIO_NUM_34;

// Optional, and for fun: connect pin 39 to 25
//  
const gpio_num_t calibratePin = GPIO_NUM_39;      // 
// Output
const gpio_num_t vrefPin = GPIO_NUM_25;   // anaLog out for VREF routing

TaskHandle_t CollectorTaskHandle;
void SoundCollector(void *pvParameters);

// The raw data that we collect
sample_t Samples[NUMSAMPLES];

void setup() {

  Serial.begin(115200);

  // is not necessary, but illustrates the use of some support functions
  // and shows a bit of how the calibration works
  // for enabling log_x messages: set compiler flags to -DCORE_DEBUG_LEVEL=4 to see what happens internally
  //  
  sample_t vLow, vRef;

  pinMode(vrefPin, OUTPUT);
  digitalWrite(vrefPin, LOW);
  vLow = Sampler.Measure(calibratePin, 512);
  Sampler.routeVref(vrefPin);
  vRef = Sampler.Measure(calibratePin, 512);
  Serial.printf("Measured Low %d, Vref %d mV", vLow, vRef);

    // Initialize our sampler
  SamplerConfig Config = Sampler.defaultConfig();
  Config.pin = micPin;
  // Max input voltage. Wich sets the ADC parameters
  Config.vmax = VCC;
  Config.samplefrequency = SAMPLEFREQ;
  Config.numsamples = NUMSAMPLES;
  // AC : voltage is integer between -,0,+
  Config.mode = SMODE_AC;
  Config.multisample = MULTISAMPLE;

  Sampler.setConfig(Config);

  xTaskCreatePinnedToCore(SoundCollector, "Collector", 4096, NULL, COLLECTOR_TASK_PRIORITY, &CollectorTaskHandle, 0);
}

void SoundCollector(void *pvParameters) {  
  unsigned  loopcounter = 0;
  time_t    t0;

  Serial.println("Collector task");
  Sampler.Begin();

  // task loop
  while (true)
  {
    // call is blocking, waits for datacollection cycle to complete.
    t0 = Sampler.Collect(Samples, NUMSAMPLES);
    if (++loopcounter % 100 == 0) {
      Serial.printf("Collected %d samples in %d msec, %d msecs overhead\n", NUMSAMPLES, COLLECTMS,millis()-t0);
      Serial.printf("AcZero level = %d mvolts\n",Sampler.AcZero);
      Serial.printf("%d,%d,%d,%d ... %d\n", Samples[0],Samples[1],Samples[2],Samples[3],Samples[NUMSAMPLES-1]);
    }
  }
}

// do other things, such as receiving data from a queue 
// provided by the sampler task . See my other sketches for examples
// 
void loop() {
  vTaskDelay(10);
}
