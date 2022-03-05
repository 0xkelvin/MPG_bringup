#ifndef __STORAGE_H__
#define __STORAGE_H__
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define MAX_SIZE 1000
#define LIMIT_NUMNER_FILE 500
int  storage_init();
int storage_read(char *data, size_t *length, char *path);
esp_err_t storage_write(const char *data, const char *type);
int storage_erase();
#endif