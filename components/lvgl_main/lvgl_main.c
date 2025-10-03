#include "lvgl_main.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
// Объявления шрифтов
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
static lv_obj_t *screen_detail;
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

// Глобальная группа фокуса для энкодера
static lv_group_t *encoder_group = NULL;

// Массив контейнеров для навигации
static lv_obj_t *sensor_containers[6];

// Стили
static lv_style_t style_title;
static lv_style_t style_label;
static lv_style_t style_value;
static lv_style_t style_unit;
static lv_style_t style_focus;  // Стиль для элемента с фокусом

// УПРОЩЕННАЯ ПАЛИТРА
#define COLOR_NORMAL    lv_color_hex(0x2E7D32)  // Темно-зеленый для нормы
#define COLOR_WARNING   lv_color_hex(0xFF8F00)  // Оранжевый
#define COLOR_DANGER    lv_color_hex(0xD32F2F)  // Красный
#define COLOR_BG        lv_color_white()
#define COLOR_TEXT      lv_color_hex(0x212121)  // Почти черный
#define COLOR_LABEL     lv_color_hex(0x616161)  // Серый для подписей
#define COLOR_FOCUS     lv_color_hex(0x1976D2)  // Синий для фокуса

// Очередь для обновлений данных датчиков
static QueueHandle_t sensor_data_queue = NULL;
#define SENSOR_DATA_QUEUE_SIZE 10

// Переменные для управления фокусом
static int current_focus_index = 0;
static const int total_focus_items = 6;

// Объявления функций
static void create_main_ui(void);
static void display_update_task(void *pvParameters);
static void init_styles(void);
static lv_obj_t* create_sensor_card(lv_obj_t *parent);
static void update_sensor_display(sensor_data_t *data);
static void create_detail_ui(int index);

// Функция для получения дескриптора очереди данных датчиков
void* lvgl_get_sensor_data_queue(void)
{
    return (void*)sensor_data_queue;
}

// Функция для обновления значений датчиков на дисплее из структуры данных
void lvgl_update_sensor_values_from_queue(sensor_data_t *data)
{
    // Проверка инициализации очереди
    if (sensor_data_queue == NULL) {
        // Если очередь не инициализирована, выходим из функции
        return;
    }
    
    // Отправка данных датчиков в очередь (неблокирующая)
    // Отправляем только если есть место в очереди
    if (uxQueueSpacesAvailable(sensor_data_queue) > 0) {
        // Помещаем данные в очередь без ожидания
        xQueueSend(sensor_data_queue, data, 0);
    }
}

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
    
    // Стиль для элемента с фокусом
    lv_style_init(&style_focus);
    lv_style_set_border_color(&style_focus, COLOR_FOCUS);
    lv_style_set_border_width(&style_focus, 2);
    lv_style_set_border_opa(&style_focus, LV_OPA_COVER);
    lv_style_set_shadow_color(&style_focus, COLOR_FOCUS);
    lv_style_set_shadow_width(&style_focus, 10);
    lv_style_set_shadow_spread(&style_focus, 3);
}

// УПРОЩЕННЫЙ КОНТЕЙНЕР (без рамок и теней!)
// Создает карточку сенсора для отображения данных
static lv_obj_t* create_sensor_card(lv_obj_t *parent)
{
    // Создаем объект контейнера для карточки
    lv_obj_t *card = lv_obj_create(parent);
    // Устанавливаем размер карточки (110x90 пикселей)
    lv_obj_set_size(card, 110, 90);
    // Отключаем возможность прокрутки содержимого
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Прозрачный фон — используем общий фон экрана
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    // Убираем рамку у контейнера
    lv_obj_set_style_border_width(card, 0, 0);
    // Убираем внутренние отступы
    lv_obj_set_style_pad_all(card, 0, 0);
    
    // Добавляем объект в группу фокуса
    if (encoder_group != NULL) {
        lv_group_add_obj(encoder_group, card);
    }
    
    // Возвращаем указатель на созданную карточку
    return card;
}

