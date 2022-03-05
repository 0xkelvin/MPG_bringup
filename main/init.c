#include "init.h"

static const adc_channel_t vBat = ADC_CHANNEL_7;		//	battery voltage
static const adc_channel_t vThermister = ADC_CHANNEL_6; // 	thermister
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

// static const adc_atten_t 		atten0 		= 	ADC_ATTEN_DB_0				;
// static const adc_atten_t 		atten25		= 	ADC_ATTEN_DB_2_5			;
// static const adc_atten_t 		atten6 		= 	ADC_ATTEN_DB_6				;
// static const adc_atten_t 		atten11		= 	ADC_ATTEN_DB_11				;
static const char *TAG_INIT = "T-INIT ->";
void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
	//	Check if TP is burned into eFuse
	if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
		ESP_LOGI(TAG_INIT, "eFuse Two Point : Supported");
	else
		ESP_LOGI(TAG_INIT, "eFuse Two Point : NOT supported");

	//	Check Vref is burned into eFuse
	if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
		ESP_LOGI(TAG_INIT, "eFuse Vref : Supported\n");
	else
		ESP_LOGI(TAG_INIT, "eFuse Vref : NOT supported");

#elif CONFIG_IDF_TARGET_ESP32S2
	if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
		ESP_LOGI(TAG_INIT, "eFuse Two Point : Supported\n");
	else
		ESP_LOGI(TAG_INIT, "Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");

#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

void initADC(void)
{
	check_efuse();

	adc1_config_width(width);
	adc1_config_channel_atten(vBat, ADC_ATTEN_DB_0);
	adc1_config_channel_atten(vThermister, ADC_ATTEN_11db);

	ESP_LOGI(TAG_INIT, "Init ADC Successfully");
}

void initIO(void)
{
	//	reset all io to default state
	gpio_reset_pin(buttonScanDevice);
	gpio_reset_pin(buzzer);
	gpio_reset_pin(chargeDetect);
	gpio_reset_pin(gpsOnOff);
	gpio_reset_pin(icm20789Int);
	gpio_reset_pin(ledDev1);
	gpio_reset_pin(ledDev2);
	gpio_reset_pin(mcuOnOffWake4g);
	gpio_reset_pin(mcuSCL);
	gpio_reset_pin(mcuSDA);
	gpio_reset_pin(me310SLedIn);
	gpio_reset_pin(me310SPwrMon);
	gpio_reset_pin(power4gOnOff);
	gpio_reset_pin(powerSensorOnOff);
	gpio_reset_pin(voltageSensor);
	gpio_reset_pin(voltageTempSensor);

	//	set gpio in / out
	gpio_set_direction(buttonScanDevice, GPIO_MODE_INPUT); //	scan button
	gpio_set_direction(chargeDetect, GPIO_MODE_INPUT);	   //	detect charger state
	gpio_set_direction(icm20789Int, GPIO_MODE_INPUT);	   //	g-sensor interrupt
	gpio_set_direction(me310SLedIn, GPIO_MODE_INPUT);	   //	me310 led status signal
	gpio_set_direction(me310SPwrMon, GPIO_MODE_INPUT);	   //	me310 1.8V monitor pin

	gpio_set_direction(buzzer, GPIO_MODE_OUTPUT);			//	buzzer
	gpio_set_direction(gpsOnOff, GPIO_MODE_OUTPUT);			//	enable gps lna
	gpio_set_direction(ledDev1, GPIO_MODE_OUTPUT);			//	status LED
	gpio_set_direction(ledDev2, GPIO_MODE_OUTPUT);			//	status LED
	gpio_set_direction(mcuOnOffWake4g, GPIO_MODE_OUTPUT);	//	me310 wake up pin
	gpio_set_direction(power4gOnOff, GPIO_MODE_OUTPUT);		//	me310 power on off
	gpio_set_direction(powerSensorOnOff, GPIO_MODE_OUTPUT); //	all peripheral power

	//	set output port initial level
	gpio_set_level(buzzer, 0);
	gpio_set_level(gpsOnOff, 0);
	gpio_set_level(ledDev1, 0);
	gpio_set_level(ledDev2, 0);
	gpio_set_level(mcuOnOffWake4g, 0);
	gpio_set_level(power4gOnOff, 0);
	gpio_set_level(powerSensorOnOff, 0);

	ESP_LOGI(TAG_INIT, "Init IO Successfully");
}

void initFlash(void)
{
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();

		ESP_LOGI(TAG_INIT, "Erase Flash Finish");
	}

	ESP_LOGI(TAG_INIT, "Init Flash Finish");
}
