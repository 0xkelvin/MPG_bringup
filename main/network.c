#include <time.h>
#include "esp_log.h"
#include <network.h>
#include <common.h>
#include "define.h"
#include "sensor.h"
static bool gpsFlag_ = false;
static uint8_t mac_[18];

static char cmdPublisher_[2 * BUF_SIZE_512] = {0};
static char utc_time[BUF_SIZE_32] = {0};

static QueueHandle_t uart0_queue;
static const char *TAG_NET = "NB_IOT info";
static const char *TAG_GPS = "GPS info";

static enum NetworkStateInit networkStateInit_ = NB_START;
static enum NetworkStateConnect networkStateCon_ = START;

static struct NetworkInfo networkInfo_ = {
	.uartState = DISCONNECTED,
	.provider = {0},
	.signalStrength = NO_SIGNAL,
	.signalReceived = NO_SIGNAL,
	.registrationStatus = NOT_REGISTERED,
	.APN = {0},
	.IPV4 = {0},
	.CCID = {0},
	.COPS = {0}};

static int timeBetweenFix_ = 3; // default 1
static uint8_t sim_status = 0;	// check sim status
static bool f_gps_ok = false;
bool f_gps_init = false;

bool f_init_ok = false;
bool f_init_reply = false;

static struct Gps gps_ = {
	.time = {0},
	.latitude = 180,
	.longitude = 180,
	.altitude = 0,
	.spkm = 0.0};

static enum SerialMode serialMode_ = NOT_USE;
static enum GPSMode gpsMode_ = GPS_NOTUSE;

static struct Mqtt mqtt_[2];
extern char *mac_adress;
size_t dataSize(const char *data)
{
	size_t length = 0;
	while (data[length++])
		;
	return length - 1;
}

static double parseLatLon(char *data)
{
	double raw = strtod(data, NULL);

	int data_int = raw / 100;
	double decimal = raw - data_int * 100;
	return (double)(data_int + decimal / 60);
}

static void parsedateTime(char *dateTime, char *data)
{
	dateTime[0] = data[0];
	dateTime[1] = data[1];
	dateTime[2] = ':';
	dateTime[3] = data[2];
	dateTime[4] = data[3];
	dateTime[5] = ':';
	dateTime[6] = data[4];
	dateTime[7] = data[5];
}
static void gpsParse(char data[13][13], char size)
{
	ESP_LOGI(TAG_NET, "---Parse GPS-----");
	char *token;
	token = strtok(data[1], ".");
	if (!token)
	{
		return;
	}
	char time[8];
	parsedateTime(time, token);
	if (!memcmp(data[4], "N", 2))
	{
		gps_.latitude = parseLatLon(data[3]);
	}
	else if (!memcmp(data[4], "S", 2))
	{
		gps_.latitude = -parseLatLon(data[3]);
	}
	else
	{
		return;
	}

	if (!memcmp(data[6], "E", 2))
	{
		gps_.longitude = parseLatLon(data[5]);
	}
	else if (!memcmp(data[4], "W", 2))
	{
		gps_.longitude = -parseLatLon(data[5]);
	}
	else
	{
		return;
	}
	ESP_LOGI(TAG_GPS, "lat :%f long: %f", gps_.latitude, gps_.longitude);
	char date[8] = {0};
	for (int pos = 6; pos < size; pos++)
	{
		if (dataSize(data[pos]) == 6)
		{
			parsedateTime(date, data[pos]);
		}
	}

	if (!dataSize(date))
	{
		return;
	}
	memcpy(gps_.time, date, 8);
	gps_.time[8] = ' ';
	memcpy(gps_.time + 9, time, 8);
	gpsFlag_ = true;
	ESP_LOGI(TAG_GPS, "Datetime GPS %s", gps_.time);
	return;
}

static void parseSignal(char *data)
{
	char *ptr;
	uint8_t rssi = strtol((data + sizeof("+CSQ:")), &ptr, 10);
	if (rssi == 99)
	{
		networkInfo_.signalStrength = NO_SIGNAL;
		ESP_LOGI(TAG_NET, "rssi: no signal");
	}
	else if (rssi < 10)
	{
		networkInfo_.signalStrength = WEAK_SIGNAL;
		ESP_LOGI(TAG_NET, "rssi: weak signal");
	}
	else if (rssi < 20)
	{
		networkInfo_.signalStrength = MEDIUM_SIGNAL;
		ESP_LOGI(TAG_NET, "rssi: medium signal");
	}
	else
	{
		networkInfo_.signalStrength = STRONG_SIGNAL;
		ESP_LOGI(TAG_NET, "rssi: strong signal");
	}
	// ESP_LOGI(TAG_NET, "ptr: %s", ptr);
	uint8_t rsrq = strtol(ptr + 1, NULL, 10);
	if (rsrq == 99)
	{
		networkInfo_.signalReceived = NO_SIGNAL;
		ESP_LOGI(TAG_NET, "rsrq: no signal");
	}
	else if (rsrq < 3)
	{
		networkInfo_.signalReceived = WEAK_SIGNAL;
		ESP_LOGI(TAG_NET, "rsrq: weak signal");
	}
	else if (rsrq < 6)
	{
		networkInfo_.signalReceived = MEDIUM_SIGNAL;
		ESP_LOGI(TAG_NET, "rsrq: medium signal");
	}
	else
	{
		networkInfo_.signalReceived = STRONG_SIGNAL;
		ESP_LOGI(TAG_NET, "rsrq: strong signal");
	}
	ESP_LOGI(TAG_NET, "rssi: %d (min/max 2:30); rsrq: %d (min/max 0:7)", rssi, rsrq);
}

