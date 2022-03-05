#include "storage.h"
#include "define.h"
static const char *TAG = "Storage";
static size_t dataSize(const char *data)
{
    size_t length = 0;
    while (data[length++])
        ;
    return length - 1;
}

int storage_init()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 4,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return -1;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return -2;
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return 0;
}

int storage_read(char *data, size_t *length, char *path)
{
    DIR *d;
    // size_t count = 0;
    struct dirent *dir;
    d = opendir("/spiffs");
    if (d != NULL)
    {
        if ((dir = readdir(d)) == NULL)
        {
            ESP_LOGI(TAG, "No thing to read");
            closedir(d);
            return -1;
        }
        sprintf(path, "/spiffs/%s", dir->d_name);
        ESP_LOGI(TAG, "%s", path);
        closedir(d);
    }
    FILE *f;
    f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -2;
    }
    char line[BUF_SIZE_512];
    while (fgets(line, sizeof(line), f))
    {
        strcat(data, line);
    }
    fclose(f);
    *length = dataSize(data);
    return 0;
}

esp_err_t storage_write(const char *data, const char *type)
{
    if (dataSize(data) > 1000)
    {
        return -2;
    }
    char path[256] = {0};
    FILE *f;
    int count;
    for (count = 0; count < LIMIT_NUMNER_FILE; count++)
    {

        sprintf(path, "/spiffs/storage%s%d.json", type, count);
        f = fopen(path, "r");
        if (f == NULL)
        {
            break;
        }
        fclose(f);
    }

    if (count == LIMIT_NUMNER_FILE)
    {
        count = 0;
        sprintf(path, "/spiffs/storage%s%d.json", type, count);
    }
    // ESP_LOGI(TAG, "path %s",path);
    f = fopen(path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "open file to writting Failed (%s) ", path);
        return -1;
    }
    ESP_LOGI(TAG, "Writing to file %s", path);
    ESP_LOGI(TAG, "Data storage: %s", data);
    fprintf(f, data);
    fclose(f);
    return 0;
}

int storage_erase()
{
    DIR *d;
    // size_t count = 0;
    struct dirent *dir;
    d = opendir("/spiffs");
    char path[264];
    if (d)
    {
        if ((dir = readdir(d)) == NULL)
        {
            closedir(d);
            return -1;
        }
        // ESP_LOGI(TAG, "%s", dir->d_name);
        memset(path, 0, BUF_SIZE_128);
        sprintf(path, "/spiffs/%s", dir->d_name);
        ESP_LOGI(TAG, "Remove file name: %s", path);
        closedir(d);
    }

    remove(path);
    return 0;
}