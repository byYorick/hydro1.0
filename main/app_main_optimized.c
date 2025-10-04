#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "sht3x.h"
#include "ccs811.h"
#include "trema_ph.h"
#include "trema_ec.h"
#include "trema_lux.h"
#include "encoder.h"
#include "peristaltic_pump.h"
#include "lcd_ili9341.h"
#include "ui_manager.h"
#include "trema_relay.h"

/* =============================
 *  КОНФИГУРАЦИЯ ПИНОВ
 * ============================= */
// Пины для I2C шины
#define I2C_SCL_PIN         17
#define I2C_SDA_PIN         18

// Пины для энкодера
#define ENC_A_PIN           38
#define ENC_B_PIN           39
#define ENC_SW_PIN          40

// Конфигурация пинов насосов
#define PUMP_PH_ACID_IA     19
#define PUMP_PH_ACID_IB     20
#define PUMP_PH_BASE_IA     21
#define PUMP_PH_BASE_IB     47
#define PUMP_EC_A_IA        38
#define PUMP_EC_A_IB        39
#define PUMP_EC_B_IA        40
#define PUMP_EC_B_IB        41
#define PUMP_EC_C_IA        26
#define PUMP_EC_C_IB        27

// Тег для логирования
static const char *TAG = "app_main";

// Глобальные переменные
static bool system_initialized = false;
static TaskHandle_t sensor_task_handle = NULL;
static TaskHandle_t encoder_task_handle = NULL;

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
 * ============================= */
static esp_err_t init_i2c_bus(void)
{
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "I2C bus initialized successfully");
    return ESP_OK;
}

static esp_err_t init_sensors(void)
{
    esp_err_t ret = ESP_OK;
    
    // Инициализация датчиков с проверкой ошибок
    if (!trema_lux_init()) {
        ESP_LOGW(TAG, "Failed to initialize LUX sensor");
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGI(TAG, "LUX sensor initialized successfully");
    }
    
    if (!trema_ph_init()) {
        ESP_LOGW(TAG, "Failed to initialize pH sensor");
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGI(TAG, "pH sensor initialized successfully");
    }
    
    if (!ccs811_init()) {
        ESP_LOGW(TAG, "Failed to initialize CCS811 sensor");
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGI(TAG, "CCS811 sensor initialized successfully");
    }
    
    if (!trema_ec_init()) {
        ESP_LOGW(TAG, "Failed to initialize EC sensor");
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGI(TAG, "EC sensor initialized successfully");
    }
    
    return ret;
}

static esp_err_t init_encoder(void)
{
    encoder_set_pins(ENC_A_PIN, ENC_B_PIN, ENC_SW_PIN);
    encoder_init();
    ESP_LOGI(TAG, "Encoder initialized successfully");
    return ESP_OK;
}

static esp_err_t init_relay(void)
{
    if (!trema_relay_init()) {
        ESP_LOGW(TAG, "Failed to initialize relay");
        if (trema_relay_is_using_stub_values()) {
            ESP_LOGW(TAG, "Relay is using stub values (not connected)");
        }
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Relay initialized successfully");
    trema_relay_digital_write(0, 1); // Включаем канал 0
    trema_relay_auto_switch(true);
    return ESP_OK;
}

static esp_err_t init_display(void)
{
    lv_disp_t *disp = lcd_ili9341_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "LCD display initialized successfully");
    return ESP_OK;
}

static esp_err_t init_ui_manager(void)
{
    esp_err_t ret = ui_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UI manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "UI manager initialized successfully");
    return ESP_OK;
}

/* =============================
 *  ЗАДАЧИ СИСТЕМЫ
 * ============================= */
