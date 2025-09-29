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
#define LVGL_TASK_MAX_DELAY_MS 500
// Минимальная задержка задачи LVGL в миллисекундах
#define LVGL_TASK_MIN_DELAY_MS 1
// Размер стека задачи LVGL (8 КБ)
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
// Приоритет задачи LVGL
#define LVGL_TASK_PRIORITY     2

// Мьютекс для обеспечения потокобезопасности при работе с LVGL
static SemaphoreHandle_t lvgl_mux = NULL;
// Дескриптор задачи LVGL для отладки
static TaskHandle_t lvgl_task_handle = NULL;

// Предварительные объявления функций
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_port_update_callback(lv_disp_drv_t *drv);
static void increase_lvgl_tick(void *arg);
static void lvgl_task_handler(void *pvParameters);

// Блокировка мьютекса LVGL
// Используется для обеспечения потокобезопасности при работе с API LVGL
bool lvgl_lock(int timeout_ms)
{
    // Преобразование таймаута из миллисекунд в тики FreeRTOS
    // Если `timeout_ms` равен -1, программа будет блокироваться до выполнения условия
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

// Разблокировка мьютекса LVGL
// Освобождает мьютекс после завершения работы с API LVGL
void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

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
                ESP_LOGD("LCD", "LVGL timer handler executed, next delay: %d ms", task_delay_ms);
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
        ESP_LOGD("LCD", "LVGL task delaying for %d ms", task_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// Добавляем глобальную переменную для драйвера энкодера
static lv_indev_drv_t encoder_indev_drv;
static lv_indev_t *encoder_indev = NULL;

// Добавляем функцию обратного вызова для чтения энкодера
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_key = 0;
    static bool button_pressed = false;
    
    // Initialize the data structure
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
    data->enc_diff = 0;
    
    // Получаем очередь событий энкодера
    QueueHandle_t encoder_queue = encoder_get_event_queue();
    if (encoder_queue == NULL) {
        return;
    }
    
    // Читаем события из очереди (неблокирующее чтение)
    encoder_event_t event;
    while (xQueueReceive(encoder_queue, &event, 0) == pdTRUE) {
        switch (event.type) {
            case ENCODER_EVENT_ROTATE_CW:
                // For encoder, we accumulate the difference
                data->enc_diff += event.value;
                break;
            case ENCODER_EVENT_ROTATE_CCW:
                // For counter-clockwise rotation, we use negative values
                data->enc_diff -= event.value;
                break;
            case ENCODER_EVENT_BUTTON_PRESS:
                button_pressed = true;
                data->state = LV_INDEV_STATE_PRESSED;
                last_key = LV_KEY_ENTER;
                data->key = last_key;
                break;
            case ENCODER_EVENT_BUTTON_LONG_PRESS:
                // For long press, we can use a different key or the same
                button_pressed = true;
                data->state = LV_INDEV_STATE_PRESSED;
                last_key = LV_KEY_ENTER;
                data->key = last_key;
                break;
            case ENCODER_EVENT_BUTTON_RELEASE:
                button_pressed = false;
                data->state = LV_INDEV_STATE_RELEASED;
                // Don't reset the key here, let it persist for one cycle
                break;
        }
    }
    
    // If no new events, maintain the last state for one cycle after button release
    if (!button_pressed && last_key != 0) {
        // Keep the key for one more cycle after release for LVGL to process
        data->key = last_key;
        last_key = 0; // Reset for next cycle
    } else if (button_pressed) {
        // Keep the key while button is pressed
        data->key = last_key;
    }
}

// Инициализация дисплея LCD ILI9341
// Настраивает SPI, инициализирует панель и регистрирует драйвер дисплея для LVGL
lv_disp_t* lcd_ili9341_init(void)
{
    // Статические буферы для графики LVGL
    static lv_disp_draw_buf_t disp_buf;
    // Структура драйвера дисплея LVGL
    static lv_disp_drv_t disp_drv;
    
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
        .max_transfer_sz = LCD_H_RES * 20 * sizeof(uint16_t), // Максимальный размер передачи данных
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

    // Дескриптор ввода-вывода панели дисплея
    esp_lcd_panel_io_handle_t io_handle = NULL;
    // Конфигурация ввода-вывода панели через SPI
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,      // Пин выбора команды/данных
        .cs_gpio_num = PIN_NUM_LCD_CS,      // Пин выбора микросхемы
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,      // Частота пиксельных часов
        .lcd_cmd_bits = 8,                  // Количество бит для команды
        .lcd_param_bits = 8,                // Количество бит для параметра
        .spi_mode = 0,                      // Режим SPI (0 или 3)
        .trans_queue_depth = 10,            // Глубина очереди передач
        .on_color_trans_done = notify_lvgl_flush_ready, // Callback по завершению передачи
        .user_ctx = &disp_drv,              // Пользовательский контекст
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
    // Настройка ориентации панели:
    // Отключаем обмен координат X и Y
    esp_lcd_panel_swap_xy(panel_handle, false);
    // Отражаем панель по вертикали (стандарт для портретного режима 240x320)
    esp_lcd_panel_mirror(panel_handle, false, true);

    // Пользователь может вывести предопределенный шаблон на экран перед включением экрана или подсветки
    // Выводим информацию о включении дисплея
    ESP_LOGI("LCD", "Turning on display");
    // Включаем дисплей
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Включаем подсветку LCD
    // Выводим информацию о включении подсветки
    ESP_LOGI("LCD", "Turn on LCD backlight");
    // Устанавливаем уровень сигнала для включения подсветки
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
    
    // Выводим информацию об инициализации библиотеки LVGL
    ESP_LOGI("LCD", "Initialize LVGL library");
    // Инициализируем библиотеку LVGL
    lv_init();
    
    // Выделяем буферы для отрисовки, используемые LVGL
    // Рекомендуется выбрать размер буфера(ов) не менее 1/10 размера экрана
    lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf1 == NULL) {
        // Если не удалось выделить память для первого буфера, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to allocate draw buffer 1");
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }
    
    // Выделяем память для второго буфера отрисовки
    lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf2 == NULL) {
        // Если не удалось выделить память для второго буфера, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to allocate draw buffer 2");
        heap_caps_free(buf1);
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }
    
    // Инициализируем буферы отрисовки LVGL
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * 20);

    // Регистрируем драйвер дисплея в LVGL
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;               // Горизонтальное разрешение
    disp_drv.ver_res = LCD_V_RES;               // Вертикальное разрешение
    disp_drv.antialiasing = 1;                  // Включаем сглаживание
    disp_drv.flush_cb = lvgl_flush_cb;          // Callback функция обновления дисплея
    disp_drv.drv_update_cb = lvgl_port_update_callback; // Callback функция обновления порта
    disp_drv.draw_buf = &disp_buf;              // Буферы отрисовки
    disp_drv.user_data = panel_handle;          // Пользовательские данные (дескриптор панели)

    // Включаем поддержку поворота экрана
    disp_drv.sw_rotate = 1;                     // Программный поворот
    disp_drv.rotated = LV_DISP_ROT_NONE;        // Начальная ориентация (без поворота)
    
    // Улучшаем качество отображения текста
    disp_drv.full_refresh = 0;  // Частичное обновление для лучшей производительности
    disp_drv.direct_mode = 0;   // Используем буферный режим для лучшего качества текста
    
    // Регистрируем драйвер дисплея в LVGL
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL) {
        // Если регистрация драйвера дисплея не удалась, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to register display driver");
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }
    
    // Регистрируем энкодер как устройство ввода
    lv_indev_drv_init(&encoder_indev_drv);
    encoder_indev_drv.type = LV_INDEV_TYPE_ENCODER;
    encoder_indev_drv.read_cb = encoder_read;
    encoder_indev_drv.user_data = NULL;
    encoder_indev = lv_indev_drv_register(&encoder_indev_drv);
    
    // Выводим информацию об установке таймера LVGL
    ESP_LOGI("LCD", "Install LVGL tick timer");
    // Интерфейс таймера для LVGL (используем esp_timer для генерации периодического события каждые 2 мс)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,  // Callback функция
        .name = "lvgl_tick"               // Имя таймера
    };
    // Дескриптор таймера LVGL
    esp_timer_handle_t lvgl_tick_timer = NULL;
    // Создаем таймер LVGL
    ret = esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    if (ret != ESP_OK) {
        // Если создание таймера не удалось, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to create LVGL tick timer: %s", esp_err_to_name(ret));
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }
    
    // Запускаем периодический таймер LVGL
    ret = esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    if (ret != ESP_OK) {
        // Если запуск таймера не удался, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to start LVGL tick timer: %s", esp_err_to_name(ret));
        esp_timer_delete(lvgl_tick_timer);
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    }
    
    // Небольшая задержка для обеспечения полной регистрации драйвера дисплея
    vTaskDelay(pdMS_TO_TICKS(10));
    // Выводим информацию о создании задачи LVGL
    ESP_LOGI("LCD", "Create LVGL task");
    // Создаем задачу LVGL

    // Результат создания задачи
    BaseType_t task_result = xTaskCreate(lvgl_task_handler, "lvgl_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle);
    if (task_result != pdPASS) {
        // Если создание задачи не удалось, выводим ошибку и освобождаем ресурсы
        ESP_LOGE("LCD", "Failed to create LVGL task");
        esp_timer_delete(lvgl_tick_timer);
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        // Освобождаем ранее выделенные ресурсы
        // Примечание: В реальной реализации может потребоваться деинициализация panel_handle и io_handle
        spi_bus_free(LCD_HOST);
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = NULL;
        return NULL;
    } 
    
    // Возвращаем указатель на драйвер дисплея
    return disp;
}

