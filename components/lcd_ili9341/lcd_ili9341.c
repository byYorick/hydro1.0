/*
 * Реализация драйвера LCD ILI9341
 * 
 * Этот файл содержит реализацию драйвера для работы с дисплеем ILI9341
 * через интерфейс SPI с использованием библиотеки LVGL для отображения графики.
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_ili9341.h"
#include "lvgl.h"

#include "lcd_ili9341.h"
// Используем правильный путь к заголовочному файлу lvgl_main
#include "../lvgl_main/lvgl_main.h"
#include "../encoder/encoder.h"

// Используем SPI2 для подключения дисплея
#define LCD_HOST  SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Конфигурация LCD //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Частота пиксельных часов дисплея (20 МГц)
#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
// Уровень сигнала для включения подсветки (1 - высокий уровень, 0 - низкий уровень)
#define LCD_BK_LIGHT_ON_LEVEL  1
// Уровень сигнала для выключения подсветки (инверсия от уровня включения)
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
// Номер пина SPI для тактирования (SCLK)
#define PIN_NUM_SCLK     12
// Номер пина SPI для данных (MOSI)
#define PIN_NUM_MOSI     11
// Номер пина SPI для входа (MISO), -1 если не используется
#define PIN_NUM_MISO    -1
// Номер пина для выбора команды/данных (DC)
#define PIN_NUM_LCD_DC   9
// Номер пина сброса (RST)
#define PIN_NUM_LCD_RST  14
// Номер пина выбора микросхемы (CS)
#define PIN_NUM_LCD_CS   10
// Номер пина управления подсветкой
#define PIN_NUM_BK_LIGHT 15

// Горизонтальное разрешение дисплея (в пикселях)
#define LCD_H_RES              240
// Вертикальное разрешение дисплея (в пикселях)
#define LCD_V_RES              320

// Период таймера LVGL в миллисекундах (2 мс)
#define LVGL_TICK_PERIOD_MS    2
// Максимальная задержка задачи LVGL в миллисекундах
#define LVGL_TASK_MAX_DELAY_MS 40  // ensure LVGL handler runs at least 25 times per second
// Минимальная задержка задачи LVGL в миллисекундах
#define LVGL_TASK_MIN_DELAY_MS 1
// Размер стека задачи LVGL (20 КБ)
#define LVGL_TASK_STACK_SIZE   (20 * 1024)
// Приоритет задачи LVGL
#define LVGL_TASK_PRIORITY     2

// Мьютекс для обеспечения потокобезопасности при работе с LVGL
static SemaphoreHandle_t lvgl_mux = NULL;
// Дескриптор задачи LVGL для отладки
static TaskHandle_t lvgl_task_handle = NULL;

// Предварительные объявления функций
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map);
static void lvgl_port_update_callback(lv_display_t *disp);
static void increase_lvgl_tick(void *arg);
static void lvgl_port_update_callback(lv_display_t *disp)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    // В LVGL 9.x ротация обрабатывается по-другому
    // Пока оставляем базовую настройку
    esp_lcd_panel_swap_xy(panel_handle, false);
    esp_lcd_panel_mirror(panel_handle, true, false);
}
bool lvgl_lock(int timeout_ms)
{
    if (!lvgl_mux) {
        return false;
    }
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    if (lvgl_mux) {
        xSemaphoreGiveRecursive(lvgl_mux);
    }
}












// Разблокировка мьютекса LVGL
// Освобождает мьютекс после завершения работы с API LVGL


// Устаревшая функция для обновления значений датчиков
// Пользовательский интерфейс теперь обрабатывается компонентом lvgl_main
void lcd_ili9341_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // Эта функция теперь устарела, так как UI обрабатывается компонентом lvgl_main
    // Сохранена для обратной совместимости
}

// Обработчик задачи LVGL
// Выполняет обработку таймеров LVGL и управляет обновлениями дисплея
static void lvgl_task_handler(void *pvParameters)
{
    // Начальная задержка задачи
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    
    ESP_LOGD("LCD", "LVGL task handler started");
    
    while (1) {
        // Блокировка мьютекса, так как API LVGL не являются потокобезопасными
        if (lvgl_lock(-1)) {  // Ожидать бесконечно для получения блокировки
            // Дополнительная проверка, что LVGL инициализирован
            if (lv_is_initialized()) {
                // Обработка таймеров LVGL и получение времени до следующего события
                task_delay_ms = lv_timer_handler();
            } else {
                // Если LVGL не инициализирован, выводим предупреждение и устанавливаем максимальную задержку
                ESP_LOGW("LCD", "LVGL not initialized, skipping timer handler");
                task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
            }
            // Освобождение мьютекса
            lvgl_unlock();
        } else {
            // Если не удалось получить блокировку мьютекса, продолжаем попытки
            ESP_LOGW("LCD", "Failed to acquire LVGL lock, retrying");
        }
        
        // Ограничение значения задержки в допустимом диапазоне
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        // Задержка задачи на указанное количество миллисекунд
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// Добавляем глобальную переменную для энкодера LVGL 9.x
static lv_indev_t *encoder_indev = NULL;

// Заглушка для регистрации encoder input device в LVGL
// ВАЖНО: Фактическая обработка энкодера выполняется в encoder_task (lvgl_main.c)
// для реализации кастомной навигации по UI
static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    // Initialize the data structure
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
    data->enc_diff = 0;
    
    // Обработка событий энкодера выполняется в encoder_task (lvgl_main.c)
    // который читает из encoder_get_event_queue() и вызывает handle_encoder_event()
}

// Инициализация дисплея LCD ILI9341
// Настраивает SPI, инициализирует панель и регистрирует драйвер дисплея для LVGL
lv_display_t* lcd_ili9341_init(void)
{
    // Статические буферы для графики LVGL 9.x
    static lv_display_t *disp;
    static lv_color_t disp_buf1[LCD_H_RES * 60];
    static lv_color_t disp_buf2[LCD_H_RES * 60];
    
    // Выводим информацию о начале инициализации дисплея
    ESP_LOGI("LCD", "Initializing LCD ILI9341 display");

    // Настройка пина управления подсветкой как выход
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    // Применяем конфигурацию пина
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // Создаем мьютекс для LVGL
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mux == NULL) {
        // Если не удалось создать мьютекс, выводим ошибку и возвращаем NULL
        ESP_LOGE("LCD", "Failed to create LVGL mutex");
        return NULL;
    }

    // Конфигурация шины SPI для подключения дисплея
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,        // Пин тактирования SPI
        .mosi_io_num = PIN_NUM_MOSI,        // Пин данных SPI (MOSI)
        .miso_io_num = PIN_NUM_MISO,        // Пин данных SPI (MISO), -1 если не используется
        .quadwp_io_num = -1,                // Не используется в данном проекте
        .quadhd_io_num = -1,                // Не используется в данном проекте
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t), // Максимальный размер передачи данных
    };
    // Выводим информацию о начале инициализации шины SPI
    ESP_LOGI("LCD", "Initializing SPI bus");

    // Инициализируем шину SPI
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        // Если инициализация шины SPI не удалась, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }

    // Дескриптор ввода-вывода панели дисплея (статический чтобы использовать позже)
    static esp_lcd_panel_io_handle_t io_handle = NULL;
    // Конфигурация ввода-вывода панели через SPI
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,      // Пин выбора команды/данных
        .cs_gpio_num = PIN_NUM_LCD_CS,      // Пин выбора микросхемы
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,      // Частота пиксельных часов
        .lcd_cmd_bits = 8,                  // Количество бит для команды
        .lcd_param_bits = 8,                // Количество бит для параметра
        .spi_mode = 0,                      // Режим SPI (0 или 3)
        .trans_queue_depth = 10,            // Глубина очереди передач
        // Callback будет зарегистрирован ПОСЛЕ создания LVGL дисплея
    };
    // Выводим информацию о создании дескриптора ввода-вывода
    ESP_LOGI("LCD", "Creating panel IO handle");

    // Создаем дескриптор ввода-вывода панели через SPI
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        // Если создание дескриптора ввода-вывода не удалось, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to create panel IO: %s", esp_err_to_name(ret));
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }

    // Дескриптор панели дисплея
    esp_lcd_panel_handle_t panel_handle = NULL;
    // Конфигурация устройства панели дисплея
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,          // Пин сброса
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, // Порядок цветовых элементов (BGR для ILI9341)
        .bits_per_pixel = 16,                       // Количество бит на пиксель
    };
    // Выводим информацию о создании дескриптора панели
    ESP_LOGI("LCD", "Creating panel handle");

    // Выводим информацию об установке драйвера панели ILI9341
    ESP_LOGI("LCD", "Install ILI9341 panel driver");
    // Создаем дескриптор панели ILI9341
    ret = esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        // Если создание дескриптора панели не удалось, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to create panel: %s", esp_err_to_name(ret));
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }

    // Выводим информацию о сбросе панели
    ESP_LOGI("LCD", "Resetting panel");
    // Сбрасываем панель дисплея
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Выводим информацию об инициализации панели
    ESP_LOGI("LCD", "Initializing panel");
    // Инициализируем панель дисплея
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Выводим информацию о настройке ориентации панели
    ESP_LOGI("LCD", "Configuring panel orientation");
    // Базовая ориентация экрана (повернут по горизонтали под монтаж нашей платы)
    esp_lcd_panel_swap_xy(panel_handle, false);
    esp_lcd_panel_mirror(panel_handle, true, false);

    // Пользователь может вывести предопределенный шаблон на экран перед включением экрана или подсветки
    // Выводим информацию о включении дисплея
    ESP_LOGI("LCD", "Turning on display");
    // Включаем дисплей
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Включаем подсветку LCD
    // Выводим информацию о включении подсветки
    ESP_LOGI("LCD", "Turn on LCD backlight");
    // Устанавливаем оптимальную яркость для лучшего качества отображения
    lcd_ili9341_set_brightness(80);
    
    // Выводим информацию об инициализации библиотеки LVGL
    ESP_LOGI("LCD", "Initialize LVGL library");
    // Инициализируем библиотеку LVGL с размером буфера, достаточным для четверти экрана
    lv_init();
    // Буферы уже объявлены выше как статические массивы
    // Создаем дисплей в LVGL 9.x
    ESP_LOGI("LCD", "Creating LVGL display");
    disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    
    // Создаем буферы для LVGL 9.x
    static lv_draw_buf_t draw_buf1;
    static lv_draw_buf_t draw_buf2;
    lv_draw_buf_init(&draw_buf1, LCD_H_RES, 60, LV_COLOR_FORMAT_RGB565, LCD_H_RES * sizeof(lv_color_t), disp_buf1, LCD_H_RES * 60 * sizeof(lv_color_t));
    lv_draw_buf_init(&draw_buf2, LCD_H_RES, 60, LV_COLOR_FORMAT_RGB565, LCD_H_RES * sizeof(lv_color_t), disp_buf2, LCD_H_RES * 60 * sizeof(lv_color_t));
    lv_display_set_draw_buffers(disp, &draw_buf1, &draw_buf2);
    
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_driver_data(disp, panel_handle);
    
    // Устанавливаем ротацию
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
    
    // Выводим информацию о создании таймера LVGL
    ESP_LOGI("LCD", "Install LVGL tick timer");
    // Создаем периодический таймер для LVGL с периодом LVGL_TICK_PERIOD_MS миллисекунд
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,        // Функция обратного вызова таймера
        .name = "lvgl_tick"                     // Имя таймера
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    // Создаем таймер
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    // Запускаем таймер в режиме повторения с периодом LVGL_TICK_PERIOD_MS миллисекунд
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // Регистрируем callback для уведомления LVGL о завершении передачи
    ESP_LOGI("LCD", "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp));

    // Выводим информацию о создании задачи обработчика LVGL
    ESP_LOGI("LCD", "Create LVGL task");
    // Создаем задачу обработчика LVGL с параметрами:
    // - Имя задачи: "LVGL"
    // - Размер стека: LVGL_TASK_STACK_SIZE байт
    // - Параметры задачи: NULL
    // - Приоритет: LVGL_TASK_PRIORITY
    // - Дескриптор задачи: &lvgl_task_handle
    xTaskCreate(lvgl_task_handler, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle);
    
    // Инициализация энкодера как устройства ввода LVGL 9.x
    ESP_LOGI("LCD", "Initialize encoder as LVGL input device");
    lv_indev_t *encoder_indev = lv_indev_create();
    lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(encoder_indev, encoder_read);
    
    // Примечание: группа будет установлена для энкодера в lvgl_main_init
    
    // Дополнительные настройки для улучшения качества отображения
    ESP_LOGI("LCD", "Applying display quality optimizations");
    
    // Настройки качества отображения применяются через конфигурацию LVGL
    // DPI и антиалиасинг настраиваются в sdkconfig
    
    // Дополнительные настройки для улучшения четкости текста
    // Антиалиасинг настраивается через конфигурацию LVGL
    
    // Выводим информацию об успешной инициализации дисплея
    ESP_LOGI("LCD", "LCD ILI9341 display initialized successfully");
    // Возвращаем дескриптор дисплея
    return disp;
}

// Функция обратного вызова при завершении передачи данных на дисплей
// Уведомляет LVGL о готовности к следующей передаче данных
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // Получаем драйвер дисплея из пользовательского контекста
    lv_display_t *disp = (lv_display_t *)user_ctx;
    // Уведомляем LVGL о завершении передачи данных
    lv_display_flush_ready(disp);
    // Возвращаем true для подтверждения обработки события
    return true;
}

// Функция обратного вызова для обновления дисплея LVGL
// Отправляет данные пикселей на дисплей через SPI
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    // Получаем дескриптур панели из пользовательских данных дисплея
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(disp);

    // Преобразуем координаты области в формат, понятный драйверу дисплея
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    // SPI LCD использует big-endian, поэтому меняем порядок байтов RGB
    lv_draw_sw_rgb565_swap(color_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    
    // Отправляем команду установки окна отображения на дисплее
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    
    ESP_LOGD("LCD", "Flushed area: (%d,%d) to (%d,%d)", offsetx1, offsety1, offsetx2, offsety2);
}

// Функция обратного вызова для обновления дисплея
// Вызывается при каждом обновлении дисплея для синхронизации ориентации


// Функция обратного вызова таймера LVGL
// Увеличивает внутренний таймер LVGL для обработки анимаций и других временных событий
static void increase_lvgl_tick(void *arg)
{
    // Увеличиваем внутренний таймер LVGL
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// Функция установки яркости дисплея
void lcd_ili9341_set_brightness(uint8_t brightness)
{
    // Ограничиваем значение яркости в диапазоне 0-100
    if (brightness > 100) {
        brightness = 100;
    }
    
    // Преобразуем процент в PWM значение (0-255)
    uint32_t pwm_value = (brightness * 255) / 100;
    
    // Устанавливаем уровень сигнала для подсветки
    // Для простоты используем цифровой выход, но можно заменить на PWM
    gpio_set_level(PIN_NUM_BK_LIGHT, (pwm_value > 127) ? LCD_BK_LIGHT_ON_LEVEL : LCD_BK_LIGHT_OFF_LEVEL);
    
    ESP_LOGI("LCD", "Display brightness set to %d%%", brightness);
}

// Функция получения устройства ввода энкодера
lv_indev_t* lcd_ili9341_get_encoder_indev(void)
{
    return encoder_indev;
}
