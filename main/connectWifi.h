#ifndef __CONNECT_WIFI_H__
#define __CONNECT_WIFI_H__
/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#define DEFAULT_SCAN_LIST_SIZE 10
esp_err_t wifi_init_sta(char *sta_hostname);
char *wifiInfo(void);
void wifiScan(void);
void set_wifi_ssid(char *ssdi);
void set_wifi_password(char *pass);
bool connect_wifi();
#endif