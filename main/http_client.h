#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_http_client.h"

#define WEB_SERVER "https://api.mypetgo.com"
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
void http_put_version(char *mac_put, const char *vertion);
char *http_get_version(char *mac_address);
#endif