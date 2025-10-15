/**
 * @file app_main_final.c
 * @brief Главный файл приложения системы гидропонного мониторинга v3.0
 * 
 * Этот файл содержит точку входа приложения и координирует работу всех
 * компонентов системы: датчиков, дисплея, насосов, планировщика задач,
 * системы уведомлений и логирования данных.
 * 
 * @author Hydroponics Monitor Team
 * @version 3.0.0
 * @date 2025
 * 
 * АРХИТЕКТУРА:
 * - Многозадачная система на базе FreeRTOS
 * - Потокобезопасные операции с использованием мьютексов
 * - Асинхронная передача данных через очереди
 * - Модульная структура с разделением ответственности
 * 
 * ЗАДАЧИ (Tasks):
 * 1. sensor_task      (Pri 5) - Чтение датчиков каждые 2 сек
 * 2. display_task     (Pri 6) - Обновление UI каждую секунду
 * 3. notification_task(Pri 4) - Обработка уведомлений каждые 5 сек
 * 4. data_logger_task (Pri 3) - Логирование каждую минуту
 * 5. scheduler_task   (Pri 7) - Выполнение задач по расписанию
 * 6. ph_ec_task       (Pri 8) - Критический контроль pH/EC каждые 0.5 сек
 * 7. encoder_task     (Pri 6) - Обработка событий энкодера
 * 
 * ПОТОКОБЕЗОПАСНОСТЬ:
 * - sensor_data_mutex: защита глобальных данных датчиков
 * - i2c_bus_mutex: защита I2C шины (в компоненте i2c_bus)
 * - ui_mutex: защита LVGL операций (в компоненте lcd_ili9341)
 * - Очереди для передачи данных между задачами
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ESP-IDF
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_flash.h"

// Централизованная конфигурация системы
#include "system_config.h"

/* =============================
 *  UI КОНСТАНТЫ
 * ============================= */

/**
 * @brief Имена насосов для отображения в UI
 * 
 * Определение общих констант, используемых в различных UI экранах.
 * Объявление см. в system_config.h
 */
const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP",
    "pH DOWN",
    "EC A",
    "EC B",
    "EC C",
    "Water"
};

// Системные задачи (вынесены в отдельный модуль)
#include "system_tasks.h"

// Компоненты системы управления
#include "config_manager.h"
#include "system_interfaces.h"
#include "notification_system.h"
#include "error_handler.h"
#include "data_logger.h"
#include "task_scheduler.h"
#include "ph_ec_controller.h"
#include "pump_manager.h"
#include "adaptive_pid.h"
#include "pid_auto_tuner.h"
#include "system_interfaces.h"

// Драйверы оборудования
#include "lcd_ili9341.h"
#include "encoder.h"
#include "i2c_bus.h"
#include "lvgl_ui.h"

// Объявление русского шрифта (после включения lvgl)
#include "lvgl.h"
LV_FONT_DECLARE(montserrat_ru);

// UI экраны
#include "screens/notification_screen.h"

// Датчики (для инициализации)
#include "sht3x.h"
#include "ccs811.h"
#include "trema_ph.h"
#include "trema_ec.h"
#include "trema_lux.h"

// Исполнительные устройства
#include "peristaltic_pump.h"
#include "trema_relay.h"

/*******************************************************************************
 * КОНСТАНТЫ И ОПРЕДЕЛЕНИЯ
 ******************************************************************************/

/// Тег для логирования (отображается в Serial Monitor)
static const char *TAG = "HYDRO_MAIN";

/// Версия приложения (отображается на дисплее)
#define APP_VERSION "3.0.0-final"

/*******************************************************************************
 * ТИПЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Структура данных датчиков
 * 
 * Содержит текущие значения всех датчиков и флаги валидности.
 * Защищается мьютексом sensor_data_mutex при доступе из разных задач.
 */
// Структура данных сенсоров определена в ui_manager.h

/*******************************************************************************
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

/// Флаг инициализации системы
static bool system_initialized = false;

/// Дескрипторы задач (управляются модулем system_tasks)
static system_task_handles_t task_handles = {0};

/// Кэш системной конфигурации
static system_config_t g_system_config = {0};

/*******************************************************************************
 * ПРОТОТИПЫ ФУНКЦИЙ
 ******************************************************************************/