static void sensor_task(void *pvParameters)
{
    sensor_data_t sensor_data[SENSOR_COUNT];
    int update_count = 0;
    
    ESP_LOGI(TAG, "Sensor task started");
    
    // Инициализируем данные датчиков
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_data[i].current_value = 0.0f;
        sensor_data[i].target_value = 0.0f;
        sensor_data[i].min_value = 0.0f;
        sensor_data[i].max_value = 0.0f;
        sensor_data[i].alarm_enabled = true;
        sensor_data[i].alarm_low = 0.0f;
        sensor_data[i].alarm_high = 0.0f;
        sensor_data[i].unit = "";
        sensor_data[i].name = "";
        sensor_data[i].description = "";
        sensor_data[i].decimals = 0;
    }
    
    while (1) {
        // Чтение pH датчика
        float ph_value = 6.8f; // Значение по умолчанию
        if (trema_ph_read(&ph_value)) {
            if (trema_ph_get_stability()) {
                ESP_LOGD(TAG, "pH measurement is stable: %.2f", ph_value);
            } else {
                ESP_LOGW(TAG, "pH measurement is not stable");
                if (trema_ph_wait_for_stable_reading(1000)) {
                    trema_ph_read(&ph_value);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to read pH sensor");
        }
        
        // Чтение EC датчика
        float ec_value = 1.5f; // Значение по умолчанию
        if (!trema_ec_read(&ec_value)) {
            ESP_LOGW(TAG, "Failed to read EC sensor");
        } else {
            uint16_t tds_value = trema_ec_get_tds();
            ESP_LOGD(TAG, "EC: %.2f mS/cm, TDS: %u ppm", ec_value, tds_value);
        }
        
        // Чтение датчика температуры и влажности
        float temp_value = 24.5f; // Значение по умолчанию
        float hum_value = 65.0f;  // Значение по умолчанию
        if (!sht3x_read(&temp_value, &hum_value)) {
            ESP_LOGW(TAG, "Failed to read SHT3x sensor");
        }
        
        // Чтение датчика освещенности
        float lux_value = 1200.0f; // Значение по умолчанию
        if (!trema_lux_read_float(&lux_value)) {
            ESP_LOGW(TAG, "Failed to read LUX sensor");
        }
        
        // Чтение CO2 и TVOC из CCS811 датчика
        float co2_value = 450.0f;  // Значение по умолчанию
        float tvoc_value = 10.0f;  // Значение по умолчанию
        if (!ccs811_read_data(&co2_value, &tvoc_value)) {
            ESP_LOGW(TAG, "Failed to read CCS811 sensor");
        }
        
        // Обновляем данные датчиков в UI менеджере
        sensor_data[SENSOR_PH].current_value = ph_value;
        sensor_data[SENSOR_EC].current_value = ec_value;
        sensor_data[SENSOR_TEMPERATURE].current_value = temp_value;
        sensor_data[SENSOR_HUMIDITY].current_value = hum_value;
        sensor_data[SENSOR_LUX].current_value = lux_value;
        sensor_data[SENSOR_CO2].current_value = co2_value;
        
        // Отправляем данные в UI менеджер
        for (int i = 0; i < SENSOR_COUNT; i++) {
            ui_update_sensor_data((sensor_type_t)i, &sensor_data[i]);
        }
        
        update_count++;
        
        // Логируем значения датчиков каждые 10 обновлений
        if (update_count % 10 == 0) {
            ESP_LOGI(TAG, "Sensor readings - pH: %.2f, EC: %.2f, Temp: %.1f, Hum: %.1f, Lux: %.0f, CO2: %.0f", 
                     ph_value, ec_value, temp_value, hum_value, lux_value, co2_value);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Обновление каждые 2 секунды
    }
}

static void encoder_task(void *pvParameters)
{
    QueueHandle_t encoder_queue = encoder_get_event_queue();
    if (encoder_queue == NULL) {
        ESP_LOGE(TAG, "Encoder queue not available");
        vTaskDelete(NULL);
        return;
    }
    
    encoder_event_t event;
    ESP_LOGI(TAG, "Encoder task started");
    
    while (1) {
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!lv_is_initialized()) {
                continue;
            }
            
            // Преобразуем события энкодера в события LVGL
            uint32_t key = 0;
            int32_t diff = 0;
            
            switch (event.type) {
                case ENCODER_EVENT_ROTATE_CW:
                    diff = 1;
                    break;
                case ENCODER_EVENT_ROTATE_CCW:
                    diff = -1;
                    break;
                case ENCODER_EVENT_BUTTON_PRESS:
                    key = LV_KEY_ENTER;
                    break;
                case ENCODER_EVENT_BUTTON_LONG_PRESS:
                    key = LV_KEY_ESC;
                    break;
                case ENCODER_EVENT_BUTTON_RELEASE:
                    // Обрабатываем отпускание кнопки если нужно
                    break;
                default:
                    break;
            }
            
            // Передаем событие в UI менеджер
            if (key != 0 || diff != 0) {
                ui_handle_encoder_event(key, diff);
            }
        }
    }
}

/* =============================
 *  ГЛАВНАЯ ФУНКЦИЯ
 * ============================= */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Hydroponics Monitor System");
    
    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    // Инициализация компонентов системы
    if (init_i2c_bus() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (init_sensors() != ESP_OK) {
        ESP_LOGW(TAG, "Some sensors failed to initialize, continuing with defaults");
    }
    
    if (init_encoder() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize encoder");
        return;
    }
    
    if (init_relay() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize relay, continuing without relay control");
    }
    
    if (init_display() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (init_ui_manager() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UI manager");
        return;
    }
    
    // Создаем задачи
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 3, &sensor_task_handle);
    xTaskCreate(encoder_task, "encoder_task", 3072, NULL, 6, &encoder_task_handle);
    
    system_initialized = true;
    ESP_LOGI(TAG, "System initialization completed successfully");
    
    // Главный цикл
    while (1) {
        // Периодически обновляем дисплей
        if (lvgl_lock(40)) { // 25 Hz
            lv_timer_handler();
            lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
