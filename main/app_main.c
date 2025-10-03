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
#include "lvgl_main.h"
#include "trema_relay.h"

/* =============================
 *  КОНФИГУРАЦИЯ ПИНОВ
 * ============================= */
// Пины для I2C шины
#define I2C_SCL_PIN         17  // Пин тактирования I2C
#define I2C_SDA_PIN         18  // Пин данных I2C

// Пины для энкодера
#define ENC_A_PIN           1   // CLK - Пин энкодера (сигнал тактирования)
#define ENC_B_PIN           2   // DT - Пин энкодера (данные)
#define ENC_SW_PIN          3   // Пин кнопки энкодера

// Добавляем определение HIGH для реле
#ifndef HIGH
#define HIGH 1
#endif

// Конфигурация пинов насосов - Используем действительные GPIO пины для ESP32-S3
// Избегаем пинов, которые могут вызвать проблемы
#define PUMP_PH_ACID_IA     19  // Пин управления насосом pH кислоты (IA)
#define PUMP_PH_ACID_IB     20  // Пин управления насосом pH кислоты (IB)
#define PUMP_PH_BASE_IA     21  // Пин управления насосом pH основания (IA)
#define PUMP_PH_BASE_IB     47  // Пин управления насосом pH основания (IB)
#define PUMP_EC_A_IA        38  // Пин управления насосом EC A (IA)
#define PUMP_EC_A_IB        39  // Пин управления насосом EC A (IB)
#define PUMP_EC_B_IA        40  // Пин управления насосом EC B (IA)
#define PUMP_EC_B_IB        41  // Пин управления насосом EC B (IB)
#define PUMP_EC_C_IA        26  // Пин управления насосом EC C (IA)
#define PUMP_EC_C_IB        27  // Пин управления насосом EC C (IB)

// Тег для логирования
static const char *TAG = "app_main";

