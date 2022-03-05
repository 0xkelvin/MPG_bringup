#ifndef __INIT_H__
#define __INIT_H__

#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_adc_cal.h"
#include "esp_log.h"

#include "define.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

void check_efuse(void);
void initADC(void);
void initFlash(void);
void initIO(void);

#endif