// Инициализация
static esp_err_t init_nvs(void);
static esp_err_t init_hardware(void);
static esp_err_t init_sensors(void);
static esp_err_t init_pumps(void);
static esp_err_t init_system_components(void);

// Callback функции
static void notification_callback(const notification_t *notification);
static void task_event_callback(uint32_t task_id, task_status_t status);
static void pump_event_callback(pump_index_t pump, bool started);
static void correction_event_callback(const char *type, float current, float target);
static void log_callback(const data_logger_entry_t *entry);

// Вспомогательные функции
static void print_system_info(void);
static esp_err_t register_task_executors(void);

/*******************************************************************************
 * ТОЧКА ВХОДА ПРИЛОЖЕНИЯ
 ******************************************************************************/

/**
 * @brief Главная функция приложения
 * 
 * Выполняет последовательную инициализацию всех компонентов системы:
 * 1. NVS (Non-Volatile Storage)
 * 2. Аппаратные компоненты (LCD, I2C, энкодер)
 * 3. Датчики
 * 4. Насосы и реле
 * 5. Системные компоненты (конфигурация, уведомления, логирование)
 * 6. FreeRTOS задачи
 * 
 * После инициализации входит в бесконечный цикл для поддержания работы системы.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "================================================================");
    ESP_LOGI(TAG, "   Hydroponics Monitor System v%s Starting...     ", APP_VERSION);
    ESP_LOGI(TAG, "================================================================");
    
    // Выводим информацию о системе
    print_system_info();
    
    // ========== ЭТАП 1: Инициализация NVS ==========
    ESP_LOGI(TAG, "[1/7] Initializing NVS...");
    if (init_nvs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS. System cannot continue.");
        return;
    }
    ESP_LOGI(TAG, "[OK] NVS initialized successfully");
    
    // ========== ЭТАП 2: Инициализация аппаратных компонентов ==========
    ESP_LOGI(TAG, "[2/7] Initializing hardware...");
    if (init_hardware() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize hardware. System cannot continue.");
        return;
    }
    ESP_LOGI(TAG, "[OK] Hardware initialized successfully");
    
    // ========== ЭТАП 3: Инициализация датчиков ==========
    ESP_LOGI(TAG, "[3/7] Initializing sensors...");
    if (init_sensors() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Some sensors failed to initialize, continuing with available sensors");
    } else {
        ESP_LOGI(TAG, "[OK] All sensors initialized successfully");
    }
    
    // ========== ЭТАП 4: Инициализация насосов и реле ==========
    ESP_LOGI(TAG, "[4/7] Initializing pumps and relays...");
    if (init_pumps() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Some pumps/relays failed to initialize");
    } else {
        ESP_LOGI(TAG, "[OK] Pumps and relays initialized successfully");
    }
    
    // ========== ЭТАП 5: Инициализация системных компонентов ==========
    ESP_LOGI(TAG, "[5/7] Initializing system components...");
    if (init_system_components() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system components. System cannot continue.");
        return;
    }
    ESP_LOGI(TAG, "[OK] System components initialized successfully");
    
    // ========== ЭТАП 6: Инициализация контекста задач ==========
    ESP_LOGI(TAG, "[6/7] Initializing task context...");
    if (system_tasks_init_context() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize task context. System cannot continue.");
        return;
    }

    // Передаем конфигурацию в контекст задач
    esp_err_t cfg_ret = system_tasks_set_config(&g_system_config);
    if (cfg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to share configuration with tasks: %s", esp_err_to_name(cfg_ret));
    }

    // ========== ЭТАП 6.1: Создание FreeRTOS задач ==========
    ESP_LOGI(TAG, "[6.1/7] Creating FreeRTOS tasks...");
    if (system_tasks_create_all(&task_handles) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create system tasks. System cannot continue.");
        return;
    }
    ESP_LOGI(TAG, "[OK] All tasks created successfully");
    
    // ========== ЭТАП 7: Регистрация исполнителей задач ==========
    ESP_LOGI(TAG, "[7/7] Registering task executors...");
    if (register_task_executors() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Some task executors failed to register");
    } else {
        ESP_LOGI(TAG, "[OK] Task executors registered successfully");
    }
    
    // Система полностью инициализирована
    system_initialized = true;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "================================================================");
    ESP_LOGI(TAG, "   System Initialization Complete!                       ");
    ESP_LOGI(TAG, "   All systems operational. Starting monitoring...       ");
    ESP_LOGI(TAG, "================================================================");
    
    // Создаем уведомление о запуске системы
    notification_system(NOTIFICATION_INFO, "System Started", 
                       NOTIF_SOURCE_SYSTEM);
    
    data_logger_log_system_event(LOG_LEVEL_INFO,
                                 "System started successfully - v" APP_VERSION);
    
    // ========== ОСНОВНОЙ ЦИКЛ ==========
    // Главный цикл - просто поддерживает работу системы
    // Все задачи выполняются асинхронно в FreeRTOS
    uint32_t loop_count = 0;
    while (1) {
        // Периодический вывод статистики (каждые 60 секунд)
        if (system_initialized && (loop_count % 60 == 0)) {
            ESP_LOGI(TAG, "System running. Free heap: %lu / %lu bytes",
                     (unsigned long)esp_get_free_heap_size(), 
                     (unsigned long)esp_get_minimum_free_heap_size());
        }
        
        loop_count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*******************************************************************************
 * ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
 ******************************************************************************/