// Задача обработки событий энкодера для навигации по LVGL
static void encoder_ui_task(void *pv)
{
    QueueHandle_t encoder_queue = encoder_get_event_queue();
    if (encoder_queue == NULL) {
        ESP_LOGE(TAG, "Encoder queue not available");
        vTaskDelete(NULL);
        return;
    }

    encoder_event_t event;
    while (1) {
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!lv_is_initialized()) {
                continue;
            }
            switch (event.type) {
                case ENCODER_EVENT_ROTATE_CW:
                case ENCODER_EVENT_ROTATE_CCW: {
                    if (lvgl_is_detail_screen_open()) {
                        // В деталях вращение игнорируем или используем для прокрутки в будущем
                        break;
                    }
                    int idx = lvgl_get_focus_index();
                    int total = lvgl_get_total_focus_items();
                    if (event.type == ENCODER_EVENT_ROTATE_CW) {
                        idx = (idx + 1) % total;
                    } else {
                        idx = (idx - 1 + total) % total;
                    }
                    ESP_LOGI(TAG, "Encoder rotate %s -> focus %d", event.type == ENCODER_EVENT_ROTATE_CW ? "CW" : "CCW", idx);
                    if (lvgl_lock(100)) {
                        lvgl_set_focus(idx);
                        lvgl_unlock();
                    }
                    break;
                }
                case ENCODER_EVENT_BUTTON_PRESS:
                    // Ничего, ждем release чтобы считать «клик»
                    break;
                case ENCODER_EVENT_BUTTON_RELEASE: {
                    if (lvgl_is_detail_screen_open()) {
                        lvgl_close_detail_screen();
                    } else {
                        int idx = lvgl_get_focus_index();
                        lvgl_open_detail_screen(idx);
                    }
                    break;
                }
                case ENCODER_EVENT_BUTTON_LONG_PRESS:
                    // Зарезервировано для будущих действий (например, меню)
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* =============================
 *  I2C ДРАЙВЕР С МЬЮТЕКСОМ
 * ============================= */
// Инициализация I2C шины с пользовательскими настройками
// Использует предопределенные пины из заголовочного файла i2c_bus.h
static void i2c_bus_init_custom(void)
{
    // Компонент i2c_bus использует предопределенные пины из своего заголовочного файла
    // Если нужно изменить их, модифицируйте i2c_bus.h или расширьте API
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        // Если инициализация I2C шины не удалась, выводим ошибку
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    }
    
    // Тестирование связи I2C с помощью простой записи
    uint8_t test_data[] = {0x01, 0x02, 0x03};
    err = i2c_bus_write(0x21, test_data, sizeof(test_data));
    if (err != ESP_OK) {
        // Если запись не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to write to I2C device: %s", esp_err_to_name(err));
    } else {
        // Если запись прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "Successfully wrote to I2C device");
    }
}

/* =============================
 *  ЗАДАЧА ДАТЧИКОВ
 * ============================= */
// Задача для обработки данных датчиков
void sensor_task(void *pv)
{
    // Значения датчиков
    float ph_value, ec_value, temp_value, hum_value, lux_value, co2_value, tvoc_value;
    
    // Инициализация датчиков
    if (!trema_lux_init()) {
        // Если инициализация датчика освещенности не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to initialize LUX sensor");
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "LUX sensor initialized successfully");
    }
    
    // Инициализация pH датчика
    if (!trema_ph_init()) {
        // Если инициализация pH датчика не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to initialize pH sensor");
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "pH sensor initialized successfully");
    }
    
    // Инициализация CCS811 датчика
    if (!ccs811_init()) {
        // Если инициализация CCS811 датчика не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to initialize CCS811 sensor");
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "CCS811 sensor initialized successfully");
    }
    
    // Инициализация EC датчика
    if (!trema_ec_init()) {
        // Если инициализация EC датчика не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to initialize EC sensor");
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "EC sensor initialized successfully");
    }

    // Более длинная задержка для обеспечения полной инициализации UI
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Счетчик обновлений
    int update_count = 0;
    // Бесконечный цикл задачи
    while (1) {
        // Чтение pH датчика
        if (!trema_ph_read(&ph_value)) {
            // Если чтение pH датчика не удалось, выводим предупреждение
            ESP_LOGW(TAG, "Failed to read pH sensor");
            ph_value = 6.8f; // Значение по умолчанию
        } else {
            // Проверка стабильности измерения
            if (!trema_ph_get_stability()) {
                // Если измерение нестабильно, выводим предупреждение
                ESP_LOGW(TAG, "pH measurement is not stable");
                // Пытаемся дождаться стабильного чтения до 1 секунды
                if (trema_ph_wait_for_stable_reading(1000)) {
                    // Если получили стабильное чтение, читаем значение снова
                    if (trema_ph_read(&ph_value)) {
                        ESP_LOGI(TAG, "pH measurement is now stable: %.2f", ph_value);
                    }
                } else {
                    // Если измерение все еще нестабильно после ожидания, используем последнее чтение
                    ESP_LOGW(TAG, "pH measurement still unstable after waiting, using last reading: %.2f", ph_value);
                }
            } else {
                // Если измерение стабильно, выводим отладочное сообщение
                ESP_LOGD(TAG, "pH measurement is stable: %.2f", ph_value);
            }
        }

        // Чтение EC датчика
        if (!trema_ec_read(&ec_value)) {
            // Если чтение EC датчика не удалось, выводим предупреждение
            ESP_LOGW(TAG, "Failed to read EC sensor");
            ec_value = 1.5f; // Значение по умолчанию
        } else {
            // Опционально получаем значение TDS
            uint16_t tds_value = trema_ec_get_tds();
            // Выводим отладочное сообщение с EC и TDS значениями
            ESP_LOGD(TAG, "EC: %.2f mS/cm, TDS: %u ppm", ec_value, tds_value);
        }

        // Чтение датчика температуры и влажности
        if (!sht3x_read(&temp_value, &hum_value)) {
            // Если чтение SHT3x датчика не удалось, выводим предупреждение
            ESP_LOGW(TAG, "Failed to read SHT3x sensor");
            temp_value = 24.5f; // Значение по умолчанию
            hum_value = 65.0f;  // Значение по умолчанию
        }

        // Чтение датчика освещенности
        if (!trema_lux_read_float(&lux_value)) {
            // Если чтение датчика освещенности не удалось, выводим предупреждение
            ESP_LOGW(TAG, "Failed to read LUX sensor");
            lux_value = 1200.0f; // Значение по умолчанию
        }

        // Чтение CO2 и TVOC из CCS811 датчика
        if (!ccs811_read_data(&co2_value, &tvoc_value)) {
            // Если чтение CCS811 датчика не удалось, выводим предупреждение
            ESP_LOGW(TAG, "Failed to read CCS811 sensor");
            co2_value = 450.0f;  // Значение по умолчанию
            tvoc_value = 10.0f;  // Значение по умолчанию
        }

        // Обновляем пользовательский интерфейс LVGL значениями датчиков
        lvgl_update_sensor_values(
            ph_value,
            ec_value,
            temp_value,
            hum_value,
            lux_value,
            co2_value
        );
        
        // Увеличиваем счетчик обновлений
        update_count++;
        
        // Логируем значения датчиков для отладки
        if (update_count % 10 == 0) { // Логируем каждые 10 обновлений
            ESP_LOGI(TAG, "Sensor readings - pH: %.2f, EC: %.2f, Temp: %.1f, Hum: %.1f, Lux: %.0f, CO2: %.0f, TVOC: %.0f", 
                     ph_value, ec_value, temp_value, hum_value, lux_value, co2_value, tvoc_value);
            
            // Проверяем, используем ли мы заглушечные значения для датчиков
            if (trema_lux_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for LUX sensor");
            }
            
            if (trema_ph_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for pH sensor");
            }
            
            if (trema_ec_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for EC sensor");
            }
        }
        
        // Ждем перед следующим обновлением
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Задача для тестирования энкодера
void encoder_test_task(void *pv)
{
    QueueHandle_t encoder_queue = encoder_get_event_queue();
    if (encoder_queue == NULL) {
        ESP_LOGE(TAG, "Encoder queue not available");
        vTaskDelete(NULL);
        return;
    }
    
    encoder_event_t event;
    while (1) {
        // Ждем события от энкодера с таймаутом 100 мс
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (event.type) {
                case ENCODER_EVENT_ROTATE_CW:
                    ESP_LOGI(TAG, "Encoder rotated CW by %ld steps", (long)event.value);
                    break;
                case ENCODER_EVENT_ROTATE_CCW:
                    ESP_LOGI(TAG, "Encoder rotated CCW by %ld steps", (long)event.value);
                    break;
                case ENCODER_EVENT_BUTTON_PRESS:
                    ESP_LOGI(TAG, "Encoder button pressed (duration: %ld ms)", (long)event.value);
                    break;
                case ENCODER_EVENT_BUTTON_LONG_PRESS:
                    ESP_LOGI(TAG, "Encoder button long pressed (duration: %ld ms)", (long)event.value);
                    break;
                case ENCODER_EVENT_BUTTON_RELEASE:
                    ESP_LOGI(TAG, "Encoder button released");
                    break;
            }
        }
    }
}

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ НАСОСОВ
 * ============================= */
