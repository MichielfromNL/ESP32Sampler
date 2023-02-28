  ## ESP32 Sampler
  
  When I was setting up a Machine Learning sketch for sound processing, I ran across a pretty steep learning curve to get reliable sampling, 
  decibel measurement, signal analysis suchas FFT, MFCC and Shazam-style fingerprint creation. SO I decided: that can be made much more simple. 
  I found after lots of trial and error software,  in mostly C, but no always very suitable for simple embedded use.
  
  Setting up fast sound / signal sampling is, despite audiotools, still quite a bit of work. Because of the I2S, ADC and other parametrs and driver details.  How to properly init de ESP32 ADC, which ADC to use, how to use ESP32 calibration, how to find and set
  the right parameters.

  ESP32 Sampler is a  simple wrapper around Phil Schatzman's awsome audiostream. I adapted that for pure mono-signal input,
  and added esp32 ADC calibration to get accurate millivolts, which is needed for Decibel measurements.  
  I turned that into a class which properly inits the ADC , sets up the audiotools and collects buffers
  Besides that I believe that class-style interfaces bring a lot of benefit in hiding complexities.
  
  The whole idea of sampling is to get a bunch of 16-bits ADC values in pretty accurate millivolts, with no CPU overhead as possible. 
  The beauty of the ADC-I2S approach is, besides cheap and easy hardware, that the ADC and I2S subsystems are ESP32 hardware-native and use no CPU. The only CPU cycles ar for copying the data.
  
  ESP32Sampler is a singleton class, alread instantiated, ready to use.
  It works for quite high frequencies, 44100 is easy , doc says up to hundreds of Kb/sec 

  With sampling, an important thing to do is use RTOS, so that you can  put a sampler task on a separate core. 
  Believe me: RTOS is awsome, and very much needed in systems that process lots of data.
  This sketch tells you how to do that too. 
  
  S/w is the MIT License
  -------

I explain how this works with some code 

```c++
#include <Arduino.h>
#include "ESP32Sampler.h"
```
## Parameters
- VMAX needs to be set for the max input voltage of the microphone, which is determined by the hardware setup.
- The Sample frequency. Speaks for itself
- Num of samples to collect in each run. best is a ^2 number, 64, 128, .. 1024, 2048, all fine.  max is 8192 in one run.
- the Input pin, Supported are GPIO32 - 39  
- Multisample:  in noisy environments (e.g.  Wifi introduces spikes) multi-sampling takes the average of multiple readings. Remember though,that 8192 is the max. So if you want the average of 4x samples for 1024 samples, the sampler creates 4 buffers hat are always 1024, and sets the frequency to 4x your sample frequency.
 - Extra buffers:  suppose your task needs more time than the hardware need to collect data. If you don;t have extra buffers there may be overrum. Example:  at 8192 Hz sample frequency, a run of 1024 samples takes 125 msec. If your task needs less than that there is no problem becuase the collection takes no time (hardware, remember). But if onnce in a hile you need 200 msec, an extra buffer is needed. Times multisample. PLus, extra buffers take up memory, so only do tat when needed.  
 - Sample node can be SMODE_AC, or SMODE_DC . Because when you analyze sound, it is easier to rule out DC and have signals centered around 0. hence the int16_t 
 
Samples are *always* 16 bits . Although it is possible to have less bits my sampler does not support that out of the box
Sample data is signed with AC mode,  true AC +- millivolts are returned

```ç++
typedef sample_t int16_t;
```

Default values for the sampler are:

```c++
#define SMP_DEFAULT_VMAX         3.3           
#define SMP_DEFAULT_FREQUENCY    44100
#define SMP_DEFAULT_SAMPLES      1024
#define SMP_DEFAULTPIN           GPIO_NUM_34  // ADC1 channel 6
#define SMP_DEFAULT_MULTISAMPLE  2
#define SMP_DEFAULT_EXTRABUFFERS 0
#define SMP_DEFAULT_MODE         SMODE_DC
```c++

// connect analogue breakout e.g MAX 4466 to pin 34
const gpio_num_t micPin = GPIO_NUM_34;

// Optional, and for fun: connect pin 39 to 25
const gpio_num_t calibratePin = GPIO_NUM_39; 
const gpio_num_t vrefPin = GPIO_NUM_25;   // anaLog out for VREF routing
```

## how to init?
```c++
  // Get the default config and set parameters
  SamplerConfig Config = Sampler.defaultConfig();
  // 8192 Hz at GPIO34, 1024 samples, AC mode, reduce noise  
  Config.pin = micPin;
  Config.vmax = VCC;
  Config.samplefrequency = 8192;
  Config.numsamples = 1024;
  Config.mode = SMODE_AC;
  Config.multisample = 4;

  // store the config in the sampler
  Sampler.setConfig(Config);
```
## To start / stop the sampler
```c++
  Sampler.Begin();
  Sampler.End();
```
## to collect a run of samples:

The collect call is blocking, it waits for the hardware to provide the number of samples requested

```c++
  Sampler.Collect(Samples, 1024);
```

## meausure

A cute little feature is Sampler.Measure(pin,msecs) . Can be used for setup tasks. 
It measures during a number of msecs and retusn the average in accurate mvolts. An better alternative for AnalogueRead
It can not be used when the sampler is running, 
The example sketch outlines a use case

## RTOS

It is strongly recommended to use a dedicated RTOS task for sampling and signal analysis. Other task can then do Wifi and web things
the Example sketch has that setup.

Have fun!