static int dataParse(const uint8_t *data, uint32_t size)
{
	char dt[size];
	memcpy(dt, data, size);
	char *tmp_str;
	if (strstr(dt, "OK"))
	{
		networkInfo_.uartState = CONNECTED;
		if (f_gps_init)
		{
			ESP_LOGI(TAG_GPS, "GPS Command Reply OK");
			f_gps_ok = true;
		}
		else if (f_init_reply)
		{
			ESP_LOGI(TAG_GPS, "Init Command Reply OK");
			f_init_ok = true;
		}
	}

	if (strstr(dt, "ERR"))
	{
		ESP_LOGE(TAG_NET, "%s", dt);
		return -1;
	}

	if (strstr(dt, "ERROR"))
	{
		ESP_LOGE(TAG_NET, "%s", dt);
		return -1;
	}

	// Check SIM status #QSS: <status>
	tmp_str = NULL;
	tmp_str = strstr(dt, "#QSS: ");
	if (tmp_str)
	{

		tmp_str = strtok(dt, ",");
		if (!tmp_str)
		{
			return -1;
		}

		tmp_str = strtok(NULL, "\r");
		if (!tmp_str)
		{
			return -1;
		}
		sim_status = strtol(tmp_str, NULL, 10);
		ESP_LOGI(TAG_NET, "SIM status: %d", sim_status);
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "#CCID:");
	if (tmp_str)
	{
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r");
		if (!tmp_str)
		{
			return -1;
		}
		ESP_LOGI(TAG_NET, "#CCID: %s", tmp_str);
		memcpy(networkInfo_.CCID, tmp_str, dataSize(tmp_str));
		return 0;
	}
	//#HTTPRING: 0,200,"application/json",312
	tmp_str = NULL;
	tmp_str = strstr(dt, "#HTTPRING: ");
	if (tmp_str)
	{
		tmp_str = strtok(dt, "200");
		if (!tmp_str)
		{
			ESP_LOGI(TAG_NET, "request to HTTP server succeed.");
			send_command("AT#HTTPRCV=0");
			return 0;
		}
		return -1;
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "<<<");
	if (tmp_str)
	{
		ESP_LOGE(TAG_NET, "htpp data: %s", dt);
		return 0;
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "#SPN: ");
	if (tmp_str)
	{
		memset(networkInfo_.provider, 0, BUF_SIZE_32);
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			// strcpy(networkInfo_.provider, "SPN is empty");
			return -1;
		}
		tmp_str = strtok(NULL, "\r");
		if (!tmp_str)
		{
			// strcpy(networkInfo_.provider, "SPN is empty");
			return -1;
		}
		memcpy(networkInfo_.provider, tmp_str, dataSize(tmp_str));
		ESP_LOGI(TAG_NET, "provider %s", networkInfo_.provider);
		return 0;
	}

	tmp_str = NULL;
	tmp_str = strstr(dt, "+CSQ: ");
	if (tmp_str)
	{
		parseSignal(tmp_str);
		return 0;
	}

	tmp_str = NULL;
	tmp_str = strstr(dt, "+CREG: ");
	if (tmp_str)
	{
		ESP_LOGW(TAG_NET, "+CREG response :(%s)", dt);
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r\n");
		if (!tmp_str)
		{
			return -1;
		}
		if (strstr(tmp_str, "1,1"))
		{
			ESP_LOGI(TAG_NET, "(1,1)registered, home network");
			networkInfo_.registrationState = CONNECTED;
			networkInfo_.registrationStatus = REGISTERED;
			networkStateCon_ = NB_CREG;
		}
		else if (strstr(tmp_str, "1,5"))
		{
			ESP_LOGI(TAG_NET, "(1,5)registered, roaming");
			networkInfo_.registrationState = CONNECTED;
			networkInfo_.registrationStatus = ROAMING;
			networkStateCon_ = NB_CREG;
		}
		else if (strstr(tmp_str, "0,1"))
		{
			ESP_LOGI(TAG_NET, "Disable the network registration");
			networkInfo_.registrationState = DISCONNECTED;
			networkInfo_.registrationStatus = NOT_REGISTERED;
		}
		else
			ESP_LOGW(TAG_NET, "Network registration status:(%s) not registered", tmp_str);
		return 0;
	}

	tmp_str = NULL;
	tmp_str = strstr(dt, "+CEREG: ");
	if (tmp_str)
	{
		ESP_LOGW(TAG_NET, "+CEREG response :(%s)", dt);
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r\n");
		if (!tmp_str)
		{
			return -1;
		}
		if (strstr(tmp_str, "1,1"))
		{
			ESP_LOGI(TAG_NET, "(1,1)registered, home network");
			networkInfo_.registrationState = CONNECTED;
			networkInfo_.registrationStatus = REGISTERED;
			networkStateCon_ = NB_CREG;
		}
		else if (strstr(tmp_str, "1,5"))
		{
			ESP_LOGI(TAG_NET, "(1,5)registered, roaming");
			networkInfo_.registrationState = CONNECTED;
			networkInfo_.registrationStatus = ROAMING;
			networkStateCon_ = NB_CREG;
		}
		else if (strstr(tmp_str, "0,1"))
		{
			ESP_LOGI(TAG_NET, "Disable the network registration");
			networkInfo_.registrationState = DISCONNECTED;
			networkInfo_.registrationStatus = NOT_REGISTERED;
		}
		else
			ESP_LOGW(TAG_NET, "Network registration status:(%s) not registered", tmp_str);
		return 0;
	}
	tmp_str = NULL;
	// tmp_str = strstr(dt, "+CGPADDR: ");
	tmp_str = strstr(dt, "+CGCONTRDP: ");
	if (tmp_str)
	{
		// +CGCONTRDP: 1,5,"v-internet","9.137.246.35",,"203.113.131.6","203.113.131.5"
		tmp_str = strtok(dt, ",");
		char arry_info[10][HASH_LEN] = {0};
		uint8_t pos = 0;
		while (tmp_str != NULL)
		{
			memcpy(arry_info[pos], tmp_str, dataSize(tmp_str));
			tmp_str = strtok(NULL, ",");
			// ESP_LOGI(TAG_NET, "arry_info[%d]=%s", pos, arry_info[pos]);
			pos++;
		}
		if (pos >= 5 &&
			arry_info[2] != NULL && arry_info[3] != NULL &&
			arry_info[2][0] != '\0' && arry_info[3][0] != '\0')
		{
			if (networkInfo_.IPV4[0] == '\0' && networkInfo_.APN[0] == '\0')
			{
				memcpy(networkInfo_.APN, arry_info[2] + 1, dataSize(arry_info[2]) - 2);
				memcpy(networkInfo_.IPV4, arry_info[3] + 1, dataSize(arry_info[3]) - 2);
				ESP_LOGI(TAG_NET, "IPV4:%s", networkInfo_.IPV4);
				ESP_LOGI(TAG_NET, "APN:%s", networkInfo_.APN);
			}
		}
		return 0;
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "DISCONNECT");
	if (tmp_str)
	{
		ESP_LOGI(TAG_NET, "mqtt disconnnected");
		return 0;
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "#MQCONN:");
	if (tmp_str)
	{
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			return -1;
		}

		tmp_str = strtok(NULL, "\r\n");
		if (!tmp_str)
		{
			return -1;
		}
		ESP_LOGI(TAG_NET, "Result connnect mqtt: %s", tmp_str);
		int channel = strtol(tmp_str, NULL, 10);
		if (channel > 2)
		{
			ESP_LOGI(TAG_NET, "channel %d", channel);
			return -1;
		}
		mqtt_[channel - 1].connectState = strtol(tmp_str + 2, NULL, 10);
		ESP_LOGI(TAG_NET, "Channel %d, Client status: %d", channel, mqtt_[channel - 1].connectState);
		if (mqtt_[channel - 1].connectState == 1)
		{
			mqtt_[channel - 1].mode = CONNECTED_MQTT;
		}
		else
		{
			ESP_LOGW(TAG_NET, "Wait until 1: client performed MQTT authentication with brokerd");
			mqtt_[channel - 1].mode = DISCONNECTED_MQTT;
		}
	}

	tmp_str = NULL;
	tmp_str = strstr(dt, "$GPSNMUN");
	if (tmp_str)
	{
		char *gps_nmun = strtok(dt, " ");
		if (!gps_nmun)
		{
			return -1;
		}
		gps_nmun = strtok(NULL, "\r\n");
		if (!gps_nmun)
		{
			return -1;
		}
		ESP_LOGI(TAG_GPS, "%s", gps_nmun);
		//$GPRMC,142741.00,A,1603.86575,N,10809.19865,E,0.0,,091221,1.1,W,A,V*71
		//$GNRMC,   140342.00,A,1603.87134,N,10809.21363,E,0.0,,091221,1.1,W,A,V*6C
		tmp_str = NULL;
		tmp_str = strstr(gps_nmun, "$GNRMC");
		if (tmp_str)
		{
			// test
			char *gps_data = strtok(gps_nmun, "$");
			if (!gps_data)
			{
				return -1;
			}
			gps_data = strtok(NULL, "\r\n");
			if (!gps_data)
			{
				return -1;
			}
			ESP_LOGI(TAG_GPS, "%s", gps_data);
			tmp_str = strtok(dt, ",");
			if (tmp_str == NULL)
			{
				return -1;
			}
			// test
			char gps_info[13][13] = {0};
			uint8_t pos = 0;
			while (tmp_str && pos < 13)
			{
				memcpy(gps_info[pos++], tmp_str, dataSize(tmp_str));
				tmp_str = strtok(NULL, ",");
			}
			if (pos < 7)
			{
				return -1;
			}
			gpsParse(gps_info, pos);
		}
		return true;
	}

	// $GPSACP: 060832.000,2212.4736N,11326.8168E,500.0,1264.4,2,0.0,,,060921,03
	// $GPSACP: ,,,,,1,,,,,
	// $GPSACP: ,,,,,0,,,,,
	tmp_str = NULL;
	tmp_str = strstr(dt, "$GPSACP: ");
	if (tmp_str)
	{
		// char str[] = "$GPSACP: ,,,,,0,,,,,\r\n";
		char *gps_data = strtok(dt, ":");
		gps_data = strtok(NULL, "\r\n");
		if (gps_data != NULL)
		{
			ESP_LOGI(TAG_GPS, "$GPSACP: %s", gps_data);
			tmp_str = strtok(gps_data, ",");
			char gps_info[13][13] = {0};
			uint8_t pos = 0;
			while (tmp_str != NULL)
			{
				memcpy(gps_info[pos], tmp_str, dataSize(tmp_str));
				tmp_str = strtok(NULL, ",");
				pos++;
			}
			if (pos >= 10)
			{
				// lat
				char *isCheck = strstr(gps_info[1], "N");
				if (isCheck != NULL)
				{
					gps_.latitude = parseLatLon(strtok(gps_info[1], "N"));
				}
				else
				{
					isCheck = strstr(gps_info[1], "S");
					if (isCheck != NULL)
					{
						gps_.latitude = -parseLatLon(strtok(gps_info[1], "S"));
					}
				}
				// long
				isCheck = strstr(gps_info[2], "E");
				if (isCheck != NULL)
				{
					gps_.longitude = parseLatLon(strtok(gps_info[2], "E"));
				}
				else
				{
					isCheck = strstr(gps_info[2], "W");
					if (isCheck != NULL)
					{
						gps_.longitude = -parseLatLon(strtok(gps_info[2], "W"));
					}
				}
				// altitude - mean-sea-level (geoid) inmeters
				ESP_LOGI(TAG_GPS, "altitude - mean-sea-level: %s", gps_info[4]);
				gps_.altitude = strtod(gps_info[4], NULL);

				// speed over ground (Km/hr)
				ESP_LOGI(TAG_GPS, "speed over ground (Km/hr): %s", gps_info[7]);
				gps_.spkm = strtod(gps_info[7], NULL);

				// time
				char *token;
				token = strtok(gps_info[0], ".");
				if (!token)
				{
					return -1;
				}
				char time[8];
				parsedateTime(time, token);
				// date
				char date[8] = {0};
				parsedateTime(date, gps_info[9]);
				if (dataSize(date))
				{
					memcpy(gps_.time, date, 8);
					gps_.time[8] = ' ';
					memcpy(gps_.time + 9, time, 8);
				}
				ESP_LOGI(TAG_GPS, "Lat :%f Long: %f", gps_.latitude, gps_.longitude);
				gpsFlag_ = true;
			}
		}
		return 0;
	}
	///
	tmp_str = NULL;
	tmp_str = strstr(dt, "$GPSCFG: ");
	if (tmp_str)
	{
		tmp_str = strtok(dt, "$");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r\n");
		if (!tmp_str)
		{
			return -1;
		}
		ESP_LOGI(TAG_GPS, "Configured GPS$%s", tmp_str);
		return 0;
	}
	////
	tmp_str = NULL;
	tmp_str = strstr(dt, "$GPSP:");
	if (tmp_str)
	{
		int power = strtol(tmp_str + sizeof("$GPSP:"), NULL, 10);
		ESP_LOGI(TAG_GPS, "GPS Power %d", power);
		if (power == 0)
		{
			gps_.state = GPS_STATE_STOPED;
		}
		else if (power == 1)
		{
			gps_.state = GPS_STATE_STARTED;
		}
		else
		{
			return -1;
		}
	}
	tmp_str = NULL;
	tmp_str = strstr(dt, "+CCLK: ");
	if (tmp_str)
	{
		tmp_str = strtok(dt, "\"");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\"");
		if (!tmp_str)
		{
			return -1;
		}
		// default datetime: +CCLK: "80/01/06,00:00:38+28" not good
		if (atoi(tmp_str) == 80)
		{
			ESP_LOGI(TAG_NET, "Invalid date: %s", tmp_str);
			return -1;
		}
		strcpy(utc_time, tmp_str);
		// ESP_LOGI(TAG_NET, "Time provider:%s", utc_time);
		serialMode_ = DATE_TIME_PROVIDER_END;
		return 0;
	}
	// Select Wireless Network
	tmp_str = NULL;
	tmp_str = strstr(dt, "+COPS: ");
	if (tmp_str)
	{
		ESP_LOGW(TAG_NET, "+COPS response=%s", dt);
		tmp_str = strtok(dt, " ");
		if (tmp_str == NULL)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r");
		if (tmp_str == NULL)
		{
			return -1;
		}
		ESP_LOGI(TAG_NET, "Perators:%s (0:GSM, 8:CAT M1 9:NB IoT)", tmp_str);
		if (dataSize(tmp_str) > 10)
			memcpy(networkInfo_.COPS, tmp_str, dataSize(tmp_str));
		return 0;
	}
	// LTE technology supported
	tmp_str = NULL;
	tmp_str = strstr(dt, "#QDNS: ");
	if (tmp_str)
	{
		tmp_str = strtok(dt, " ");
		if (!tmp_str)
		{
			return -1;
		}
		tmp_str = strtok(NULL, "\r\n");
		if (!tmp_str)
		{
			return -1;
		}
		ESP_LOGI(TAG_NET, "Query DNS %s", tmp_str);
		networkStateInit_ = NB_RESOLVE_DNS;
		return 0;
	}
	return -1;
}