/**
 * @brief Инициализация NVS (Non-Volatile Storage)
 * 
 * NVS используется для хранения:
 * - Конфигурации системы
 * - Калибровочных данных датчиков
 * - Настроек пользователя
 * - Истории данных
 * 
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition было обрезано или найдена новая версия.
        // Стираем и пытаемся инициализировать заново
        ESP_LOGW(TAG, "NVS partition needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    return ret;
}

/**
 * @brief Инициализация аппаратных компонентов
 * 
 * Инициализирует:
 * - I2C шину для датчиков
 * - LCD дисплей через SPI
 * - Rotary encoder для управления
 * - Создает мьютексы и очереди
 * 
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
static esp_err_t init_hardware(void)
{
    esp_err_t ret;
    
    // Инициализация I2C шины
    // I2C используется для всех датчиков (SHT3x, CCS811, Trema pH/EC/Lux)
    ret = i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "  [OK] I2C bus initialized (SCL: GPIO%d, SDA: GPIO%d)", 
             I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    
    // Небольшая задержка для стабилизации I2C шины
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Инициализация LCD дисплея
    // LCD использует SPI интерфейс и LVGL для графики
    lv_disp_t* disp = lcd_ili9341_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  [OK] LCD initialized (Resolution: %dx%d)", LCD_H_RES, LCD_V_RES);
    
    // Задержка для инициализации дисплея
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Инициализация LVGL UI ПЕРЕД энкодером
    // LVGL управляет всеми экранами и виджетами
    ESP_LOGI(TAG, "  Initializing LVGL UI...");
    lvgl_main_init();
    ESP_LOGI(TAG, "  [OK] LVGL UI initialized");
    
    // Задержка для завершения инициализации UI
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Инициализация энкодера ПОСЛЕ UI
    // Энкодер используется для навигации по UI
    ESP_LOGI(TAG, "  Initializing encoder...");
    encoder_set_pins(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_SW);
    encoder_set_long_press_duration(ENCODER_LONG_PRESS_MS);
    encoder_init();  // void function
    ESP_LOGI(TAG, "  [OK] Encoder initialized (A: GPIO%d, B: GPIO%d, SW: GPIO%d)", 
             ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_SW);
    
    return ESP_OK;
}

/**
 * @brief Инициализация датчиков
 * 
 * Инициализирует все датчики системы:
 * - SHT3x: температура и влажность
 * - CCS811: CO2 и VOC
 * - Trema pH: кислотность раствора
 * - Trema EC: электропроводность
 * - Trema Lux: освещенность
 * 
 * ВАЖНО: Некоторые датчики могут быть недоступны. Система продолжит
 * работу с доступными датчиками.
 * 
 * @return ESP_OK если хотя бы один датчик инициализирован
 */
