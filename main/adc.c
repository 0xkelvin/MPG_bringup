#include "adc.h"
#include "esp_log.h"
static esp_adc_cal_characteristics_t *batterry_chars;
static esp_adc_cal_characteristics_t *temperature_chars;
static const adc_channel_t batterry_channel = ADC_CHANNEL_6; // GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t temperature_channel = ADC_CHANNEL_7;
static const adc_channel_t detect_charge = ADC2_CHANNEL_2;

static const char *TAG = "ADC";
void powSensor(bool pow)
{
    gpio_pad_select_gpio(POWER_SENSOR_ON_OFF);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(POWER_SENSOR_ON_OFF, GPIO_MODE_OUTPUT);

    gpio_set_level(POWER_SENSOR_ON_OFF, pow);
}
void adcSetup()
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(batterry_channel, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(temperature_channel, ADC_ATTEN_DB_11);

    // Characterize ADC
    batterry_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    temperature_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, DEFAULT_VREF, batterry_chars);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 3300, temperature_chars);
}

int getBattery()
{
    uint32_t batterry_raw = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        batterry_raw += adc1_get_raw((adc1_channel_t)batterry_channel);
    }
    batterry_raw /= NO_OF_SAMPLES;
    // Convert batterry_raw to batterry_voltage in mV
    uint32_t batterry_voltage = esp_adc_cal_raw_to_voltage(batterry_raw, batterry_chars);
    ESP_LOGI(TAG, "Raw: %d\tVoltage: %dmV", batterry_raw, batterry_voltage);
    if (batterry_voltage < 567)
        return 0;
    int percent = (batterry_voltage - 567) * 100 / 133;
    if (percent > 100)
    {
        percent = 100;
    }
    return (percent + 0.5);
}

float getTemperature()
{
    uint32_t temperature_raw = 0;
    // Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        temperature_raw += adc1_get_raw((adc1_channel_t)temperature_channel);
    }
    temperature_raw /= NO_OF_SAMPLES;
    // Convert batterry_raw to batterry_voltage in mV
    uint32_t temperature_voltage = esp_adc_cal_raw_to_voltage(temperature_raw, temperature_chars);
    // ESP_LOGI(TAG, "temperature raw: %d\tTemperatureVoltage: %dmV", temperature_raw, temperature_voltage);
    float R = 100000 * temperature_voltage / (2650 - temperature_voltage);
    float Beta = 3950.0; // Beta value
    float To = 298.15;   // Temperature in Kelvin for 25 degree Celsius
    float Ro = 100000.0; // Resistance of Thermistor at 25 degree Celsius
    float temperarture = 1.0 / (1 / To + log(R / Ro) / Beta);
    return temperarture - 273.15;
}
// read Voltage
float readVoltage(void)
{
    float voltage;
    uint32_t adc_reading = 0;
    adc_reading = adc1_get_raw((adc2_channel_t)detect_charge);
    voltage = (adc_reading * 0.95 / 4095) * 410 / 20;
    return voltage;
}