static void networkStart(void)
{
	ESP_LOGI(TAG_NET, "Network start");
	static uint8_t timeout = 0;
	uint8_t cnt_timeout = 0;
	networkInfo_.registrationState = NOT_REGISTERED;
	while (networkInfo_.registrationState != CONNECTED && cnt_timeout < 5)
	{
		send_command(NETWORK_REGISTRATION_COMMAND);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		send_command(CHECK_REGISTRATION_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		++cnt_timeout;
	}
	if (networkInfo_.registrationState != REGISTERED)
	{
		serialMode_ = NOT_USE;
		return;
	}
	timeout = 0;
	memset(networkInfo_.IPV4, '\0', BUF_SIZE_32);
	memset(networkInfo_.APN, '\0', BUF_SIZE_32);
	while (networkInfo_.IPV4[0] == '\0' && networkInfo_.APN[0] == '\0')
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(150 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(CHECK_ADDRESS_COMMAND);
		timeout = 1;
	}
	if (serialMode_ != NOT_USE)
	{
		networkStateCon_++;
		send_command(CONFIG_PDP_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		send_command(ACTIVE_PDP_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		send_command(HTTP_CONFIG);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		char command[BUF_SIZE_128];
		sprintf(command, HTTP_QRY, mac_adress);
		send_command(command);
	}
	serialMode_ = CONNECTED_NETWORK;
}

static bool networkInit(void)
{
	// reset net work
	ESP_LOGI(TAG_NET, "Network init");
	networkInfo_.uartState = DISCONNECTED;
	memset(networkInfo_.provider, 0, sizeof(networkInfo_.provider));
	networkInfo_.signalStrength = NO_SIGNAL;
	networkInfo_.signalReceived = NO_SIGNAL;
	networkInfo_.registrationStatus = NOT_REGISTERED;
	memset(networkInfo_.APN, 0, sizeof(networkInfo_.APN));
	memset(networkInfo_.IPV4, 0, sizeof(networkInfo_.IPV4));
	networkStateInit_ = NB_START;
	int timeout = 0;
	while (networkInfo_.uartState == DISCONNECTED)
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(AT_COMMAND);
		timeout = 1;
	}
	// - Query SIM Status
	timeout = 0;
	sim_status = 0;
	uint16_t cnt_timeout = 0;
	while (sim_status == 0 && cnt_timeout < 5)
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(CHECK_SIM_STATUS);
		timeout = 1;
		++cnt_timeout;
	}
	if (sim_status == 0)
	{
		ESP_LOGI(TAG_NET, "SIM not inserted");
		return false;
	}
	else if (sim_status == 1)
		ESP_LOGI(TAG_NET, "SIM inserted");
	else if (sim_status == 2)
		ESP_LOGI(TAG_NET, "SIM inserted, and PIN unlocked");
	else if (sim_status == 3)
		ESP_LOGI(TAG_NET, "SIM inserted and READY");
	// Execution command reads on SIM the Integrated Circuit Card Identification (ICCID).
	cnt_timeout = 0;
	memset(networkInfo_.CCID, '\0', BUF_SIZE_32);

	send_command("AT#CCID");
	while (networkInfo_.CCID[0] == '\0' && cnt_timeout < COUNT_TIMEOUT)
	{
		//send_command("AT#CCID");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		++cnt_timeout;
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "IOT SIM CCID=%s", networkInfo_.CCID);

	// to check SPN
	cnt_timeout = 0;
	memset(networkInfo_.provider, '\0', BUF_SIZE_32);
	send_command(CHECK_PROVIDER_COMMAND);
	while (networkInfo_.provider[0] == '\0' && cnt_timeout < COUNT_TIMEOUT)
	{
		//send_command(CHECK_PROVIDER_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		++cnt_timeout;
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "IOT SIM provider=%s", networkInfo_.provider);

	// to check operators
	cnt_timeout = 0;

	//Set automatically register to network
	//send_command(NETWORK_REGISTRATION_COMMAND);
	//vTaskDelay(2000 / portTICK_PERIOD_MS);

	memset(networkInfo_.COPS, '\0', BUF_SIZE_64);
	send_command(CHECK_OPERATORS_COMMAND);
	while (networkInfo_.COPS[0] == '\0' && cnt_timeout < COUNT_TIMEOUT)
	{
		//send_command(CHECK_OPERATORS_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		++cnt_timeout;
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "IOT SIM COPS=%s", networkInfo_.COPS);

	// signal test
//	send_command(CHECK_SIGNAL_COMMAND);
//	cnt_timeout = 0;
//	do
//	{
//		//send_command(CHECK_SIGNAL_COMMAND);
//		vTaskDelay(2000 / portTICK_PERIOD_MS);
//		if (networkInfo_.signalStrength == NO_SIGNAL && networkInfo_.signalReceived == NO_SIGNAL)
//			++cnt_timeout;
//	} while ((networkInfo_.signalStrength == NO_SIGNAL ||
//			  networkInfo_.signalReceived == NO_SIGNAL) &&
//			 //serialMode_ != NOT_USE && cnt_timeout < 600);
//			serialMode_ != NOT_USE && cnt_timeout < 30);
//	//if(cnt_timeout >= 600)
//	if(cnt_timeout >= 30)
//	{
//		serialMode_ = NOT_USE;
//		return false;
//	}
//	ESP_LOGI(TAG_NET, "IOT SIM signalStrength=%d", networkInfo_.signalStrength);
//	ESP_LOGI(TAG_NET, "IOT SIM signalReceived=%d", networkInfo_.signalReceived);

	// Set time UTC
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	send_command(UTC_SET_TYPE);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	// networkStart();
	serialMode_ = INITIALIZED_NETWORK;
	ESP_LOGI(TAG_NET, "Network Initialized");
	return true;
}

static void networkStop()
{
	ESP_LOGI(TAG_NET, "Stop network");
	// static int timeout = 0;
	//  networkStateCon_ = CREG;
	//  send_command(CHECK_REGISTRATION_COMMAND);
	if (networkInfo_.registrationState != DISCONNECTED)
	{
		send_command(DISABLE_NETWORK_COMMAND);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		send_command(CHECK_REGISTRATION_COMMAND);
		// timeout = 1;
	}
	ESP_LOGI(TAG_NET, "Network Stoped");
	// timeout = 0;
}
static void gpsInit(void)
{
	int timeout = 0;
	ESP_LOGI(TAG_GPS, "GPS Init(1 time when initializing the device) %d", networkInfo_.uartState);
	f_gps_init = true;
	while (networkInfo_.uartState == DISCONNECTED)
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(AT_COMMAND);
		timeout = 1;
	}
	char command[BUF_SIZE_128];
	// it defines the Time Between Fix
	sprintf(command, GPS_CONFIG, 1, 3);
	send_command(command);
	f_gps_ok = false;
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(command);
		timeout = 0;
	}
	// 0: the constellation is selected automatically based on Mobile CountryCode (MCC) of camped network
	sprintf(command, GPS_CONFIG, 2, 3); // GPS+GLO
	send_command(command);
	f_gps_ok = false;
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(command);
		timeout = 0;
	}

	// Send cmd reboot
	f_gps_ok = false;
	send_command(REBOOT);
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(REBOOT);
		timeout = 0;
	}

	sprintf(command, GPS_CONFIG, 3, 0);
	f_gps_ok = false;
	send_command(command);
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(command);
		timeout = 0;
	}

	send_command(GPS_CONFIG_DATA_EX);
	f_gps_ok = false;
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(GPS_CONFIG_DATA_EX);
		timeout = 0;
	}

	send_command(GPS_CONFIG_DATA);
	f_gps_ok = false;
	timeout = 0;
	while (!f_gps_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(GPS_CONFIG_DATA);
		timeout = 0;
	}
	// Call 1 time when click button connect ble
	f_gps_init = false;
	ESP_LOGI(TAG_GPS, "---End Init GPS---");
}