// Установка яркости дисплея
// Принимает значение яркости от 0 до 100
void lcd_ili9341_set_brightness(uint8_t brightness)
{
    // Ограничиваем значение яркости диапазоном 0-100
    if (brightness > 100) brightness = 100;
    
    // Для простого управления вкл/выкл просто включаем/выключаем подсветку
    // Для управления ШИМ необходимо реализовать ШИМ на пине подсветки
    gpio_set_level(PIN_NUM_BK_LIGHT, brightness > 0 ? LCD_BK_LIGHT_ON_LEVEL : LCD_BK_LIGHT_OFF_LEVEL);
}

// Callback функции
// Уведомляет о готовности обновления экрана
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // Получаем указатель на драйвер дисплея из пользовательского контекста
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    // Сообщаем LVGL, что обновление дисплея завершено
    lv_disp_flush_ready(disp_driver);
    // Возвращаем false, чтобы не продолжать обработку события
    return false;
}

// Callback функция обновления экрана
// Рисует битмап в указанной области дисплея
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    // Проверка на нулевые указатели
    if (!drv || !area || !color_map) {
        // Если есть нулевые указатели, выводим ошибку и выходим
        ESP_LOGE("LCD", "Null pointer in lvgl_flush_cb");
        return;
    }
    
    // Получаем дескриптор панели из пользовательских данных драйвера
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    
    // Проверка на корректность дескриптора панели
    if (!panel_handle) {
        // Если дескриптор панели некорректный, выводим ошибку и выходим
        ESP_LOGE("LCD", "Invalid panel handle in lvgl_flush_cb");
        return;
    }
    
    // Получаем координаты области для обновления
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    // Проверка границ координат
    if (offsetx1 < 0 || offsety1 < 0 || offsetx2 >= LCD_H_RES || offsety2 >= LCD_V_RES) {
        // Если координаты выходят за пределы экрана, выводим предупреждение и выходим
        ESP_LOGW("LCD", "Invalid area coordinates: (%d,%d) to (%d,%d)", offsetx1, offsety1, offsetx2, offsety2);
        return;
    }
    
    // Логируем операции обновления для отладки (только для больших областей, чтобы избежать спама)
    int area_size = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);
    if (area_size > 1000) {  // Логируем только для областей больше 1000 пикселей
        ESP_LOGD("LCD", "Flush area: (%d,%d) to (%d,%d), size: %d pixels", 
                 offsetx1, offsety1, offsetx2, offsety2, area_size);
    }
    
    // Копируем содержимое буфера в указанную область дисплея
    esp_err_t result = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    
    // Принудительное обновление всего экрана для лучшей четкости текста
    if (area_size > LCD_H_RES * LCD_V_RES * 0.8) {  // Если область больше 80% экрана
        ESP_LOGD("LCD", "Forcing full screen refresh for better text consistency");
    }
    
    // Проверяем результат операции рисования
    if (result != ESP_OK) {
        // Если операция рисования не удалась, выводим предупреждение
        ESP_LOGW("LCD", "Failed to draw bitmap: %s", esp_err_to_name(result));
    }
    
    // НЕ ВЫЗЫВАЙТЕ lv_disp_flush_ready здесь!
    // Это сделает колбэк notify_lvgl_flush_ready после завершения DMA.
    // lv_disp_flush_ready(drv);  // Удалено согласно рекомендациям
}

// Callback функция обновления порта
// Обрабатывает обновление данных сенсоров
static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    // Проверка на нулевой указатель
    if (!drv) {
        // Если указатель нулевой, выводим ошибку и выходим
        ESP_LOGE("LCD", "Null pointer in lvgl_port_update_callback");
        return;
    }
    
    // Инвалидируем экран для обновления отображения
    if (lv_is_initialized() && lv_scr_act()) {
        lv_obj_invalidate(lv_scr_act());
    }
}

// Увеличение тиков LVGL
// Сообщает LVGL, сколько миллисекунд прошло
static void increase_lvgl_tick(void *arg)
{
    /* Сообщаем LVGL, сколько миллисекунд прошло */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}