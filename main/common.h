#ifndef __COMMON_H__
#define __COMMON_H__
#include <stdio.h>
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#define GPIO_LED_RED 15
#define GPIO_LED_BLU 13
char *chipInfo(void);
esp_err_t get_value_nsv(char *, uint8_t *out_value);
bool set_value_nsv(char *key, uint8_t value);
void led_light_on_off(uint8_t gpioNumber, bool value);
bool storage_pass_wifi(char *ssid, char *password);
char *read_pass_wifi_by_ssid(char *ssid);
void buzzeron(void *arg);
void light_red_blu_mix(void *milliseconds);
#endif