static void resetData(void)
{
	ESP_LOGI(TAG_NET, "resetdata");
	networkInfo_.uartState = DISCONNECTED;
	// memset(networkInfo_.provider, 0, dataSize(networkInfo_.provider));
	networkInfo_.signalStrength = NO_SIGNAL;
	networkInfo_.registrationStatus = NOT_REGISTERED;
	// memset(networkInfo_.address, 0, dataSize(networkInfo_.address));
	serialMode_ = STOP_NETWORK;
	// mqtt_[1].mode = INIT_MQTT;
	gps_.state = GPS_STATE_NULL;
}

static void gpsStart(void)
{
	gpsInit();
	while (f_gps_init)
		;

	ESP_LOGI(TAG_GPS, "GPS Start");
	char command[BUF_SIZE_32];
	sprintf(command, GPS_POWER, 1);
	do
	{
		send_command(CHECK_GPS_POWER);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		if (gps_.state == GPS_STATE_STOPED)
		{
			send_command(command);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	} while (gps_.state != GPS_STATE_STARTED);

	send_command(FUNCTION_GPS); //	AT+CFUN=4
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	/*
	send_command(GPS_GET_CONFIG); //	AT$GPSCFG?
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	*/
	send_command(AUTO_POSITION_GPS);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	static int timeout = 0;
	do
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(POSITION_GPS);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
		timeout = 0;
	} while (gpsFlag_ != true &&
			 gpsMode_ == START_GPS &&
			 serialMode_ == MODE_SEARCH_GPS);
	gpsMode_ = STOP_GPS;
}