// Функция для установки фокуса на элемент интерфейса
void lvgl_set_focus(int index)
{
    // Проверяем корректность индекса
    if (index < 0 || index >= total_focus_items) {
        ESP_LOGW(TAG, "Invalid focus index: %d", index);
        return;
    }
    
    // Снимаем фокус с предыдущего элемента
    if (current_focus_index >= 0 && current_focus_index < total_focus_items) {
        lv_obj_remove_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
    }
    
    // Устанавливаем фокус на новый элемент
    current_focus_index = index;
    lv_obj_add_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
    
    // Устанавливаем фокус в группе LVGL
    if (encoder_group != NULL) {
        lv_group_focus_obj(sensor_containers[current_focus_index]);
    }
    
    // Прокручиваем экран к элементу с фокусом (если необходимо)
    lv_obj_scroll_to_view_recursive(sensor_containers[current_focus_index], LV_ANIM_ON);
    
    ESP_LOGD(TAG, "Focus set to index: %d", index);
}

// Получение текущего индекса фокуса
int lvgl_get_focus_index(void)
{
    return current_focus_index;
}

// Получение общего количества элементов для фокуса
int lvgl_get_total_focus_items(void)
{
    return total_focus_items;
}

// Очистка группы фокуса
void lvgl_clear_focus_group(void)
{
    if (encoder_group != NULL) {
        lv_group_remove_all_objs(encoder_group);
    }
    
    // Сбрасываем индекс фокуса
    current_focus_index = 0;
}

