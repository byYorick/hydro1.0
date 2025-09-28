#include "lvgl_main.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
// Font declarations
LV_FONT_DECLARE(lv_font_montserrat_14)
LV_FONT_DECLARE(lv_font_montserrat_18)
LV_FONT_DECLARE(lv_font_montserrat_20)
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "LVGL_MAIN";

// Элементы экрана и пользовательского интерфейса
static lv_obj_t *screen_main;
static lv_obj_t *screen_settings;
static lv_obj_t *label_title;
static lv_obj_t *cont_ph;
static lv_obj_t *cont_ec;
static lv_obj_t *cont_climate;
static lv_obj_t *cont_hum;  // Новый контейнер для влажности
static lv_obj_t *cont_light;
static lv_obj_t *cont_air;

// Метки значений
static lv_obj_t *label_ph_value;
static lv_obj_t *label_ec_value;
static lv_obj_t *label_temp_value;
static lv_obj_t *label_hum_value;
static lv_obj_t *label_lux_value;
static lv_obj_t *label_co2_value;

// Settings screen elements
static lv_obj_t *label_settings_title;
static lv_obj_t *btn_back;
static lv_obj_t *label_back;
static lv_obj_t *settings_list;

// Стили
static lv_style_t style_title;
static lv_style_t style_label;
static lv_style_t style_value;
static lv_style_t style_unit;
static lv_style_t style_btn;
static lv_style_t style_list;

// LVGL group for encoder navigation
static lv_group_t *encoder_group = NULL;

// УПРОЩЕННАЯ ПАЛИТРА
#define COLOR_NORMAL    lv_color_hex(0x2E7D32)  // Темно-зеленый для нормы
#define COLOR_WARNING   lv_color_hex(0xFF8F00)  // Оранжевый
#define COLOR_DANGER    lv_color_hex(0xD32F2F)  // Красный
#define COLOR_BG        lv_color_white()
#define COLOR_TEXT      lv_color_hex(0x212121)  // Почти черный
#define COLOR_LABEL     lv_color_hex(0x616161)  // Серый для подписей
#define COLOR_PRIMARY   lv_color_hex(0x1976D2)  // Primary blue color

// Очередь для обновлений данных датчиков
static QueueHandle_t sensor_data_queue = NULL;
#define SENSOR_DATA_QUEUE_SIZE 10

// Структура данных датчиков
typedef struct {
    float ph;
    float ec;
    float temp;
    float hum;
    float lux;
    float co2;
} sensor_data_t;

// Объявления функций
static void create_main_ui(void);
static void create_settings_ui(void);
static void display_update_task(void *pvParameters);
static void init_styles(void);
static lv_obj_t* create_sensor_card(lv_obj_t *parent);
static void update_sensor_display(sensor_data_t *data);

// Event handlers
static void back_btn_event_handler(lv_event_t * e);
static void settings_btn_event_handler(lv_event_t * e);

// УЛУЧШЕННАЯ ИНИЦИАЛИЗАЦИЯ СТИЛЕЙ
static void init_styles(void)
{
    // Основной фон
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, COLOR_BG);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_obj_add_style(lv_scr_act(), &style_bg, 0);

    // Заголовок экрана
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, COLOR_TEXT);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_18);
    lv_style_set_text_align(&style_title, LV_TEXT_ALIGN_CENTER);

    // Подпись параметра (pH, Temp и т.д.)
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, COLOR_LABEL);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
    lv_style_set_text_align(&style_label, LV_TEXT_ALIGN_CENTER);

    // Значение (крупное)
    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, COLOR_TEXT); // Будет меняться динамически
    lv_style_set_text_font(&style_value, &lv_font_montserrat_20); // Using 20 instead of 24
    lv_style_set_text_align(&style_value, LV_TEXT_ALIGN_CENTER);

    // Единица измерения
    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, COLOR_LABEL);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_14); // Using 14 instead of 12
    lv_style_set_text_align(&style_unit, LV_TEXT_ALIGN_CENTER);
    
    // Button style
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COLOR_PRIMARY);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_radius(&style_btn, 5);
    lv_style_set_pad_all(&style_btn, 10);
    
    // List style
    lv_style_init(&style_list);
    lv_style_set_bg_color(&style_list, lv_color_white());
    lv_style_set_border_width(&style_list, 1);
    lv_style_set_border_color(&style_list, lv_color_hex(0xDDDDDD));
    lv_style_set_radius(&style_list, 5);
    lv_style_set_pad_all(&style_list, 10);
}