static void dateTimeProvider(void)
{
	// Set Datetime UTC
	send_command("AT+CIMI");
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	send_command(LTE_SUPPORT_COMMAND);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	send_command(CHECK_ADDRESS_COMMAND);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	static int timeout = 0;
	do
	{
		send_command(UTC_GET_TIME);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		timeout++;
	} while (serialMode_ == DATE_TIME_PROVIDER_START && timeout < COUNT_TIME_OUT);
}

static void gpsStop(void)
{
	if (gps_.state == GPS_STATE_STOPED)
		return;
	ESP_LOGI(TAG_GPS, "Stop gps");
	send_command(CHECK_GPS_POWER);
	char command[BUF_SIZE_32];
	sprintf(command, GPS_POWER, 0);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	while (gps_.state != GPS_STATE_STOPED)
	{
		send_command(command);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		send_command(CHECK_GPS_POWER);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	gps_power(0);
	// send_command(FUNCTION_SAVING);
	send_command("AT+CFUN=1");
	vTaskDelay(100 / portTICK_PERIOD_MS);
}

static void mqttDisconnect(uint8_t type)
{
	if (type != 1 && type != 2)
	{
		return;
	}
	ESP_LOGI(TAG_NET, "MQTT Disconnect");
	char command[16] = {0};
	sprintf(command, "AT#MQDISC=%d", mqtt_[type].channel);
	send_command(command);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	mqtt_[type].mode = DISCONNECTED_MQTT;
}

static void mqttConnect(uint8_t type)
{
	ESP_LOGI(TAG_NET, "MQTT connecting");
	static int timeout = 0;
	char commandConn[BUF_SIZE_512];
	memset(commandConn, 0, BUF_SIZE_512);
	sprintf(commandConn,
			CONNECT_MQTT_COMMAND,
			mqtt_[type].channel,
			mac_,
			mqtt_[type].userName,
			mqtt_[type].passWord);

	char commandDisConn[BUF_SIZE_32];
	memset(commandDisConn, 0, BUF_SIZE_32);
	sprintf(commandDisConn,
			DISCONNECT_MQTT_CONNECTED,
			mqtt_[type].channel);
	while (mqtt_[type].connectState != CONNECTED_STATE && serialMode_ == MODE_MQTT_STATR)
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			send_command(CHECK_MQTT_CONNECTED);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		if (mqtt_[type].connectState > 1)
		{
			send_command(commandDisConn);
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		send_command(commandConn);
		timeout = 1;
		// ESP_LOGI(TAG_NET, "Connecting mqtt - Send AT command:%s", commandConn);
	}
	timeout = 0;
	ESP_LOGI(TAG_NET, "MQTT End connect");
}

static void mqttInit(uint8_t type)
{
	ESP_LOGI(TAG_NET, "MQTT init");
	/*
	uart_write_bytes(EX_UART_NUM,
					 ACTIVE_PDP_COMMAND,
					 sizeof(ACTIVE_PDP_COMMAND) - 1);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "activate the context:%s", ACTIVE_PDP_COMMAND);
	*/
	char command[BUF_SIZE_128] = {0};

	sprintf(command,
			ENABLE_MQTT_COMMAND,
			mqtt_[type].channel,
			mqtt_[type].state);
	send_command(command);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "enable the MQTT client:%s", command);
	memset(command, 0, BUF_SIZE_128);
	sprintf(command,
			CONFIG_MQTT_COMMAND,
			mqtt_[type].channel,
			mqtt_[type].hostname,
			mqtt_[type].port);
	send_command(command);
	ESP_LOGI(TAG_NET, "Configure MQTT:%s", command);
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	memset(command, 0, BUF_SIZE_128);
	sprintf(command,
			CONFIG_ADD_MQTT_COMMAND,
			mqtt_[type].channel,
			mqtt_[type].keepAlive);
	send_command(command);
	ESP_LOGI(TAG_NET, "Configure Additional MQTT:%s", command);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	mqtt_[type].mode = INITED_MQTT;
	mqtt_[type].connectState = INITED_AND_NOT_CONNECTED;
	ESP_LOGI(TAG_NET, "MQTT initialized");
	mqttConnect(type);
}

