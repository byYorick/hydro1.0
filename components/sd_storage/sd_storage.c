/**
 * @file sd_storage.c
 * @brief Реализация SD-карта хранилища данных
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "sd_storage.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>

/// Тег для логирования
static const char *TAG = "SD_STORAGE";

/// Глобальные переменные
static sdmmc_card_t *sd_card = NULL;
static SemaphoreHandle_t sd_mutex = NULL;
static bool sd_mounted = false;

/// Константы путей
#define SD_MOUNT_POINT      "/sdcard"
#define SD_DATA_DIR         SD_MOUNT_POINT "/data"
#define SD_SENSORS_DIR      SD_DATA_DIR "/sensors"
#define SD_EVENTS_DIR       SD_DATA_DIR "/events"
#define SD_CONFIG_DIR       SD_DATA_DIR "/config"

/**
 * @brief Создание структуры каталогов
 */
static esp_err_t create_directory_structure(void) {
    struct stat st;
    
    // Создаем /data
    if (stat(SD_DATA_DIR, &st) != 0) {
        if (mkdir(SD_DATA_DIR, 0775) != 0) {
            ESP_LOGE(TAG, "Ошибка создания %s", SD_DATA_DIR);
            return ESP_FAIL;
        }
    }
    
    // Создаем /data/sensors
    if (stat(SD_SENSORS_DIR, &st) != 0) {
        if (mkdir(SD_SENSORS_DIR, 0775) != 0) {
            ESP_LOGE(TAG, "Ошибка создания %s", SD_SENSORS_DIR);
            return ESP_FAIL;
        }
    }
    
    // Создаем /data/events
    if (stat(SD_EVENTS_DIR, &st) != 0) {
        if (mkdir(SD_EVENTS_DIR, 0775) != 0) {
            ESP_LOGE(TAG, "Ошибка создания %s", SD_EVENTS_DIR);
            return ESP_FAIL;
        }
    }
    
    // Создаем /data/config
    if (stat(SD_CONFIG_DIR, &st) != 0) {
        if (mkdir(SD_CONFIG_DIR, 0775) != 0) {
            ESP_LOGE(TAG, "Ошибка создания %s", SD_CONFIG_DIR);
            return ESP_FAIL;
        }
    }
    
    ESP_LOGI(TAG, "Структура каталогов создана");
    return ESP_OK;
}

/**
 * @brief Инициализация SD-карты
 */
esp_err_t sd_storage_init(const sd_storage_config_t *config) {
    esp_err_t ret = ESP_OK;
    
    if (sd_mounted) {
        ESP_LOGW(TAG, "SD-карта уже смонтирована");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Конфигурация SD не указана");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Создание мьютекса
    if (sd_mutex == NULL) {
        sd_mutex = xSemaphoreCreateMutex();
        if (sd_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    // Конфигурация монтирования
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = config->format_if_mount_failed,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    if (config->mode == SD_MODE_SPI) {
        // SPI режим
        ESP_LOGI(TAG, "Инициализация SD в SPI режиме");
        
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = config->max_frequency / 1000;
        
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = config->cs_pin;
        slot_config.host_id = host.slot;
        
        ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    } else {
        // SDMMC режим
        ESP_LOGI(TAG, "Инициализация SD в SDMMC режиме");
        
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = config->max_frequency / 1000;
        
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        
        if (config->mode == SD_MODE_SDMMC_1BIT) {
            slot_config.width = 1;
        } else {
            slot_config.width = 4;
        }
        
        ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    }
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Ошибка монтирования SD-карты");
        } else {
            ESP_LOGE(TAG, "Ошибка инициализации SD-карты: %s", esp_err_to_name(ret));
        }
        goto cleanup;
    }
    
    // Вывод информации о карте
    sdmmc_card_print_info(stdout, sd_card);
    
    // Создание структуры каталогов
    ret = create_directory_structure();
    if (ret != ESP_OK) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
        goto cleanup;
    }
    
    sd_mounted = true;
    ESP_LOGI(TAG, "SD-карта успешно инициализирована");
    
cleanup:
    xSemaphoreGive(sd_mutex);
    return ret;
}