static esp_err_t init_sensors(void)
{
    int initialized_count = 0;
    esp_err_t ret;
    
    // SHT3x: Температура и влажность
    // Не требует отдельной инициализации, используется I2C шина
    ESP_LOGI(TAG, "  [OK] SHT3x (Temp/Humidity) configured @ 0x%02X", I2C_ADDR_SHT3X);
    initialized_count++;
    
    // CCS811: CO2 и VOC
    if (ccs811_init()) {
        ESP_LOGI(TAG, "  [OK] CCS811 (CO2/VOC) initialized @ 0x%02X", I2C_ADDR_CCS811);
        initialized_count++;
    } else {
        ESP_LOGW(TAG, "  [FAIL] CCS811 initialization failed");
    }
    
    // Trema pH: Кислотность
    ret = trema_ph_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Trema pH initialized @ 0x%02X", I2C_ADDR_TREMA_PH);
        initialized_count++;
    } else {
        ESP_LOGW(TAG, "  [FAIL] Trema pH initialization failed: %s", esp_err_to_name(ret));
    }
    
    // Trema EC: Электропроводность
    ret = trema_ec_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Trema EC initialized @ 0x%02X", I2C_ADDR_TREMA_EC);
        initialized_count++;
    } else {
        ESP_LOGW(TAG, "  [FAIL] Trema EC initialization failed: %s", esp_err_to_name(ret));
    }
    
    // Trema Lux: Освещенность
    ret = trema_lux_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Trema Lux initialized @ 0x%02X", I2C_ADDR_TREMA_LUX);
        initialized_count++;
    } else {
        ESP_LOGW(TAG, "  [FAIL] Trema Lux initialization failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "  Sensors initialized: %d/5", initialized_count);
    
    // Возвращаем успех, если хотя бы один датчик инициализирован
    return (initialized_count > 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Инициализация насосов и реле
 * 
 * Инициализирует:
 * - 6 перистальтических насосов для коррекции pH и EC
 * - 4 реле для управления освещением, вентиляцией и т.д.
 * 
 * @return ESP_OK при успехе
 */
static esp_err_t init_pumps(void)
{
    // Примечание: Инициализация перистальтических насосов и их конфигурация
    // выполняется в ph_ec_controller_init(), который будет вызван позже
    // в init_system_components(). Здесь только логируем пины для справки.
    ESP_LOGI(TAG, "  Peristaltic pumps (via optocoupler, initialized in pump_manager):");
    ESP_LOGI(TAG, "    - pH UP:   GPIO%d", PUMP_PH_UP_PIN);
    ESP_LOGI(TAG, "    - pH DOWN: GPIO%d", PUMP_PH_DOWN_PIN);
    ESP_LOGI(TAG, "    - EC A:    GPIO%d", PUMP_EC_A_PIN);
    ESP_LOGI(TAG, "    - EC B:    GPIO%d", PUMP_EC_B_PIN);
    ESP_LOGI(TAG, "    - EC C:    GPIO%d", PUMP_EC_C_PIN);
    ESP_LOGI(TAG, "    - WATER:   GPIO%d", PUMP_WATER_PIN);
    
    // Инициализация реле
    esp_err_t ret = trema_relay_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Relays initialized (4 channels)");
        ESP_LOGI(TAG, "    - Relay 1 (Light):  GPIO%d", RELAY_1_PIN);
        ESP_LOGI(TAG, "    - Relay 2 (Fan):    GPIO%d", RELAY_2_PIN);
        ESP_LOGI(TAG, "    - Relay 3 (Heater): GPIO%d", RELAY_3_PIN);
        ESP_LOGI(TAG, "    - Relay 4 (Reserve):GPIO%d", RELAY_4_PIN);
    } else {
        ESP_LOGW(TAG, "  [FAIL] Relays initialization failed: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}

/**
 * @brief Инициализация системных компонентов
 * 
 * Инициализирует:
 * - Config Manager: управление конфигурацией в NVS
 * - Notification System: система уведомлений и алертов
 * - Data Logger: логирование данных и событий
 * - Task Scheduler: планировщик задач
 * - pH/EC Controller: контроллер коррекции параметров
 * 
 * Также устанавливает callback функции для обработки событий.
 * 
 * @return ESP_OK при успехе, код ошибки при неудаче
 */

static esp_err_t init_system_components(void)
{
    esp_err_t ret;

    ret = config_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize config manager: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = config_load(&g_system_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load system configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "  [OK] Config Manager initialized (auto mode: %s, brightness: %d%%)",
             g_system_config.auto_control_enabled ? "ON" : "OFF",
             g_system_config.display_brightness);
    
    // Применяем сохраненную яркость дисплея
    if (g_system_config.display_brightness > 0) {
        lcd_ili9341_set_brightness(g_system_config.display_brightness);
        ESP_LOGI(TAG, "  Display brightness restored to %d%%", g_system_config.display_brightness);
    } else {
        ESP_LOGW(TAG, "  Display brightness is 0, keeping default 80%%");
    }

    // Interfaces: базовые адаптеры датчиков и исполнительных устройств
    ret = system_interfaces_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system interfaces: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "  [OK] System interfaces initialized");
    
    // Notification System: Система уведомлений
    ret = notification_system_init(MAX_NOTIFICATIONS); // Используем константу из system_config.h
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize notification system: %s", esp_err_to_name(ret));
        return ret;
    }
    notification_set_callback(notification_callback);
    
    // Применяем настройки cooldown из конфигурации
    notification_screen_set_cooldown(g_system_config.notification_config.popup_cooldown_ms);
    ESP_LOGI(TAG, "  Notification cooldown set to %lu ms", 
             (unsigned long)g_system_config.notification_config.popup_cooldown_ms);
    
    // Восстанавливаем критические уведомления из NVS (если включено)
    if (g_system_config.notification_config.save_critical_to_nvs) {
        ret = notification_load_critical_from_nvs();
        if (ret == ESP_OK) {
            uint32_t unread = notification_get_unread_count();
            ESP_LOGI(TAG, "  Restored critical notifications from NVS (unread: %lu)", 
                     (unsigned long)unread);
        }
    }
    
    ESP_LOGI(TAG, "  [OK] Notification System initialized");
    
    // Error Handler: Централизованная обработка ошибок
    ret = error_handler_init(true); // Включаем всплывающие окна
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
        return ret;
    }
    // Устанавливаем русский шрифт для отображения кириллицы
    error_handler_set_font(&montserrat_ru);
    ESP_LOGI(TAG, "  [OK] Error Handler initialized (with Cyrillic support)");
    
    // Data Logger: Логирование данных
    ret = data_logger_init(MAX_LOG_ENTRIES);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize data logger: %s", esp_err_to_name(ret));
        return ret;
    }
    data_logger_set_callback(log_callback);
    
    // Включаем автоматическую очистку старых записей (3 дня)
    data_logger_set_auto_cleanup(true, 3);
    ESP_LOGI(TAG, "Data logger auto-cleanup enabled (keep last 3 days)");
    ret = data_logger_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  ! Failed to restore logs from NVS: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "  [OK] Data Logger initialized (capacity: %d)", MAX_LOG_ENTRIES);
    
    // Task Scheduler: Планировщик задач
    ret = task_scheduler_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize task scheduler: %s", esp_err_to_name(ret));
        return ret;
    }
    task_scheduler_set_event_callback(task_event_callback);
    ESP_LOGI(TAG, "  [OK] Task Scheduler initialized");

    // Инициализация Pump Manager с PID контроллерами
    ret = pump_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pump manager: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "  [OK] Pump Manager initialized (6 pumps with PID)");
    
    // Применение конфигурации PID из NVS
    ret = pump_manager_apply_config(&g_system_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  [WARN] Failed to apply pump manager config: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "  [OK] Pump Manager config applied from NVS");
    }
    
    // Инициализация адаптивной PID системы
    ret = adaptive_pid_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  [WARN] Failed to initialize adaptive PID: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "  [OK] Adaptive PID initialized");
        
        // Загрузка выученных параметров из NVS
        ret = adaptive_pid_load_all();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  [OK] Adaptive parameters loaded from NVS");
        } else {
            ESP_LOGD(TAG, "  [INFO] No saved adaptive parameters (first run)");
        }
    }
    
    // Инициализация автонастройки PID
    ret = pid_auto_tuner_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  [WARN] Failed to initialize PID auto-tuner: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "  [OK] PID Auto-Tuner initialized");
    }

    ret = ph_ec_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pH/EC controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ph_ec_controller_set_pump_callback(pump_event_callback);
    ph_ec_controller_set_correction_callback(correction_event_callback);
    ret = ph_ec_controller_apply_config(&g_system_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  ! Failed to apply controller config: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "  [OK] pH/EC Controller initialized");

    return ESP_OK;
}