static void mqttPublish(void)
{
	// uart_write_bytes(EX_UART_NUM, cmdPublisher_, dataSize(cmdPublisher_));
	send_command(cmdPublisher_);
	mqtt_[1].mode = CONNECTED_MQTT;
}
/*
static void mqttSubscribe(void)
{
	uart_write_bytes(EX_UART_NUM, cmdSubscribe_, dataSize(cmdSubscribe_));
	mqtt_[1].mode = SUBSCRIBE_MQTT;
}*/
void me310_power(bool value)
{
	gpio_set_level(GPIO_OUTPUT_IO_0, value);
}

void me310_WakeUp4g(bool value)
{
	gpio_set_direction(GPIO_OUTPUT_IO_1, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_OUTPUT_IO_1, value);
}

void gps_power(bool value)
{
	// GPIO_OUTPUT_IO_2
	gpio_pad_select_gpio(GPIO_NUM_27);
	gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_27, value);
}

void me310_init(char *mac)
{
	memcpy(mac_, mac, dataSize(mac));
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	// Install UART driver, and get the queue.
	uart_driver_install(EX_UART_NUM, BUF_SIZE_1024 * 2, BUF_SIZE_1024 * 2, 20, &uart0_queue, 0);
	uart_param_config(EX_UART_NUM, &uart_config);

	// Set UART log level

	// Set UART pins (using UART0 default pins ie no changes.)
	uart_set_pin(EX_UART_NUM, TXD, RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	// Set uart pattern detect function.
	uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
	// Reset the pattern queue length to record at most 20 pattern positions.
	uart_pattern_queue_reset(EX_UART_NUM, 20);

	gpio_config_t io_conf;
	// disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	// set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	// bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	// disable pull-down mode
	io_conf.pull_down_en = 0;
	// disable pull-up mode
	io_conf.pull_up_en = 0;
	// configure GPIO with the given settings
	gpio_config(&io_conf);

	vTaskDelay(100 / portTICK_PERIOD_MS);
	me310_power(ON);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	me310_WakeUp4g(ON);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG_NET, "me310 inited");
}

