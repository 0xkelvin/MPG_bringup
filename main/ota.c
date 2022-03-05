
#include "ota.h"
#include "http_client.h"
#include "define.h"
const char *TAG_OTA = "OTA Upgrade";
/*an ota data write buffer ready to write to the flash*/

#define HASH_LEN 32 /* SHA-256 digest length */
char ota_write_data[BUF_SIZE_1024 + 1] = {0};
void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

void task_fatal_error(void)
{
    ESP_LOGE(TAG_OTA, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);
}

void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG_OTA, "%s: %s", label, hash_print);
}
void infinite_loop(void)
{
    int i = 0;
    ESP_LOGI(TAG_OTA, "When a new firmware is available on the server, press the reset button to download it");
    while (1)
    {
        ESP_LOGI(TAG_OTA, "Waiting for a new firmware ... %d", ++i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void ota_example_task(void *pvUrlUpgrade)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG_OTA, "Starting OTA upgrade");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG_OTA, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG_OTA, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG_OTA, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    esp_http_client_config_t config = {
        .url = (char *)pvUrlUpgrade,
        //.cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
    ESP_LOGI(TAG_OTA, "Fimrware update url:%s", config.url);

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG_OTA, "Failed to initialise HTTP connection");
        task_fatal_error();
        return;
    }
    int content_length = 1400 * 1024;
    /*
    very slow
    err = esp_http_client_perform(client);
    int content_length = 0;
    if (err == ESP_OK)
    {
        content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG_OTA, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client), content_length);
    }
    else
    {
        ESP_LOGE(TAG_OTA, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    */
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
        return;
    }
    esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG_OTA, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    esp_app_desc_t new_app_info;
    while (1)
    {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0)
        {
            ESP_LOGE(TAG_OTA, "Error: SSL data read error");
            http_cleanup(client);
            task_fatal_error();
            return;
        }
        else if (data_read > 0)
        {
            if (image_header_was_checked == false)
            {
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
                {
                    // check current version with downloading
                    memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG_OTA, "New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                    {
                        ESP_LOGI(TAG_OTA, "Running firmware version: %s", running_app_info.version);
                    }
                    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                    {
                        ESP_LOGI(TAG_OTA, "Last invalid firmware version: %s", invalid_app_info.version);
                    }

                    // check current version with last invalid partition
                    if (last_invalid_app != NULL)
                    {
                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                        {
                            ESP_LOGW(TAG_OTA, "New version is the same as invalid version.");
                            ESP_LOGW(TAG_OTA, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                            ESP_LOGW(TAG_OTA, "The firmware has been rolled back to the previous version.");
                            http_cleanup(client);
                            infinite_loop();
                        }
                    }
#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW(TAG_OTA, "Current running version is the same as a new. We will not continue the update.");
                        http_cleanup(client);
                        infinite_loop();
                    }
#endif

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG_OTA, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        http_cleanup(client);
                        esp_ota_abort(update_handle);
                        task_fatal_error();
                        return;
                    }
                    ESP_LOGI(TAG_OTA, "esp_ota_begin succeeded");
                }
                else
                {
                    ESP_LOGE(TAG_OTA, "received package is not fit len");
                    http_cleanup(client);
                    esp_ota_abort(update_handle);
                    task_fatal_error();
                    return;
                }
            }
            err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK)
            {
                http_cleanup(client);
                esp_ota_abort(update_handle);
                task_fatal_error();
                return;
            }
            binary_file_length += data_read;
            ESP_LOGI(TAG_OTA, "Downloading at:%d (%d %c)", binary_file_length, (binary_file_length * 100) / content_length, '%');
        }
        else if (data_read == 0)
        {
            /*
             * As esp_http_client_read never returns negative error code, we rely on
             * `errno` to check for underlying transport connectivity closure if any
             */
            if (errno == ECONNRESET || errno == ENOTCONN)
            {
                ESP_LOGE(TAG_OTA, "Connection closed, errno = %d", errno);
                break;
            }
            if (esp_http_client_is_complete_data_received(client) == true)
            {
                ESP_LOGI(TAG_OTA, "Connection closed");
                break;
            }
        }
    }
    ESP_LOGI(TAG_OTA, "Total Write binary data length: %d", binary_file_length);
    if (esp_http_client_is_complete_data_received(client) != true)
    {
        ESP_LOGE(TAG_OTA, "Error in receiving complete file");
        http_cleanup(client);
        esp_ota_abort(update_handle);
        task_fatal_error();
        return;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG_OTA, "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG_OTA, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        http_cleanup(client);
        task_fatal_error();
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        task_fatal_error();
        return;
    }
    ESP_LOGI(TAG_OTA, "Prepare to restart system!");
    esp_restart();
    return;
}

bool diagnostic(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG_OTA, "Diagnostics (5 sec)...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bool diagnostic_is_ok = gpio_get_level(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);

    gpio_reset_pin(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    return diagnostic_is_ok;
}