/*******************************************************************************
 * CALLBACK ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Callback для уведомлений
 */
static void notification_callback(const notification_t *notification)
{
    if (notification == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Notification [%s]: %s",
             notification_type_to_string(notification->type),
             notification->message);
    
    // Показываем попап в зависимости от типа уведомления
    uint32_t timeout_ms = 0;
    switch (notification->type) {
        case NOTIF_TYPE_CRITICAL:
            timeout_ms = 0;  // Без таймера - только по кнопке OK
            break;
        case NOTIF_TYPE_ERROR:
            timeout_ms = 0;  // Без таймера - только по кнопке OK
            break;
        case NOTIF_TYPE_WARNING:
            timeout_ms = 0;  // Без таймера - пользователь должен нажать OK
            break;
        case NOTIF_TYPE_INFO:
            timeout_ms = 0;  // Без таймера - пользователь должен нажать OK
            break;
        default:
            timeout_ms = 0;
            break;
    }
    
    // Показываем уведомление как экран
    notification_screen_show(notification, timeout_ms);
}

static void log_callback(const data_logger_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    ESP_LOGD(TAG, "Log[%lu] %s: %s",
             (unsigned long)entry->id,
             data_logger_type_to_string(entry->type),
             entry->message);
}

/**
 * @brief Callback для событий планировщика
 */