struct NetworkInfo get_network_info(void)
{
	return networkInfo_;
}

void set_serial_mode(enum SerialMode mode)
{
	serialMode_ = mode;
}
uint8_t get_serial_mode()
{
	return serialMode_;
}

void set_gps_tbf(int time)
{
	ESP_LOGI(TAG_GPS, "GPS tbf: %d", time);
	timeBetweenFix_ = time;
}

void set_gps_mode(enum GPSMode mode)
{
	gpsMode_ = mode;
}

struct Gps get_gps(void)
{
	return gps_;
}
struct tm get_time_provider(void)
{
	struct tm result;
	if (utc_time[0] != 0)
	{
		if (strptime(utc_time, "%y/%m/%d,%H:%M:%S+%Z", &result) == NULL)
			ESP_LOGE(TAG_NET, "Format function strptime(%s) failed.", utc_time);
	}
	return result;
}
void network_task(void *pvParameters)
{
	static int timeout = 0;
	while (networkInfo_.uartState == DISCONNECTED)
	{
		if (timeout++ < TIMEOUT && timeout > 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(AT_COMMAND);
		timeout = 1;
	}
	timeout = 0;
	send_command(FUNCTION_SAVING);
	f_init_reply = true;
	f_init_ok = false;
	while (!f_init_ok)
	{
		if (timeout++ < TIMEOUT)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		send_command(FUNCTION_SAVING);
		timeout = 0;
	}
	f_init_reply = false;
	for (;;)
	{
		// printf("gps mode %d\n", gpsMode_);
		// ESP_LOGI(TAG_NET, "serialMode: %d", serialMode_);
		switch (serialMode_)
		{
		case NOT_USE:
		case INITIALIZED_NETWORK:
		case DATE_TIME_PROVIDER_END:
			break;
		case DATE_TIME_PROVIDER_START:
			dateTimeProvider();
			break;
		case INIT_NETWORK:
			networkInit();
			break;
		case CONNECT_NETWORK:
			networkStart();
			break;
		case STOP_NETWORK:
			networkStop();
			break;
		case MODE_SEARCH_GPS:
		{
			switch (gpsMode_)
			{
			case STOP_GPS:
				gpsStop();
				break;
			case START_GPS:
				gpsStart();
				break;
			default:
				break;
			}
			switch (gps_.state)
			{
			case GPS_STATE_REBOOT:
				resetData();
				break;
			default:
				break;
			}
			break;
		}
		default:
			break;
		}
		// ESP_LOGI(TAG_NET, "net work registration State: %d Status: %d ", networkInfo_.registrationState, networkInfo_.registrationStatus);
		// ESP_LOGI(TAG_NET, "MQTT mode: %d", mqtt_[1].mode);
		if (networkInfo_.registrationState == CONNECTED &&
			networkInfo_.registrationStatus != REGISTERING)
		{
			switch (mqtt_[1].mode)
			{
			case DISCONNECT_MQTT:
				mqttDisconnect(1);
				break;
			case INIT_MQTT:
				mqttInit(1);
				break;
			case CONNECT_MQTT:
			{
				mqttConnect(1);
				break;
			}
			case PUBLISH_MQTT:
				mqttPublish();
				break;
			default:
				break;
			}

			switch (mqtt_[0].mode)
			{
			case DISCONNECT_MQTT:
				mqttDisconnect(0);
				break;
			case INIT_MQTT:
				mqttInit(0);
				break;
			case CONNECT_MQTT:
				mqttConnect(0);
				break;
			default:
				break;
			}
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void uart_event_task(void *pvParameters)
{
	uart_event_t event;
	size_t buffered_size;
	uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
	for (;;)
	{
		// Waiting for UART event.
		if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY))
		{
			bzero(dtmp, RD_BUF_SIZE);
			// ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
			switch (event.type)
			{
			// Event of UART receving data
			/*We'd better handler data event fast, there would be much more data events than
				other types of events. If we take too much time on data event, the queue might
				be full.*/
			case UART_DATA:
				// ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
				uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
				// ESP_LOGI(TAG, "[DATA EVT]:");
				dataParse(dtmp, event.size);
				// uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
				break;
			// Event of HW FIFO overflow detected
			case UART_FIFO_OVF:
				ESP_LOGI(TAG_NET, "hw fifo overflow");
				// If fifo overflow happened, you should consider adding flow control for your application.
				// The ISR has already reset the rx FIFO,
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(EX_UART_NUM);
				xQueueReset(uart0_queue);
				break;
			// Event of UART ring buffer full
			case UART_BUFFER_FULL:
				ESP_LOGI(TAG_NET, "ring buffer full");
				// If buffer full happened, you should consider encreasing your buffer size
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(EX_UART_NUM);
				xQueueReset(uart0_queue);
				break;
			// Event of UART RX break detected
			case UART_BREAK:
				ESP_LOGI(TAG_NET, "uart rx break");
				break;
			// Event of UART parity check error
			case UART_PARITY_ERR:
				ESP_LOGI(TAG_NET, "uart parity error");
				break;
			// Event of UART frame error
			case UART_FRAME_ERR:
				ESP_LOGI(TAG_NET, "uart frame error");
				break;
			// UART_PATTERN_DET
			case UART_PATTERN_DET:
				uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
				int pos = uart_pattern_pop_pos(EX_UART_NUM);
				ESP_LOGI(TAG_NET, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
				if (pos == -1)
				{
					// There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
					// record the position. We should set a larger queue size.
					// As an example, we directly flush the rx buffer here.
					uart_flush_input(EX_UART_NUM);
				}
				else
				{
					uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
					uint8_t pat[PATTERN_CHR_NUM + 1];
					memset(pat, 0, sizeof(pat));
					// uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
					ESP_LOGI(TAG_NET, "read data: %s; read pat : %s", dtmp, pat);
				}
				break;
			// Others
			default:
				ESP_LOGI(TAG_NET, "uart event type: %d", event.type);
				break;
			}
		}
	}
	free(dtmp);
	dtmp = NULL;
	vTaskDelete(NULL);
}

void send_command(char *command)
{
	if (dataSize(command))
	{
		char cmd[dataSize(command) + 2];
		memset(cmd, 0, dataSize(command) + 2);
		memcpy(cmd, command, dataSize(command));
		cmd[dataSize(command)] = '\r';
		cmd[dataSize(command) + 1] = '\n';
		uart_write_bytes(EX_UART_NUM, cmd, dataSize(cmd));
		ESP_LOGI("Send command", "%s", command);
	}
}

void mqtt_init(struct Mqtt mqtt, uint8_t type)
{
	mqtt_[type] = mqtt;
	mqtt_[type].mode = INIT_MQTT;
	ESP_LOGI(TAG_NET, "MQTT init: channel %d, state %d, hostname %s, port %d, keepAlive %d, userName %s, passWord %s, connectState %d, mode %d",
			 mqtt_[type].channel, mqtt_[type].state,
			 mqtt_[type].hostname, mqtt_[type].port,
			 mqtt_[type].keepAlive, mqtt_[type].userName,
			 mqtt_[type].passWord, mqtt_[type].connectState,
			 mqtt_[type].mode);
}

int mqtt_publish(const char *topic, const char *data)
{
	if (networkStateCon_ <= ADDR)
	{
		ESP_LOGI(TAG_NET, "networkState:%d", networkStateCon_);
		return -1;
	}
	if (networkInfo_.signalStrength == NO_SIGNAL)
	{
		return -1;
	}

	if (networkInfo_.registrationState != CONNECTED)
	{
		ESP_LOGI(TAG_NET, "network not connected %d", networkInfo_.registrationState);
		return -1;
	}

	if (mqtt_[1].connectState != CONNECTED_STATE)
	{
		ESP_LOGI(TAG_NET, "mqtt not connected %d mode %d \n", mqtt_[1].connectState, mqtt_[1].mode);
		if (mqtt_[1].mode > CONNECT_MQTT || mqtt_[1].mode == DISCONNECTED_MQTT)
		{
			mqtt_[1].mode = CONNECT_MQTT;
		}
		return -1;
	}
	memset(cmdPublisher_, 0, dataSize(cmdPublisher_));
	sprintf(cmdPublisher_, "AT#MQPUBS=2,%s,0,0,\"%s\"", topic, data);
	mqtt_[1].mode = PUBLISH_MQTT;
	return 0;
}
int mqtt_connect(uint8_t type)
{
	if (mqtt_[type].mode == CONNECTED_MQTT)
	{
		ESP_LOGI(TAG_NET, "mqtt connected");
		return 0;
	}
	if (mqtt_[type].mode != INITED_MQTT &&
		mqtt_[type].mode != DISCONNECTED_MQTT)
	{
		ESP_LOGI(TAG_NET, "mqtt not inited");
		return -1;
	}
	mqtt_[type].mode = CONNECT_MQTT;
	return 0;
}

int mqtt_disconnect(uint8_t type)
{
	if (mqtt_[type].mode == DISCONNECTED_MQTT)
	{
		ESP_LOGI(TAG_NET, "mqtt disconnected");
		return 0;
	}

	if (mqtt_[type].mode != INITED_MQTT &&
		mqtt_[type].mode != CONNECTED_MQTT)
	{
		ESP_LOGI(TAG_NET, "mqtt_disconnect mqtt not inited:%d", mqtt_[type].mode);
		return -1;
	}

	mqtt_[type].mode = DISCONNECT_MQTT;
	return 0;
}
void clear_gps_flag(void)
{
	gpsFlag_ = false;
}

bool get_gps_flag(void)
{
	return gpsFlag_;
}

uint8_t get_mqtt_state(uint8_t type)
{
	return mqtt_[type].connectState;
}