/*
// Инициализация насосов
// Настраивает все системные насосы с соответствующими пинами
static void pumps_init(void)
{
    ESP_LOGI(TAG, "Initializing pumps...");
    ESP_LOGI(TAG, "PUMP_PH_ACID: IA=%d, IB=%d", PUMP_PH_ACID_IA, PUMP_PH_ACID_IB);
    pump_init(PUMP_PH_ACID_IA, PUMP_PH_ACID_IB);
    
    ESP_LOGI(TAG, "PUMP_PH_BASE: IA=%d, IB=%d", PUMP_PH_BASE_IA, PUMP_PH_BASE_IB);
    pump_init(PUMP_PH_BASE_IA, PUMP_PH_BASE_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_A: IA=%d, IB=%d", PUMP_EC_A_IA, PUMP_EC_A_IB);
    pump_init(PUMP_EC_A_IA, PUMP_EC_A_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_B: IA=%d, IB=%d", PUMP_EC_B_IA, PUMP_EC_B_IB);
    pump_init(PUMP_EC_B_IA, PUMP_EC_B_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_C: IA=%d, IB=%d", PUMP_EC_C_IA, PUMP_EC_C_IB);
    pump_init(PUMP_EC_C_IA, PUMP_EC_C_IB);
    
    ESP_LOGI(TAG, "Pumps initialization completed");
}
*/

