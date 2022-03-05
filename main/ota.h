/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef __OTA_UPDATE_H__
#define __OTA_UPDATE_H__
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
//#include "protocol_examples_common.h"
#include "errno.h"
#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
#define OTA_URL_SIZE 256
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
void http_cleanup(esp_http_client_handle_t client);
void task_fatal_error(void);
void print_sha256(const uint8_t *image_hash, const char *label);
void infinite_loop(void);
void ota_example_task(void *pvParameter);
bool diagnostic(void);
enum state_t
{
   TASK_ADDED,
   TASK_READY,
   TASK_RUNNING,
   TASK_WAITING,
   TASK_TERMINATED
};
#endif