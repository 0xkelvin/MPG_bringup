#ifndef __ADC_H__
#define __ADC_H__
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <math.h>
#define DEFAULT_VREF 800 // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 64 // Multisampling
#define POWER_SENSOR_ON_OFF 26
#define GPIO_OUTPUT_PIN_SELECT (1ULL << POWER_SENSOR_ON_OFF)
void powSensor(bool pow);
void adcSetup();
int getBattery();
float getTemperature();
float readVoltage(void);
#endif