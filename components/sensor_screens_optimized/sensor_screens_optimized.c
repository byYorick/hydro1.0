#include "sensor_screens_optimized.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SENSOR_SCREENS_OPT";

// Данные датчиков
static sensor_data_t sensor_data[SENSOR_COUNT];
static bool initialized = false;

// Метаданные датчиков (дублируем для совместимости)
static const struct {
    const char *name;
    const char *unit;
    const char *description;
    float min_value;
    float max_value;
    float default_target;
    float alarm_low;
    float alarm_high;
    uint8_t decimals;
} sensor_metadata[SENSOR_COUNT] = {
    [SENSOR_PH] = {
        .name = "pH",
        .unit = "",
        .description = "Keep the nutrient solution balanced for optimal uptake.",
        .min_value = 4.0f,
        .max_value = 9.0f,
        .default_target = 6.8f,
        .alarm_low = 6.0f,
        .alarm_high = 7.5f,
        .decimals = 2
    },
    [SENSOR_EC] = {
        .name = "EC",
        .unit = "mS/cm",
        .description = "Electrical conductivity shows nutrient strength. Stay in range!",
        .min_value = 0.0f,
        .max_value = 3.0f,
        .default_target = 1.5f,
        .alarm_low = 0.8f,
        .alarm_high = 2.0f,
        .decimals = 2
    },
    [SENSOR_TEMPERATURE] = {
        .name = "Temperature",
        .unit = "°C",
        .description = "Keep solution and air temperature comfortable for the crop.",
        .min_value = 15.0f,
        .max_value = 35.0f,
        .default_target = 24.0f,
        .alarm_low = 18.0f,
        .alarm_high = 30.0f,
        .decimals = 1
    },
    [SENSOR_HUMIDITY] = {
        .name = "Humidity",
        .unit = "%",
        .description = "Stable humidity reduces stress and supports steady growth.",
        .min_value = 20.0f,
        .max_value = 100.0f,
        .default_target = 70.0f,
        .alarm_low = 45.0f,
        .alarm_high = 75.0f,
        .decimals = 1
    },
    [SENSOR_LUX] = {
        .name = "Light",
        .unit = "lux",
        .description = "Monitor light levels to maintain healthy photosynthesis.",
        .min_value = 0.0f,
        .max_value = 2500.0f,
        .default_target = 500.0f,
        .alarm_low = 400.0f,
        .alarm_high = 1500.0f,
        .decimals = 0
    },
    [SENSOR_CO2] = {
        .name = "CO2",
        .unit = "ppm",
        .description = "Avoid excessive CO2 to keep plants and people comfortable.",
        .min_value = 0.0f,
        .max_value = 2000.0f,
        .default_target = 450.0f,
        .alarm_low = 0.0f,
        .alarm_high = 800.0f,
        .decimals = 0
    }
};

static esp_err_t init_sensor_data(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_data[i].current_value = 0.0f;
        sensor_data[i].target_value = sensor_metadata[i].default_target;
        sensor_data[i].min_value = sensor_metadata[i].min_value;
        sensor_data[i].max_value = sensor_metadata[i].max_value;
        sensor_data[i].alarm_enabled = true;
        sensor_data[i].alarm_low = sensor_metadata[i].alarm_low;
        sensor_data[i].alarm_high = sensor_metadata[i].alarm_high;
        sensor_data[i].unit = sensor_metadata[i].unit;
        sensor_data[i].name = sensor_metadata[i].name;
        sensor_data[i].description = sensor_metadata[i].description;
        sensor_data[i].decimals = sensor_metadata[i].decimals;
    }
    return ESP_OK;
}

esp_err_t sensor_screens_optimized_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing optimized sensor screens");
    
    esp_err_t ret = init_sensor_data();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor data");
        return ret;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Optimized sensor screens initialized successfully");
    return ESP_OK;
}

esp_err_t sensor_screens_update_data(sensor_type_t sensor_type, float current_value, float target_value)
{
    if (!initialized || sensor_type >= SENSOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Обновляем локальные данные
    sensor_data[sensor_type].current_value = current_value;
    sensor_data[sensor_type].target_value = target_value;
    
    // Отправляем в UI менеджер
    return ui_update_sensor_data(sensor_type, &sensor_data[sensor_type]);
}

esp_err_t sensor_screens_update_ph_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_PH, current_value, target_value);
}

esp_err_t sensor_screens_update_ec_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_EC, current_value, target_value);
}

esp_err_t sensor_screens_update_temp_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_TEMPERATURE, current_value, target_value);
}

esp_err_t sensor_screens_update_humidity_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_HUMIDITY, current_value, target_value);
}

esp_err_t sensor_screens_update_lux_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_LUX, current_value, target_value);
}

esp_err_t sensor_screens_update_co2_data(float current_value, float target_value)
{
    return sensor_screens_update_data(SENSOR_CO2, current_value, target_value);
}