/* =============================
 *  ГЛАВНАЯ ФУНКЦИЯ
 * ============================= */
// Главная функция приложения
// Инициализирует все компоненты системы и запускает задачи
void app_main(void)
{
    // Инициализация NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Если найдена новая версия NVS, стираем и инициализируем заново
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Инициализация I2C
    i2c_bus_init_custom();
    
    // Небольшая задержка для обеспечения полной инициализации I2C
    vTaskDelay(pdMS_TO_TICKS(100));

    // Инициализация энкодера
    encoder_set_pins(ENC_A_PIN, ENC_B_PIN, ENC_SW_PIN);
    encoder_init();
    
    // Создаем задачу для тестирования энкодера
    // xTaskCreate(encoder_test_task, "encoder_test", 2048, NULL, 5, NULL);
    
    // Инициализация реле
    ESP_LOGI(TAG, "Attempting to initialize relay...");
    if (!trema_relay_init()) {
        // Если инициализация реле не удалась, выводим предупреждение
        ESP_LOGW(TAG, "Failed to initialize relay");
        // Проверяем, используем ли мы заглушечные значения
        if (trema_relay_is_using_stub_values()) {
            ESP_LOGW(TAG, "Relay is using stub values (not connected)");
        }
    } else {
        // Если инициализация прошла успешно, выводим сообщение
        ESP_LOGI(TAG, "Relay initialized successfully");
        // Включаем канал 0 в качестве примера
        trema_relay_digital_write(0, HIGH);
        ESP_LOGI(TAG, "Channel 0 turned ON");
        // Запускаем режим автоматического переключения
        trema_relay_auto_switch(true);
        ESP_LOGI(TAG, "Auto-switching mode started");
    }

    // Инициализация LCD дисплея
    lv_disp_t *disp = lcd_ili9341_init();
    if (disp == NULL) {
        // Если инициализация LCD дисплея не удалась, выводим ошибку и завершаем
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        return;
    }
    
    // Более длинная задержка для обеспечения готовности дисплея
    vTaskDelay(pdMS_TO_TICKS(100));  // Увеличенная задержка до 3 секунд
    
    // Создаем пользовательский интерфейс LCD с использованием компонента lvgl_main
    lvgl_main_init();
    
    // Добавляем небольшую задержку для обеспечения полной инициализации UI
    vTaskDelay(pdMS_TO_TICKS(100));  // Увеличенная задержка
    
    // Принудительно обновляем дисплей для обеспечения правильной инициализации
    if (lvgl_lock(1000)) {  // Увеличен таймаут до 1 секунды
        lv_obj_invalidate(lv_scr_act());
        lv_timer_handler();
        lvgl_unlock();
    } else {
        // Если не удалось получить блокировку LVGL, выводим ошибку
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for initial refresh");
    }
    
    // Более длинная задержка для обеспечения полной инициализации UI
    vTaskDelay(pdMS_TO_TICKS(300));  // Увеличенная задержка до 3 секунд
    
    // Создаем задачу обработки энкодера (навигация)
    xTaskCreate(encoder_ui_task, "encoder_ui", 3072, NULL, 6, NULL);

    // Создаем задачу датчиков с реальными показаниями датчиков
    xTaskCreate(sensor_task, "sensors", 4096, NULL, 3, NULL);

    // Поддерживаем главную задачу активной с повышенным приоритетом для лучшей обработки input
    while (1) {
       // Периодически обновляем дисплей для обеспечения стабильного рендеринга
        if (lvgl_lock(-1)) {
            lv_timer_handler();
            lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Спим для уменьшения использования CPU
    }
}