/**
 * @brief Деинициализация SD-карты
 */
esp_err_t sd_storage_deinit(void) {
    if (!sd_mounted) {
        return ESP_OK;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
    sd_card = NULL;
    sd_mounted = false;
    
    ESP_LOGI(TAG, "SD-карта деинициализирована");
    
    xSemaphoreGive(sd_mutex);
    
    if (sd_mutex != NULL) {
        vSemaphoreDelete(sd_mutex);
        sd_mutex = NULL;
    }
    
    return ret;
}

/**
 * @brief Проверка монтирования
 */
bool sd_storage_is_mounted(void) {
    return sd_mounted;
}

/**
 * @brief Получение имени файла для датчика по дате
 */
static void get_sensor_filename(const char *sensor_name, time_t timestamp, char *filename, size_t max_len) __attribute__((unused));
static void get_sensor_filename(const char *sensor_name, time_t timestamp, char *filename, size_t max_len) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    
    snprintf(filename, max_len, "%s/%s_%04d%02d%02d.csv",
             SD_SENSORS_DIR, sensor_name,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

/**
 * @brief Запись данных датчика
 */
esp_err_t sd_write_sensor_log(const sd_sensor_record_t *record) {
    if (!sd_mounted || record == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    char filename[256];
    struct tm timeinfo;
    localtime_r(&record->timestamp, &timeinfo);
    
    // Общий файл со всеми датчиками за день
    snprintf(filename, sizeof(filename), "%s/all_%04d%02d%02d.csv",
             SD_SENSORS_DIR,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    FILE *f = fopen(filename, "a");
    if (f == NULL) {
        // Создаем файл с заголовком
        f = fopen(filename, "w");
        if (f != NULL) {
            fprintf(f, "timestamp,ph,ec,temperature,humidity,lux,co2\n");
            fclose(f);
            f = fopen(filename, "a");
        }
    }
    
    if (f == NULL) {
        ESP_LOGE(TAG, "Ошибка открытия файла: %s", filename);
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }
    
    fprintf(f, "%lld,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            (long long)record->timestamp,
            record->ph,
            record->ec,
            record->temperature,
            record->humidity,
            record->lux,
            record->co2);
    
    fclose(f);
    
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

/**
 * @brief Запись события
 */
esp_err_t sd_write_event_log(const sd_event_record_t *event) {
    if (!sd_mounted || event == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    char filename[256];
    struct tm timeinfo;
    localtime_r(&event->timestamp, &timeinfo);
    
    // Файл событий за день
    snprintf(filename, sizeof(filename), "%s/alarms_%04d%02d%02d.log",
             SD_EVENTS_DIR,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    FILE *f = fopen(filename, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Ошибка открытия файла событий: %s", filename);
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    fprintf(f, "[%s] [%s] %s: %s\n",
            time_str,
            event->severity,
            event->type,
            event->message);
    
    fclose(f);
    
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

/**
 * @brief Чтение истории датчиков
 */
esp_err_t sd_read_sensor_history(const char *sensor_name, time_t start_time, time_t end_time,
                                  sd_sensor_record_t *records, size_t max_records, size_t *count) {
    if (!sd_mounted || sensor_name == NULL || records == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    // TODO: Реализовать чтение с фильтрацией по времени
    // Это требует парсинга CSV файлов за указанный период
    
    ESP_LOGW(TAG, "Чтение истории датчиков пока не реализовано полностью");
    
    return ESP_OK;
}

/**
 * @brief Чтение событий
 */
esp_err_t sd_read_events(time_t start_time, time_t end_time,
                          sd_event_record_t *events, size_t max_events, size_t *count) {
    if (!sd_mounted || events == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    // TODO: Реализовать чтение событий с фильтрацией по времени
    
    ESP_LOGW(TAG, "Чтение событий пока не реализовано полностью");
    
    return ESP_OK;
}

/**
 * @brief Получение статистики SD-карты
 */
esp_err_t sd_get_storage_stats(sd_storage_stats_t *stats) {
    if (!sd_mounted || stats == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    FATFS *fs;
    DWORD fre_clust;
    
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
        uint64_t free_sectors = fre_clust * fs->csize;
        
        stats->total_bytes = total_sectors * 512ULL;
        stats->free_bytes = free_sectors * 512ULL;
        stats->used_bytes = stats->total_bytes - stats->free_bytes;
    } else {
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }
    
    // TODO: Подсчет количества записей
    stats->sensor_records = 0;
    stats->event_records = 0;
    
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

/**
 * @brief Синхронизация с облаком
 */
esp_err_t sd_sync_to_cloud(void) {
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // TODO: Реализовать синхронизацию через MQTT
    // 1. Прочитать несинхронизированные данные
    // 2. Агрегировать по часам
    // 3. Отправить через mqtt_publish_*
    // 4. Пометить как синхронизированные
    
    ESP_LOGW(TAG, "Синхронизация с облаком пока не реализована");
    
    return ESP_OK;
}

/**
 * @brief Очистка старых данных
 */
esp_err_t sd_cleanup_old_data(uint32_t days_to_keep) {
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // time_t now = time(NULL);
    // Удаление файлов старше days_to_keep дней
    // time_t cutoff_time = now - (days_to_keep * 24 * 60 * 60);
    (void)days_to_keep; // Временно не используется
    
    // TODO: Реализовать удаление файлов старше cutoff_time
    
    ESP_LOGI(TAG, "Очистка данных старше %d дней", days_to_keep);
    
    return ESP_OK;
}

/**
 * @brief Сохранение конфигурации
 */
esp_err_t sd_save_config(const char *config_name, const char *json_data) {
    if (!sd_mounted || config_name == NULL || json_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/%s.json", SD_CONFIG_DIR, config_name);
    
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Ошибка создания файла конфигурации: %s", filename);
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_data);
    fclose(f);
    
    ESP_LOGI(TAG, "Конфигурация сохранена: %s", config_name);
    
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

/**
 * @brief Загрузка конфигурации
 */
esp_err_t sd_load_config(const char *config_name, char *json_data, size_t max_len) {
    if (!sd_mounted || config_name == NULL || json_data == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/%s.json", SD_CONFIG_DIR, config_name);
    
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Файл конфигурации не найден: %s", config_name);
        xSemaphoreGive(sd_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    size_t bytes_read = fread(json_data, 1, max_len - 1, f);
    json_data[bytes_read] = '\0';
    
    fclose(f);
    
    ESP_LOGI(TAG, "Конфигурация загружена: %s (%d байт)", config_name, bytes_read);
    
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

/**
 * @brief Экспорт агрегированных данных
 */
esp_err_t sd_export_aggregated_data(time_t start_time, time_t end_time, const char *output_file) {
    if (!sd_mounted || output_file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Реализовать агрегацию данных по часам для экспорта
    
    ESP_LOGW(TAG, "Экспорт агрегированных данных пока не реализован");
    
    return ESP_OK;
}

/**
 * @brief Форматирование SD-карты
 */
esp_err_t sd_format(void) {
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGW(TAG, "Форматирование SD-карты требует деинициализации и повторной инициализации");
    
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief Проверка целостности данных
 */
esp_err_t sd_check_integrity(void) {
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Проверяем наличие структуры каталогов
    struct stat st;
    
    if (stat(SD_DATA_DIR, &st) != 0 ||
        stat(SD_SENSORS_DIR, &st) != 0 ||
        stat(SD_EVENTS_DIR, &st) != 0 ||
        stat(SD_CONFIG_DIR, &st) != 0) {
        ESP_LOGE(TAG, "Нарушена структура каталогов");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Проверка целостности прошла успешно");
    
    return ESP_OK;
}

