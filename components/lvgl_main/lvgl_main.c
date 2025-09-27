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

// Стили
static lv_style_t style_title;
static lv_style_t style_label;
static lv_style_t style_value;
static lv_style_t style_unit;

// УПРОЩЕННАЯ ПАЛИТРА
#define COLOR_NORMAL    lv_color_hex(0x2E7D32)  // Темно-зеленый для нормы
#define COLOR_WARNING   lv_color_hex(0xFF8F00)  // Оранжевый
#define COLOR_DANGER    lv_color_hex(0xD32F2F)  // Красный
#define COLOR_BG        lv_color_white()
#define COLOR_TEXT      lv_color_hex(0x212121)  // Почти черный
#define COLOR_LABEL     lv_color_hex(0x616161)  // Серый для подписей

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
static void display_update_task(void *pvParameters);
static void init_styles(void);
static lv_obj_t* create_sensor_card(lv_obj_t *parent);
static void update_sensor_display(sensor_data_t *data);

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
    screen_main = lv_scr_act();
    lv_obj_clean(screen_main);
    
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

    // Очередь и задача (без изменений)
    sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
    xTaskCreate(display_update_task, "display_update", 4096, NULL, 5, NULL);
}

// Обновление отображения датчиков с новыми значениями
static void update_sensor_display(sensor_data_t *data)
{
    char buffer[20];
    
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
}

// Задача обновления дисплея
static void display_update_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    
    while (1) {
        // Ожидание данных датчиков
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Попытка блокировки мьютекса, так как API LVGL не являются потокобезопасными
            if (!lvgl_lock(100)) {  // Increased timeout to 100ms
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            // Проверка, что LVGL система инициализирована
            if (lv_is_initialized()) {
                // Обновление отображения датчиков
                update_sensor_display(&sensor_data);
            } else {
                ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
            }
            
            // Освобождение мьютекса
            lvgl_unlock();
        }
        
        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Инициализация пользовательского интерфейса LVGL
void lvgl_main_init(void)
{
    // Ensure LVGL is properly initialized and ready
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Make sure we can acquire the LVGL lock before creating UI
    if (lvgl_lock(1000)) {  // Wait up to 1 second for the lock
        create_main_ui();
        lvgl_unlock();
    } else {
        ESP_LOGE("LVGL_MAIN", "Failed to acquire LVGL lock for UI initialization");
    }
}

// Обновление значений датчиков на экране
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // Проверка инициализации очереди
    if (sensor_data_queue == NULL) {
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
    
    // Отправка данных датчиков в очередь (неблокирующая)
    // Only send if there's space in the queue
    if (uxQueueSpacesAvailable(sensor_data_queue) > 0) {
        xQueueSend(sensor_data_queue, &sensor_data, 0);
    }
}