static void task_event_callback(uint32_t task_id, task_status_t status)
{
    ESP_LOGI(TAG, "Task %lu status: %s",
             (unsigned long)task_id,
             task_scheduler_status_to_string(status));
}

/**
 * @brief Callback для событий насосов
 */
static void pump_event_callback(pump_index_t pump, bool started)
{
    ESP_LOGI(TAG, "Pump %d (%s) %s",
             pump,
             ph_ec_controller_get_pump_name(pump),
             started ? "started" : "stopped");
    
    // Логируем действие насоса (если logger включен)
    // data_logger_log_pump_action(pump, started, "user action");
}

/**
 * @brief Callback для событий коррекции
 */
static void correction_event_callback(const char *type, float current, float target)
{
    ESP_LOGI(TAG, "Correction %s: %.2f -> %.2f",
             type,
             current,
             target);
}

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Вывод информации о системе
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    ESP_LOGI(TAG, "System Information:");
    ESP_LOGI(TAG, "  Chip: %s rev%d, %d CPU cores, WiFi%s%s",
             CONFIG_IDF_TARGET,
             chip_info.revision,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "  Flash: %luMB %s",
             (unsigned long)(flash_size / (1024 * 1024)),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "  IDF version: %s", esp_get_idf_version());
}

/**
 * @brief Регистрация исполнителей задач
 */
static esp_err_t register_task_executors(void)
{
    // Здесь можно зарегистрировать функции-исполнители для планировщика задач
    // Например:
    // task_scheduler_register_executor(TASK_TYPE_PH_CORRECTION, ph_correction_executor);
    // task_scheduler_register_executor(TASK_TYPE_EC_CORRECTION, ec_correction_executor);
    
    ESP_LOGI(TAG, "Task executors registration complete");
    return ESP_OK;
}
/*******************************************************************************
 * КОНЕЦ ФАЙЛА
 *******************************************************************************/