// УПРОЩЕННЫЙ КОНТЕЙНЕР (без рамок и теней!)
static lv_obj_t* create_sensor_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 110, 90);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Прозрачный фон — используем общий фон экрана
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    
    return card;
}

static void create_main_ui(void)
{
    ESP_LOGI(TAG, "Creating main UI screen");
    screen_main = lv_obj_create(NULL);
    lv_obj_clean(screen_main);
    
    ESP_LOGI(TAG, "Main screen created at %p", screen_main);
    
    // Установка фона (уже в init_styles)
    init_styles();

    // Заголовок
    label_title = lv_label_create(screen_main);
    lv_label_set_text(label_title, "Hydroponics");
    lv_obj_add_style(label_title, &style_title, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 8);

    // === СТРОКА 1: pH и EC ===
    cont_ph = create_sensor_card(screen_main);
    lv_obj_align(cont_ph, LV_ALIGN_TOP_LEFT, 5, 40);

    lv_obj_t *ph_label = lv_label_create(cont_ph);
    lv_label_set_text(ph_label, "pH");
    lv_obj_add_style(ph_label, &style_label, 0);
    lv_obj_align(ph_label, LV_ALIGN_TOP_MID, 0, 0);

    label_ph_value = lv_label_create(cont_ph);
    lv_label_set_text(label_ph_value, "--");
    lv_obj_add_style(label_ph_value, &style_value, 0);
    lv_obj_align(label_ph_value, LV_ALIGN_CENTER, 0, -5);

    cont_ec = create_sensor_card(screen_main);
    lv_obj_align_to(cont_ec, cont_ph, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

    lv_obj_t *ec_label = lv_label_create(cont_ec);
    lv_label_set_text(ec_label, "EC");
    lv_obj_add_style(ec_label, &style_label, 0);
    lv_obj_align(ec_label, LV_ALIGN_TOP_MID, 0, 0);

    label_ec_value = lv_label_create(cont_ec);
    lv_label_set_text(label_ec_value, "--");
    lv_obj_add_style(label_ec_value, &style_value, 0);
    lv_obj_align(label_ec_value, LV_ALIGN_CENTER, 0, -5);

    // === СТРОКА 2: Температура и Влажность ===
    cont_climate = create_sensor_card(screen_main);
    lv_obj_align_to(cont_climate, cont_ph, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    lv_obj_t *temp_label = lv_label_create(cont_climate);
    lv_label_set_text(temp_label, "Temp");
    lv_obj_add_style(temp_label, &style_label, 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 0);

    label_temp_value = lv_label_create(cont_climate);
    lv_label_set_text(label_temp_value, "--");
    lv_obj_add_style(label_temp_value, &style_value, 0);
    lv_obj_align(label_temp_value, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *temp_unit = lv_label_create(cont_climate);
    lv_label_set_text(temp_unit, "°C");
    lv_obj_add_style(temp_unit, &style_unit, 0);
    lv_obj_align_to(temp_unit, label_temp_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    cont_hum = create_sensor_card(screen_main); // Новый контейнер для влажности
    lv_obj_align_to(cont_hum, cont_climate, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

    lv_obj_t *hum_label = lv_label_create(cont_hum);
    lv_label_set_text(hum_label, "Hum");
    lv_obj_add_style(hum_label, &style_label, 0);
    lv_obj_align(hum_label, LV_ALIGN_TOP_MID, 0, 0);

    label_hum_value = lv_label_create(cont_hum);
    lv_label_set_text(label_hum_value, "--");
    lv_obj_add_style(label_hum_value, &style_value, 0);
    lv_obj_align(label_hum_value, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *hum_unit = lv_label_create(cont_hum);
    lv_label_set_text(hum_unit, "%");
    lv_obj_add_style(hum_unit, &style_unit, 0);
    lv_obj_align_to(hum_unit, label_hum_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // === СТРОКА 3: Освещение и CO2 ===
    cont_light = create_sensor_card(screen_main);
    lv_obj_align_to(cont_light, cont_climate, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    lv_obj_t *lux_label = lv_label_create(cont_light);
    lv_label_set_text(lux_label, "Light");
    lv_obj_add_style(lux_label, &style_label, 0);
    lv_obj_align(lux_label, LV_ALIGN_TOP_MID, 0, 0);

    label_lux_value = lv_label_create(cont_light);
    lv_label_set_text(label_lux_value, "--");
    lv_obj_add_style(label_lux_value, &style_value, 0);
    lv_obj_align(label_lux_value, LV_ALIGN_CENTER, 0, -5);

    lv_obj_t *lux_unit = lv_label_create(cont_light);
    lv_label_set_text(lux_unit, "lux");
    lv_obj_add_style(lux_unit, &style_unit, 0);
    lv_obj_align_to(lux_unit, label_lux_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    cont_air = create_sensor_card(screen_main);
    lv_obj_align_to(cont_air, cont_light, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

    lv_obj_t *co2_label = lv_label_create(cont_air);
    lv_label_set_text(co2_label, "CO₂");
    lv_obj_add_style(co2_label, &style_label, 0);
    lv_obj_align(co2_label, LV_ALIGN_TOP_MID, 0, 0);

    label_co2_value = lv_label_create(cont_air);
    lv_label_set_text(label_co2_value, "--");
    lv_obj_add_style(label_co2_value, &style_value, 0);
    lv_obj_align(label_co2_value, LV_ALIGN_CENTER, 0, -5);

    lv_obj_t *co2_unit = lv_label_create(cont_air);
    lv_label_set_text(co2_unit, "ppm");
    lv_obj_add_style(co2_unit, &style_unit, 0);
    lv_obj_align_to(co2_unit, label_co2_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    // Add settings button
    lv_obj_t *btn_settings = lv_btn_create(screen_main);
    lv_obj_add_style(btn_settings, &style_btn, 0);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_size(btn_settings, 80, 40);
    lv_obj_add_event_cb(btn_settings, settings_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_settings, LV_OBJ_FLAG_CLICKABLE);
    
    // Add to encoder group for navigation
    if (encoder_group) {
        lv_group_add_obj(encoder_group, btn_settings);
    }
    
    lv_obj_t *label_settings = lv_label_create(btn_settings);
    lv_label_set_text(label_settings, "Settings");
    lv_obj_center(label_settings);
    
    ESP_LOGI(TAG, "Main UI components created successfully");
}

static void create_settings_ui(void)
{
    ESP_LOGI(TAG, "Creating settings UI screen");
    screen_settings = lv_obj_create(NULL);
    ESP_LOGI(TAG, "Settings screen created at %p", screen_settings);
    
    // Settings title
    label_settings_title = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_title, "Settings");
    lv_obj_add_style(label_settings_title, &style_title, 0);
    lv_obj_align(label_settings_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Back button
    btn_back = lv_btn_create(screen_settings);
    lv_obj_add_style(btn_back, &style_btn, 0);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_add_event_cb(btn_back, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_CLICKABLE);
    
    // Add to encoder group for navigation
    if (encoder_group) {
        lv_group_add_obj(encoder_group, btn_back);
    }
    
    label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);
    
    // Settings list
    settings_list = lv_obj_create(screen_settings);
    lv_obj_add_style(settings_list, &style_list, 0);
    lv_obj_set_size(settings_list, 220, 200);
    lv_obj_align(settings_list, LV_ALIGN_CENTER, 0, 20);
    
    // Add some sample settings
    lv_obj_t *label_brightness = lv_label_create(settings_list);
    lv_label_set_text(label_brightness, "Brightness: 80%");
    lv_obj_align(label_brightness, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *label_wifi = lv_label_create(settings_list);
    lv_label_set_text(label_wifi, "WiFi: Connected");
    lv_obj_align_to(label_wifi, label_brightness, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    lv_obj_t *label_calibration = lv_label_create(settings_list);
    lv_label_set_text(label_calibration, "Calibrate Sensors");
    lv_obj_align_to(label_calibration, label_wifi, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    lv_obj_t *label_firmware = lv_label_create(settings_list);
    lv_label_set_text(label_firmware, "Firmware: v1.0.2");
    lv_obj_align_to(label_firmware, label_calibration, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    lv_obj_t *label_reset = lv_label_create(settings_list);
    lv_label_set_text(label_reset, "Factory Reset");
    lv_obj_align_to(label_reset, label_firmware, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    ESP_LOGI(TAG, "Settings UI components created successfully");
}

// Event handler for back button
static void back_btn_event_handler(lv_event_t * e)
{
    lvgl_show_main_screen();
}

// Event handler for settings button
static void settings_btn_event_handler(lv_event_t * e)
{
    lvgl_show_settings_screen();
}

// Обновление отображения датчиков с новыми значениями
static void update_sensor_display(sensor_data_t *data)
{
    char buffer[20];
    
    ESP_LOGI(TAG, "Updating sensor display: pH=%.2f, EC=%.2f, Temp=%.1f", 
             data->ph, data->ec, data->temp);
    
    // Verify that LVGL is initialized and screen is valid
    if (!lv_is_initialized()) {
        ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
        return;
    }
    
    // Additional check to ensure screen is valid
    if (!lv_scr_act()) {
        ESP_LOGW(TAG, "LVGL screen not active, skipping display update");
        return;
    }
    
    // Обновление значения pH
    if (label_ph_value) {
        snprintf(buffer, sizeof(buffer), "%.2f", data->ph);
        lv_label_set_text(label_ph_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->ph < 5.5 || data->ph > 7.5) {
            lv_obj_set_style_text_color(label_ph_value, COLOR_DANGER, 0);
        } else if (data->ph < 6.0 || data->ph > 7.0) {
            lv_obj_set_style_text_color(label_ph_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_ph_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление значения EC
    if (label_ec_value) {
        snprintf(buffer, sizeof(buffer), "%.2f", data->ec);
        lv_label_set_text(label_ec_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->ec < 1.0 || data->ec > 2.5) {
            lv_obj_set_style_text_color(label_ec_value, COLOR_DANGER, 0);
        } else if (data->ec < 1.2 || data->ec > 2.0) {
            lv_obj_set_style_text_color(label_ec_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_ec_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление температуры
    if (label_temp_value) {
        snprintf(buffer, sizeof(buffer), "%.1f", data->temp);
        lv_label_set_text(label_temp_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->temp < 18.0 || data->temp > 30.0) {
            lv_obj_set_style_text_color(label_temp_value, COLOR_DANGER, 0);
        } else if (data->temp < 20.0 || data->temp > 28.0) {
            lv_obj_set_style_text_color(label_temp_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_temp_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление влажности
    if (label_hum_value) {
        snprintf(buffer, sizeof(buffer), "%.1f", data->hum);
        lv_label_set_text(label_hum_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->hum < 40.0 || data->hum > 80.0) {
            lv_obj_set_style_text_color(label_hum_value, COLOR_DANGER, 0);
        } else if (data->hum < 45.0 || data->hum > 75.0) {
            lv_obj_set_style_text_color(label_hum_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_hum_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление освещенности
    if (label_lux_value) {
        snprintf(buffer, sizeof(buffer), "%.0f", data->lux);
        lv_label_set_text(label_lux_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->lux < 200.0 || data->lux > 2000.0) {
            lv_obj_set_style_text_color(label_lux_value, COLOR_DANGER, 0);
        } else if (data->lux < 400.0 || data->lux > 1500.0) {
            lv_obj_set_style_text_color(label_lux_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_lux_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление CO2
    if (label_co2_value) {
        snprintf(buffer, sizeof(buffer), "%.0f", data->co2);
        lv_label_set_text(label_co2_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->co2 > 1200.0) {
            lv_obj_set_style_text_color(label_co2_value, COLOR_DANGER, 0);
        } else if (data->co2 > 800.0) {
            lv_obj_set_style_text_color(label_co2_value, COLOR_WARNING, 0);
        } else {
            lv_obj_set_style_text_color(label_co2_value, COLOR_NORMAL, 0);
        }
    }
    
    // Принудительное обновление экрана
    lv_obj_invalidate(lv_scr_act());
    ESP_LOGI(TAG, "Sensor display update completed");
}

// Задача обновления дисплея
static void display_update_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
        // Ожидание данных датчиков
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Received sensor data from queue: pH=%.2f, EC=%.2f, Temp=%.1f", 
                     sensor_data.ph, sensor_data.ec, sensor_data.temp);
            
            // Попытка блокировки мьютекса, так как API LVGL не являются потокобезопасными
            // Use a longer timeout to avoid contention with the main loop
            if (!lvgl_lock(1000)) {  // Increased timeout to 1000ms
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            // Проверка, что LVGL система инициализирована
            if (lv_is_initialized()) {
                // Обновление отображения датчиков
                update_sensor_display(&sensor_data);
                ESP_LOGI(TAG, "Sensor display updated successfully");
            } else {
                ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
            }
            
            // Освобождение мьютекса
            lvgl_unlock();
        } else {
            ESP_LOGD(TAG, "No sensor data received within timeout");
        }
        
        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Инициализация пользовательского интерфейса LVGL
void lvgl_main_init(void)
{
    // Ensure LVGL is properly initialized and ready
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create queue for sensor data updates
    sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
    if (sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor data queue");
        return;
    }
    ESP_LOGI(TAG, "Sensor data queue created successfully");
    
    // Create display update task
    BaseType_t task_result = xTaskCreate(display_update_task, "display_update", 4096, NULL, 5, NULL);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display update task");
        return;
    }
    ESP_LOGI(TAG, "Display update task created successfully");
    
    // Create LVGL group for encoder navigation
    encoder_group = lv_group_create();
    lv_group_set_default(encoder_group);
    lv_group_set_wrap(encoder_group, true);
    
    // Make sure we can acquire the LVGL lock before creating UI
    ESP_LOGI(TAG, "Attempting to acquire LVGL lock for UI initialization");
    if (lvgl_lock(3000)) {  // Wait up to 3 seconds for the lock
        ESP_LOGI(TAG, "LVGL lock acquired, creating UI");
        create_main_ui();
        create_settings_ui();
        
        // Verify screens were created successfully
        if (screen_main == NULL) {
            ESP_LOGE(TAG, "Failed to create main screen");
            lvgl_unlock();
            return;
        }
        
        if (screen_settings == NULL) {
            ESP_LOGE(TAG, "Failed to create settings screen");
            lvgl_unlock();
            return;
        }
        
        lv_scr_load(screen_main);
        lvgl_unlock();
        ESP_LOGI(TAG, "UI created and loaded successfully");
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for UI initialization");
        return;
    }
    
    // Verify that the screen was loaded
    if (lv_scr_act() == screen_main) {
        ESP_LOGI(TAG, "Main screen is active");
    } else {
        ESP_LOGW(TAG, "Main screen is not active after initialization");
    }
}

// Show settings screen
void lvgl_show_settings_screen(void)
{
    ESP_LOGD(TAG, "Switching to settings screen");
    
    // Verify screen is valid
    if (screen_settings == NULL) {
        ESP_LOGE(TAG, "Settings screen not initialized");
        return;
    }
    
    if (lvgl_lock(100)) {
        lv_scr_load(screen_settings);
        // Set focus to back button when switching to settings screen
        if (encoder_group && btn_back) {
            lv_group_focus_obj(btn_back);
        }
        lvgl_unlock();
        ESP_LOGD(TAG, "Settings screen loaded successfully");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for settings screen");
    }
}

// Show main dashboard screen
void lvgl_show_main_screen(void)
{
    ESP_LOGD(TAG, "Switching to main screen");
    
    // Verify screen is valid
    if (screen_main == NULL) {
        ESP_LOGE(TAG, "Main screen not initialized");
        return;
    }
    
    if (lvgl_lock(100)) {
        lv_scr_load(screen_main);
        // Set focus to settings button when switching to main screen
        // Find the settings button in the main screen
        lv_obj_t *btn_settings = NULL;
        if (screen_main) {
            // Iterate through children to find the settings button
            for (int i = 0; i < lv_obj_get_child_cnt(screen_main); i++) {
                lv_obj_t *child = lv_obj_get_child(screen_main, i);
                if (lv_obj_check_type(child, &lv_btn_class)) {
                    // Check if this is the settings button by looking for a label with "Settings" text
                    lv_obj_t *label = lv_obj_get_child(child, 0);
                    if (label && lv_obj_check_type(label, &lv_label_class)) {
                        const char *text = lv_label_get_text(label);
                        if (text && strcmp(text, "Settings") == 0) {
                            btn_settings = child;
                            break;
                        }
                    }
                }
            }
            
            // Set focus to settings button
            if (encoder_group && btn_settings) {
                lv_group_focus_obj(btn_settings);
            }
        }
        lvgl_unlock();
        ESP_LOGD(TAG, "Main screen loaded successfully");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for main screen");
    }
}

// Обновление значений датчиков на экране
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // Проверка инициализации очереди
    if (sensor_data_queue == NULL) {
        ESP_LOGW(TAG, "Sensor data queue not initialized");
        return;
    }
    
    // Создание структуры данных датчиков
    sensor_data_t sensor_data = {
        .ph = ph,
        .ec = ec,
        .temp = temp,
        .hum = hum,
        .lux = lux,
        .co2 = co2
    };
    
    ESP_LOGI(TAG, "Sending sensor data to queue: pH=%.2f, EC=%.2f, Temp=%.1f", ph, ec, temp);
    
    // Отправка данных датчиков в очередь (неблокирующая)
    // Only send if there's space in the queue
    if (uxQueueSpacesAvailable(sensor_data_queue) > 0) {
        BaseType_t result = xQueueSend(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(100)); // Added timeout
        if (result != pdTRUE) {
            ESP_LOGW(TAG, "Failed to send sensor data to queue");
        } else {
            ESP_LOGI(TAG, "Sensor data sent to queue successfully");
        }
    } else {
        ESP_LOGW(TAG, "Sensor data queue is full, dropping data");
    }
}

// Test function to simulate sensor updates
void lvgl_test_sensor_updates(void)
{
    static float ph = 6.5f;
    static float ec = 1.8f;
    static float temp = 25.0f;
    static float hum = 60.0f;
    static float lux = 1000.0f;
    static float co2 = 450.0f;
    
    // Increment values slightly for each call
    ph += 0.01f;
    ec += 0.005f;
    temp += 0.1f;
    hum += 0.2f;
    lux += 5.0f;
    co2 += 5.0f;
    
    // Keep values within reasonable ranges
    if (ph > 7.5f) ph = 6.5f;
    if (ec > 2.5f) ec = 1.5f;
    if (temp > 30.0f) temp = 20.0f;
    if (hum > 80.0f) hum = 50.0f;
    if (lux > 2000.0f) lux = 800.0f;
    if (co2 > 1200.0f) co2 = 400.0f;
    
    ESP_LOGI(TAG, "Testing sensor updates with simulated values");
    lvgl_update_sensor_values(ph, ec, temp, hum, lux, co2);
}