// Создание основного пользовательского интерфейса
static void create_main_ui(void)
{
    // Получаем активный экран и очищаем его
    screen_main = lv_scr_act();
    lv_obj_clean(screen_main);
    
    // Создаем глобальную группу фокуса, если она еще не создана
    if (encoder_group == NULL) {
        encoder_group = lv_group_create();
        lv_group_set_wrap(encoder_group, true);  // Цикличный фокус
    }
    
    // Очищаем группу фокуса перед добавлением новых объектов
    lvgl_clear_focus_group();
    
    // Установка фона (уже в init_styles)
    init_styles();

    // Создание заголовка экрана
    label_title = lv_label_create(screen_main);
    lv_label_set_text(label_title, "Hydroponics");  // Текст заголовка
    lv_obj_add_style(label_title, &style_title, 0);  // Применяем стиль заголовка
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 8);  // Выравнивание по центру сверху

    // === СТРОКА 1: pH и EC ===
    // Создаем контейнер для отображения pH
    cont_ph = create_sensor_card(screen_main);
    lv_obj_align(cont_ph, LV_ALIGN_TOP_LEFT, 5, 40);  // Позиционируем слева сверху
    sensor_containers[0] = cont_ph;  // Добавляем в массив для навигации

    // Создаем метку для подписи pH
    lv_obj_t *ph_label = lv_label_create(cont_ph);
    lv_label_set_text(ph_label, "pH");  // Текст подписи
    lv_obj_add_style(ph_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(ph_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения pH
    label_ph_value = lv_label_create(cont_ph);
    lv_label_set_text(label_ph_value, "--");  // Начальное значение
    lv_obj_add_style(label_ph_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_ph_value, LV_ALIGN_CENTER, 0, -5);  // Центрируем с небольшим смещением

    // === СТРОКА 1: EC ===
    // Создаем контейнер для отображения EC
    cont_ec = create_sensor_card(screen_main);
    lv_obj_align_to(cont_ec, cont_ph, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);  // Позиционируем справа от pH
    sensor_containers[1] = cont_ec;  // Добавляем в массив для навигации

    // Создаем метку для подписи EC
    lv_obj_t *ec_label = lv_label_create(cont_ec);
    lv_label_set_text(ec_label, "EC");  // Текст подписи
    lv_obj_add_style(ec_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(ec_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения EC
    label_ec_value = lv_label_create(cont_ec);
    lv_label_set_text(label_ec_value, "--");  // Начальное значение
    lv_obj_add_style(label_ec_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_ec_value, LV_ALIGN_CENTER, 0, -5);  // Центрируем с небольшим смещением

    // === СТРОКА 2: Температура ===
    // Создаем контейнер для отображения температуры
    cont_climate = create_sensor_card(screen_main);
    lv_obj_align_to(cont_climate, cont_ph, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // Позиционируем под pH
    sensor_containers[2] = cont_climate;  // Добавляем в массив для навигации

    // Создаем метку для подписи температуры
    lv_obj_t *temp_label = lv_label_create(cont_climate);
    lv_label_set_text(temp_label, "Temp");  // Текст подписи
    lv_obj_add_style(temp_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения температуры
    label_temp_value = lv_label_create(cont_climate);
    lv_label_set_text(label_temp_value, "--");  // Начальное значение
    lv_obj_add_style(label_temp_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_temp_value, LV_ALIGN_CENTER, 0, -8);  // Центрируем с небольшим смещением

    // Создаем метку для единицы измерения температуры
    lv_obj_t *temp_unit = lv_label_create(cont_climate);
    lv_label_set_text(temp_unit, "°C");  // Единица измерения
    lv_obj_add_style(temp_unit, &style_unit, 0);  // Применяем стиль единицы измерения
    lv_obj_align_to(temp_unit, label_temp_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);  // Позиционируем под значением

    // === СТРОКА 2: Влажность ===
    // Создаем контейнер для отображения влажности (Новый контейнер для влажности)
    cont_hum = create_sensor_card(screen_main);
    lv_obj_align_to(cont_hum, cont_climate, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);  // Позиционируем справа от температуры
    sensor_containers[3] = cont_hum;  // Добавляем в массив для навигации

    // Создаем метку для подписи влажности
    lv_obj_t *hum_label = lv_label_create(cont_hum);
    lv_label_set_text(hum_label, "Hum");  // Текст подписи
    lv_obj_add_style(hum_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(hum_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения влажности
    label_hum_value = lv_label_create(cont_hum);
    lv_label_set_text(label_hum_value, "--");  // Начальное значение
    lv_obj_add_style(label_hum_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_hum_value, LV_ALIGN_CENTER, 0, -8);  // Центрируем с небольшим смещением

    // Создаем метку для единицы измерения влажности
    lv_obj_t *hum_unit = lv_label_create(cont_hum);
    lv_label_set_text(hum_unit, "%");  // Единица измерения
    lv_obj_add_style(hum_unit, &style_unit, 0);  // Применяем стиль единицы измерения
    lv_obj_align_to(hum_unit, label_hum_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);  // Позиционируем под значением

    // === СТРОКА 3: Освещение ===
    // Создаем контейнер для отображения освещенности
    cont_light = create_sensor_card(screen_main);
    lv_obj_align_to(cont_light, cont_climate, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // Позиционируем под температурой
    sensor_containers[4] = cont_light;  // Добавляем в массив для навигации

    // Создаем метку для подписи освещенности
    lv_obj_t *lux_label = lv_label_create(cont_light);
    lv_label_set_text(lux_label, "Light");  // Текст подписи
    lv_obj_add_style(lux_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(lux_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения освещенности
    label_lux_value = lv_label_create(cont_light);
    lv_label_set_text(label_lux_value, "--");  // Начальное значение
    lv_obj_add_style(label_lux_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_lux_value, LV_ALIGN_CENTER, 0, -5);  // Центрируем с небольшим смещением

    // Создаем метку для единицы измерения освещенности
    lv_obj_t *lux_unit = lv_label_create(cont_light);
    lv_label_set_text(lux_unit, "lux");  // Единица измерения
    lv_obj_add_style(lux_unit, &style_unit, 0);  // Применяем стиль единицы измерения
    lv_obj_align_to(lux_unit, label_lux_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);  // Позиционируем под значением

    // === СТРОКА 3: CO2 ===
    // Создаем контейнер для отображения CO2
    cont_air = create_sensor_card(screen_main);
    lv_obj_align_to(cont_air, cont_light, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);  // Позиционируем справа от освещенности
    sensor_containers[5] = cont_air;  // Добавляем в массив для навигации

    // Создаем метку для подписи CO2
    lv_obj_t *co2_label = lv_label_create(cont_air);
    lv_label_set_text(co2_label, "CO₂");  // Текст подписи
    lv_obj_add_style(co2_label, &style_label, 0);  // Применяем стиль подписи
    lv_obj_align(co2_label, LV_ALIGN_TOP_MID, 0, 0);  // Выравнивание по центру сверху

    // Создаем метку для значения CO2
    label_co2_value = lv_label_create(cont_air);
    lv_label_set_text(label_co2_value, "--");  // Начальное значение
    lv_obj_add_style(label_co2_value, &style_value, 0);  // Применяем стиль значения
    lv_obj_align(label_co2_value, LV_ALIGN_CENTER, 0, -5);  // Центрируем с небольшим смещением

    // Создаем метку для единицы измерения CO2
    lv_obj_t *co2_unit = lv_label_create(cont_air);
    lv_label_set_text(co2_unit, "ppm");  // Единица измерения
    lv_obj_add_style(co2_unit, &style_unit, 0);  // Применяем стиль единицы измерения
    lv_obj_align_to(co2_unit, label_co2_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);  // Позиционируем под значением

    // Создаем очередь для обновлений данных датчиков и задачу обработки
    sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
    // Увеличиваем приоритет задачи до 6 для уменьшения задержек в обработке input
    xTaskCreate(display_update_task, "display_update", 4096, NULL, 6, NULL);
    
    // Устанавливаем начальный фокус на первый элемент
    lvgl_set_focus(0);
}

// Создание экрана деталей
static void create_detail_ui(int index)
{
    LV_UNUSED(index);
    screen_detail = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_detail, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_detail, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen_detail);
    lv_obj_add_style(title, &style_title, 0);
    switch (index) {
        case 0: lv_label_set_text(title, "Details: pH"); break;
        case 1: lv_label_set_text(title, "Details: EC"); break;
        case 2: lv_label_set_text(title, "Details: Temp"); break;
        case 3: lv_label_set_text(title, "Details: Hum"); break;
        case 4: lv_label_set_text(title, "Details: Light"); break;
        case 5: lv_label_set_text(title, "Details: CO2"); break;
        default: lv_label_set_text(title, "Details"); break;
    }
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *hint = lv_label_create(screen_detail);
    lv_obj_add_style(hint, &style_label, 0);
    lv_label_set_text(hint, "Нажмите для выхода");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

bool lvgl_is_detail_screen_open(void)
{
    return screen_detail != NULL && lv_scr_act() == screen_detail;
}

void lvgl_open_detail_screen(int index)
{
    if (!lv_is_initialized()) return;
    if (lvgl_is_detail_screen_open()) {
        lvgl_close_detail_screen();
    }
    if (!lvgl_lock(1000)) return;
    create_detail_ui(index);
    lv_scr_load_anim(screen_detail, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    lvgl_unlock();
}

void lvgl_close_detail_screen(void)
{
    if (!lv_is_initialized()) return;
    if (!lvgl_lock(1000)) return;
    if (screen_detail) {
        lv_obj_del_async(screen_detail);
        screen_detail = NULL;
    }
    create_main_ui();
    lvgl_unlock();
}

// Обновление отображения датчиков с новыми значениями
static void update_sensor_display(sensor_data_t *data)
{
    // Буфер для форматирования строк
    char buffer[20];
    
    // Проверяем, что LVGL инициализирован и экран действителен
    if (!lv_is_initialized()) {
        ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
        return;
    }
    
    // Дополнительная проверка, что экран действителен
    if (!lv_scr_act()) {
        ESP_LOGW(TAG, "LVGL screen not active, skipping display update");
        return;
    }
    
    // Обновление значения pH
    if (label_ph_value) {
        // Форматируем значение pH с двумя знаками после запятой
        snprintf(buffer, sizeof(buffer), "%.2f", data->ph);
        // Устанавливаем текст метки значения pH
        lv_label_set_text(label_ph_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->ph < 5.5 || data->ph > 7.5) {
            // Если значение вне нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_ph_value, COLOR_DANGER, 0);
        } else if (data->ph < 6.0 || data->ph > 7.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_ph_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_ph_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление значения EC
    if (label_ec_value) {
        // Форматируем значение EC с двумя знаками после запятой
        snprintf(buffer, sizeof(buffer), "%.2f", data->ec);
        // Устанавливаем текст метки значения EC
        lv_label_set_text(label_ec_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->ec < 1.0 || data->ec > 2.5) {
            // Если значение вне нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_ec_value, COLOR_DANGER, 0);
        } else if (data->ec < 1.2 || data->ec > 2.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_ec_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_ec_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление температуры
    if (label_temp_value) {
        // Форматируем значение температуры с одним знаком после запятой
        snprintf(buffer, sizeof(buffer), "%.1f", data->temp);
        // Устанавливаем текст метки значения температуры
        lv_label_set_text(label_temp_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->temp < 18.0 || data->temp > 30.0) {
            // Если значение вне нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_temp_value, COLOR_DANGER, 0);
        } else if (data->temp < 20.0 || data->temp > 28.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_temp_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_temp_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление влажности
    if (label_hum_value) {
        // Форматируем значение влажности с одним знаком после запятой
        snprintf(buffer, sizeof(buffer), "%.1f", data->hum);
        // Устанавливаем текст метки значения влажности
        lv_label_set_text(label_hum_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->hum < 40.0 || data->hum > 80.0) {
            // Если значение вне нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_hum_value, COLOR_DANGER, 0);
        } else if (data->hum < 45.0 || data->hum > 75.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_hum_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_hum_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление освещенности
    if (label_lux_value) {
        // Форматируем значение освещенности без знаков после запятой
        snprintf(buffer, sizeof(buffer), "%.0f", data->lux);
        // Устанавливаем текст метки значения освещенности
        lv_label_set_text(label_lux_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->lux < 200.0 || data->lux > 2000.0) {
            // Если значение вне нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_lux_value, COLOR_DANGER, 0);
        } else if (data->lux < 400.0 || data->lux > 1500.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_lux_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_lux_value, COLOR_NORMAL, 0);
        }
    }
    
    // Обновление CO2
    if (label_co2_value) {
        // Форматируем значение CO2 без знаков после запятой
        snprintf(buffer, sizeof(buffer), "%.0f", data->co2);
        // Устанавливаем текст метки значения CO2
        lv_label_set_text(label_co2_value, buffer);
        // Динамическое изменение цвета в зависимости от значения
        if (data->co2 > 1200.0) {
            // Если значение выше нормы, устанавливаем красный цвет
            lv_obj_set_style_text_color(label_co2_value, COLOR_DANGER, 0);
        } else if (data->co2 > 800.0) {
            // Если значение в предупредительном диапазоне, устанавливаем оранжевый цвет
            lv_obj_set_style_text_color(label_co2_value, COLOR_WARNING, 0);
        } else {
            // Если значение в норме, устанавливаем зеленый цвет
            lv_obj_set_style_text_color(label_co2_value, COLOR_NORMAL, 0);
        }
    }
    
    // Принудительное обновление экрана
    lv_obj_invalidate(lv_scr_act());
}

// Задача обновления дисплея
// Обрабатывает данные датчиков из очереди и обновляет отображение на экране
static void display_update_task(void *pvParameters)
{
    // Структура для хранения данных датчиков
    sensor_data_t sensor_data;
    
    // Бесконечный цикл задачи
    while (1) {
        // Ожидание данных датчиков из очереди с таймаутом 1 секунда
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Попытка блокировки мьютекса, так как API LVGL не являются потокобезопасными
            if (!lvgl_lock(100)) {  // Увеличен таймаут до 100 мс
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
        
        // Небольшая задержка для предотвращения чрезмерного использования CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Инициализация пользовательского интерфейса LVGL
void lvgl_main_init(void)
{
    // Обеспечиваем правильную инициализацию LVGL и готовность системы
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Убеждаемся, что можем получить блокировку LVGL перед созданием UI
    if (lvgl_lock(1000)) {  // Ожидаем до 1 секунды для получения блокировки
        // Создаем основной пользовательский интерфейс
        create_main_ui();
        // Освобождаем блокировку
        lvgl_unlock();
    } else {
        // Если не удалось получить блокировку LVGL, выводим ошибку
        ESP_LOGE("LVGL_MAIN", "Failed to acquire LVGL lock for UI initialization");
    }
}

// Обновление значений датчиков на экране
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // Проверка инициализации очереди
    if (sensor_data_queue == NULL) {
        // Если очередь не инициализирована, выходим из функции
        return;
    }
    
    // Создание структуры данных датчиков с переданными значениями
    sensor_data_t sensor_data = {
        .ph = ph,      // Значение pH
        .ec = ec,      // Значение EC
        .temp = temp,  // Значение температуры
        .hum = hum,    // Значение влажности
        .lux = lux,    // Значение освещенности
        .co2 = co2     // Значение CO2
    };
    
    // Отправка данных датчиков в очередь (неблокирующая)
    // Отправляем только если есть место в очереди
    if (uxQueueSpacesAvailable(sensor_data_queue) > 0) {
        // Помещаем данные в очередь без ожидания
        xQueueSend(sensor_data_queue, &sensor_data, 0);
    }
}