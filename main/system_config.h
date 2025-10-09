/**
 * @file system_config.h
 * @brief Централизованная конфигурация системы гидропоники
 * 
 * Этот файл содержит все определения пинов, адресов устройств, констант и параметров
 * системы в одном месте для упрощения настройки и предотвращения конфликтов.
 * 
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * ВЕРСИЯ СИСТЕМЫ
 ******************************************************************************/
#define SYSTEM_VERSION_MAJOR 3
#define SYSTEM_VERSION_MINOR 0
#define SYSTEM_VERSION_PATCH 0
#define SYSTEM_VERSION_STRING "3.0.0-advanced"

/*******************************************************************************
 * КОНФИГУРАЦИЯ LCD ДИСПЛЕЯ (ILI9341 через SPI)
 ******************************************************************************/
#define LCD_SPI_HOST    SPI2_HOST           // SPI хост
#define LCD_PIN_MOSI    11                  // SPI MOSI (Master Out Slave In)
#define LCD_PIN_SCLK    12                  // SPI Clock
#define LCD_PIN_CS      10                  // Chip Select
#define LCD_PIN_DC      9                   // Data/Command
#define LCD_PIN_RST     14                  // Reset
#define LCD_PIN_BCKL    15                  // Backlight control

#define LCD_H_RES       240                 // Горизонтальное разрешение
#define LCD_V_RES       320                 // Вертикальное разрешение
#define LCD_PIXEL_CLK   40000000            // 40 MHz pixel clock

/*******************************************************************************
 * КОНФИГУРАЦИЯ I2C ШИНЫ (для датчиков)
 ******************************************************************************/
#define I2C_MASTER_NUM          0           // I2C порт номер
#define I2C_MASTER_SCL_IO       17          // GPIO для SCL
#define I2C_MASTER_SDA_IO       18          // GPIO для SDA
#define I2C_MASTER_FREQ_HZ      100000      // 100 kHz частота I2C
#define I2C_MASTER_TIMEOUT_MS   1000        // Timeout для I2C операций

// I2C адреса устройств
#define I2C_ADDR_SHT3X          0x44        // Датчик температуры/влажности
#define I2C_ADDR_CCS811         0x5A        // Датчик CO2/VOC
#define I2C_ADDR_TREMA_PH       0x0A        // Датчик pH (iarduino pH Flash-I2C, Model ID: 0x1A)
#define I2C_ADDR_TREMA_EC       0x08        // Датчик EC (iarduino TDS/EC Flash-I2C, Model ID: 0x19)
#define I2C_ADDR_TREMA_LUX      0x12        // Датчик освещенности

/*******************************************************************************
 * КОНФИГУРАЦИЯ ЭНКОДЕРА (для управления UI)
 ******************************************************************************/
#define ENCODER_PIN_A           1           // CLK (A) пин энкодера
#define ENCODER_PIN_B           2           // DT (B) пин энкодера
#define ENCODER_PIN_SW          3           // SW (кнопка) пин энкодера

#define ENCODER_LONG_PRESS_MS   2000        // Длительность длинного нажатия (2 сек)
#define ENCODER_DEBOUNCE_MS     50          // Время дебаунса (мс)

/*******************************************************************************
 * КОНФИГУРАЦИЯ ПЕРИСТАЛЬТИЧЕСКИХ НАСОСОВ
 * 
 * Каждый насос управляется драйвером L298N с двумя пинами (IA, IB):
 * - IA HIGH, IB LOW = насос работает вперед
 * - IA LOW, IB HIGH = насос работает назад
 * - IA LOW, IB LOW = насос остановлен
 ******************************************************************************/

// Насос pH UP (повышение pH)
#define PUMP_PH_UP_IA           4           // Управление IA
#define PUMP_PH_UP_IB           5           // Управление IB

// Насос pH DOWN (понижение pH)
#define PUMP_PH_DOWN_IA         6           // Управление IA
#define PUMP_PH_DOWN_IB         7           // Управление IB

// Насос EC A (раствор A)
#define PUMP_EC_A_IA            8           // Управление IA
#define PUMP_EC_A_IB            13          // Управление IB

// Насос EC B (раствор B)
#define PUMP_EC_B_IA            16          // Управление IA
#define PUMP_EC_B_IB            21          // Управление IB

// Насос EC C (раствор C)
#define PUMP_EC_C_IA            47          // Управление IA
#define PUMP_EC_C_IB            48          // Управление IB

// Насос WATER (подача чистой воды)
#define PUMP_WATER_IA           45          // Управление IA
#define PUMP_WATER_IB           46          // Управление IB

/*******************************************************************************
 * КОНФИГУРАЦИЯ РЕЛЕ (для управления освещением, вентиляцией и т.д.)
 ******************************************************************************/
#define RELAY_1_PIN             19          // Реле 1 (освещение)
#define RELAY_2_PIN             20          // Реле 2 (вентилятор)
#define RELAY_3_PIN             26          // Реле 3 (нагреватель)
#define RELAY_4_PIN             27          // Реле 4 (резерв)

/*******************************************************************************
 * ПАРАМЕТРЫ FREERTOS ЗАДАЧ
 * 
 * Приоритеты задач (выше = важнее):
 * - 8: pH/EC контроль (критично для растений)
 * - 7: Планировщик (управление задачами)
 * - 6: Display (UI должен быть отзывчивым)
 * - 5: Sensor (регулярное чтение данных)
 * - 4: Notifications (уведомления о проблемах)
 * - 3: Data logger (может подождать)
 ******************************************************************************/

// Размеры стека задач (в словах, 1 слово = 4 байта)
// Оптимизированы для экономии памяти с запасом безопасности
#define TASK_STACK_SIZE_SENSOR      5120    // Задача чтения датчиков (20KB) - увеличен для предотвращения stack overflow
#define TASK_STACK_SIZE_DISPLAY     3072    // Задача обновления дисплея (12KB)
#define TASK_STACK_SIZE_NOTIFICATION 2560   // Задача уведомлений (10KB) - увеличен для форматирования
#define TASK_STACK_SIZE_DATALOGGER  4096    // Задача логирования (16KB) - увеличен для NVS операций и буферов
#define TASK_STACK_SIZE_SCHEDULER   2048    // Задача планировщика (8KB) - увеличен для обработки задач
#define TASK_STACK_SIZE_PH_EC       2048    // Задача pH/EC контроля (8KB) - увеличен для вычислений
#define TASK_STACK_SIZE_ENCODER     2048    // Задача энкодера (8KB) - увеличен для обработки событий

// Приоритеты задач (0 = самый низкий, 31 = самый высокий)
#define TASK_PRIORITY_SENSOR        5       // Средний приоритет
#define TASK_PRIORITY_DISPLAY       6       // Высокий (для плавного UI)
#define TASK_PRIORITY_NOTIFICATION  4       // Средний
#define TASK_PRIORITY_DATALOGGER    3       // Низкий
#define TASK_PRIORITY_SCHEDULER     7       // Очень высокий
#define TASK_PRIORITY_PH_EC         8       // Критический
#define TASK_PRIORITY_ENCODER       6       // Высокий (для отзывчивости)

// Интервалы выполнения задач (в миллисекундах)
#define TASK_INTERVAL_SENSOR        2000    // Чтение датчиков каждые 2 сек
#define TASK_INTERVAL_DISPLAY       1000    // Обновление дисплея каждую секунду
#define TASK_INTERVAL_NOTIFICATION  5000    // Проверка уведомлений каждые 5 сек
#define TASK_INTERVAL_DATALOGGER    60000   // Логирование каждую минуту
#define TASK_INTERVAL_SCHEDULER     1000    // Проверка планировщика каждую секунду
#define TASK_INTERVAL_PH_EC         500     // Контроль pH/EC каждые 0.5 сек

/*******************************************************************************
 * РАЗМЕРЫ ОЧЕРЕДЕЙ FREERTOS (оптимизированы)
 ******************************************************************************/
#define QUEUE_SIZE_SENSOR_DATA      3       // Очередь данных датчиков (малая для предотвращения переполнения)
#define QUEUE_SIZE_ENCODER          10      // Очередь событий энкодера
#define QUEUE_SIZE_NOTIFICATIONS    10      // Очередь уведомлений

/*******************************************************************************
 * ПАРАМЕТРЫ ДАТЧИКОВ
 ******************************************************************************/

// pH датчик
#define PH_MIN_VALUE                0.0f    // Минимальное значение pH
#define PH_MAX_VALUE                14.0f   // Максимальное значение pH
#define PH_TARGET_DEFAULT           6.8f    // Целевое значение по умолчанию
#define PH_TOLERANCE_DEFAULT        0.1f    // Допустимое отклонение
#define PH_ALARM_LOW_DEFAULT        6.0f    // Нижний порог тревоги
#define PH_ALARM_HIGH_DEFAULT       7.5f    // Верхний порог тревоги

// EC датчик (электропроводность)
#define EC_MIN_VALUE                0.0f    // Минимальное значение EC
#define EC_MAX_VALUE                5.0f    // Максимальное значение EC
#define EC_TARGET_DEFAULT           1.5f    // Целевое значение по умолчанию
#define EC_TOLERANCE_DEFAULT        0.1f    // Допустимое отклонение
#define EC_ALARM_LOW_DEFAULT        0.8f    // Нижний порог тревоги
#define EC_ALARM_HIGH_DEFAULT       2.0f    // Верхний порог тревоги

// Температура
#define TEMP_MIN_VALUE              -40.0f  // Минимальная температура (°C)
#define TEMP_MAX_VALUE              85.0f   // Максимальная температура (°C)
#define TEMP_TARGET_DEFAULT         24.0f   // Целевая температура
#define TEMP_ALARM_LOW_DEFAULT      18.0f   // Нижний порог тревоги
#define TEMP_ALARM_HIGH_DEFAULT     30.0f   // Верхний порог тревоги

// Влажность
#define HUMIDITY_MIN_VALUE          0.0f    // Минимальная влажность (%)
#define HUMIDITY_MAX_VALUE          100.0f  // Максимальная влажность (%)
#define HUMIDITY_TARGET_DEFAULT     70.0f   // Целевая влажность
#define HUMIDITY_ALARM_LOW_DEFAULT  45.0f   // Нижний порог тревоги
#define HUMIDITY_ALARM_HIGH_DEFAULT 75.0f   // Верхний порог тревоги

// Освещенность (Lux)
#define LUX_MIN_VALUE               0.0f    // Минимальная освещенность
#define LUX_MAX_VALUE               10000.0f // Максимальная освещенность
#define LUX_TARGET_DEFAULT          500.0f  // Целевая освещенность
#define LUX_ALARM_LOW_DEFAULT       400.0f  // Нижний порог тревоги
#define LUX_ALARM_HIGH_DEFAULT      1500.0f // Верхний порог тревоги

// CO2
#define CO2_MIN_VALUE               0.0f    // Минимальный CO2 (ppm)
#define CO2_MAX_VALUE               5000.0f // Максимальный CO2 (ppm)
#define CO2_TARGET_DEFAULT          450.0f  // Целевой CO2
#define CO2_ALARM_LOW_DEFAULT       0.0f    // Нижний порог тревоги
#define CO2_ALARM_HIGH_DEFAULT      800.0f  // Верхний порог тревоги

/*******************************************************************************
 * ПАРАМЕТРЫ pH/EC КОНТРОЛЛЕРА
 ******************************************************************************/

// Насосы
#define PUMP_FLOW_RATE_DEFAULT      1.0f    // Скорость потока (мл/сек)
#define PUMP_MIN_DURATION_MS        100     // Минимальное время работы насоса
#define PUMP_MAX_DURATION_MS        30000   // Максимальное время работы насоса (30 сек)
#define PUMP_COOLDOWN_MS            5000    // Время охлаждения между включениями

// Коррекция pH
#define PH_CORRECTION_INTERVAL_MS   300000  // Минимальный интервал между коррекциями (5 мин)
#define PH_MAX_CORRECTION_ML        50.0f   // Максимальный объем коррекции за раз
#define PH_SOLUTION_CONCENTRATION   0.1f    // Концентрация раствора (%)

// Коррекция EC
#define EC_CORRECTION_INTERVAL_MS   600000  // Минимальный интервал между коррекциями (10 мин)
#define EC_MAX_CORRECTION_ML        100.0f  // Максимальный объем коррекции за раз
#define EC_SOLUTION_CONCENTRATION   0.1f    // Концентрация раствора (%)

/*******************************************************************************
 * ПАРАМЕТРЫ СИСТЕМЫ УВЕДОМЛЕНИЙ
 ******************************************************************************/
#define MAX_NOTIFICATIONS           30      // Максимальное количество уведомлений (уменьшено)
#define NOTIFICATION_DURATION_MS    5000    // Длительность уведомления по умолчанию

/*******************************************************************************
 * ПАРАМЕТРЫ ЛОГИРОВАНИЯ ДАННЫХ
 ******************************************************************************/
#define MAX_LOG_ENTRIES             50      // Максимальное количество записей в логе (уменьшено для экономии памяти)
#define LOG_AUTO_CLEANUP_DAYS       7       // Автоматическая очистка записей старше N дней
#define DATA_LOG_INTERVAL_MS        60000   // Интервал логирования данных (1 мин)

/*******************************************************************************
 * ПАРАМЕТРЫ ПЛАНИРОВЩИКА ЗАДАЧ
 ******************************************************************************/
#define MAX_SCHEDULED_TASKS         20      // Максимальное количество задач в планировщике

/*******************************************************************************
 * ПАРАМЕТРЫ NVS (Non-Volatile Storage)
 ******************************************************************************/
#define NVS_NAMESPACE               "hydro_config" // Namespace для NVS
#define NVS_CONFIG_VERSION          1       // Версия формата конфигурации

/*******************************************************************************
 * ПАРАМЕТРЫ WATCHDOG
 ******************************************************************************/
#define WATCHDOG_TIMEOUT_MS         30000   // Timeout watchdog (30 сек)
#define WATCHDOG_ENABLED            true    // Включить watchdog

/*******************************************************************************
 * РЕЖИМ ОТЛАДКИ
 ******************************************************************************/
#define DEBUG_MODE                  true    // Режим отладки
#define DEBUG_VERBOSE               false   // Подробная отладка
#define DEBUG_SENSOR_DATA           true    // Отладка данных датчиков
#define DEBUG_UI_EVENTS             false   // Отладка событий UI
#define DEBUG_PUMP_CONTROL          true    // Отладка управления насосами

/*******************************************************************************
 * ПРОВЕРКА КОНФИГУРАЦИИ (выполняется на этапе компиляции)
 ******************************************************************************/

// Проверка конфликтов пинов
#if (LCD_PIN_MOSI == I2C_MASTER_SCL_IO) || (LCD_PIN_MOSI == I2C_MASTER_SDA_IO)
#error "LCD MOSI pin conflicts with I2C pins"
#endif

#if (ENCODER_PIN_A == ENCODER_PIN_B) || (ENCODER_PIN_A == ENCODER_PIN_SW) || (ENCODER_PIN_B == ENCODER_PIN_SW)
#error "Encoder pins must be different"
#endif

// Проверка разумности параметров
#if (TASK_INTERVAL_SENSOR < 100) || (TASK_INTERVAL_SENSOR > 60000)
#error "TASK_INTERVAL_SENSOR must be between 100 and 60000 ms"
#endif

// Проверки диапазонов убраны - препроцессор не поддерживает float

// Количество датчиков
#define SENSOR_COUNT 6

// Структура данных датчиков для app_main.c
typedef struct {
    uint64_t timestamp;           // Время измерения
    float ph;                     // pH значение
    float ec;                     // EC значение
    float temperature;            // Температура
    float humidity;               // Влажность
    float lux;                    // Освещенность
    float co2;                    // CO2 уровень
    bool valid[SENSOR_COUNT];     // Флаги валидности для каждого датчика
    
    // Дополнительные поля для совместимости с UI компонентами
    float current_value;          // Текущее значение (для UI)
    float target_value;           // Целевое значение
    float min_value;              // Минимальное значение
    float max_value;              // Максимальное значение
    bool alarm_enabled;           // Включена ли сигнализация
    float alarm_low;              // Нижний порог сигнализации
    float alarm_high;             // Верхний порог сигнализации
    const char *unit;             // Единица измерения
    const char *name;             // Имя датчика
    const char *description;      // Описание датчика
    int decimals;                 // Количество знаков после запятой
    
    // Алиасы для совместимости
    float temp;                   // Алиас для temperature
    float hum;                    // Алиас для humidity
} sensor_data_t;

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ
 ******************************************************************************/

// Преобразование времени
#define MS_TO_TICKS(ms)             pdMS_TO_TICKS(ms)
#define SEC_TO_TICKS(sec)           pdMS_TO_TICKS((sec) * 1000)
#define MIN_TO_TICKS(min)           pdMS_TO_TICKS((min) * 60 * 1000)

// Ограничение значений
#define CLAMP(x, min, max)          ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// Проверка диапазона
#define IN_RANGE(x, min, max)       (((x) >= (min)) && ((x) <= (max)))

// Абсолютное значение
#ifndef ABS
#define ABS(x)                      ((x) < 0 ? -(x) : (x))
#endif

/*******************************************************************************
 * ТИПЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Индексы датчиков в массиве
 */
typedef enum {
    SENSOR_INDEX_PH = 0,        ///< pH датчик
    SENSOR_INDEX_EC,            ///< EC датчик
    SENSOR_INDEX_TEMPERATURE,   ///< Температура
    SENSOR_INDEX_HUMIDITY,      ///< Влажность
    SENSOR_INDEX_LUX,           ///< Освещенность
    SENSOR_INDEX_CO2,           ///< CO2
    SENSOR_INDEX_COUNT          ///< Количество датчиков
} sensor_index_t;

/**
 * @brief Индексы насосов
 */
typedef enum {
    PUMP_INDEX_PH_UP = 0,       ///< Насос pH UP
    PUMP_INDEX_PH_DOWN,         ///< Насос pH DOWN
    PUMP_INDEX_EC_A,            ///< Насос EC A
    PUMP_INDEX_EC_B,            ///< Насос EC B
    PUMP_INDEX_EC_C,            ///< Насос EC C
    PUMP_INDEX_WATER,           ///< Насос воды
    PUMP_INDEX_COUNT            ///< Количество насосов
} pump_index_t;

/**
 * @brief Индексы реле
 */
typedef enum {
    RELAY_INDEX_LIGHT = 0,      ///< Реле освещения
    RELAY_INDEX_FAN,            ///< Реле вентилятора
    RELAY_INDEX_HEATER,         ///< Реле нагревателя
    RELAY_INDEX_RESERVE,        ///< Резервное реле
    RELAY_INDEX_COUNT           ///< Количество реле
} relay_index_t;

/**
 * @brief Конфигурация датчика
 */
typedef struct {
    float target_value;           // Целевое значение
    float alarm_low;              // Нижний порог сигнализации
    float alarm_high;             // Верхний порог сигнализации
    bool enabled;                 // Включен ли датчик
} sensor_config_t;

/**
 * @brief Конфигурация насоса
 */
typedef struct {
    char name[32];              // Название насоса
    bool enabled;               // Включен
    float flow_rate_ml_per_sec; // Скорость потока мл/сек
    uint32_t min_duration_ms;   // Минимальная длительность
    uint32_t max_duration_ms;   // Максимальная длительность
    uint32_t cooldown_ms;       // Задержка перед следующим запуском
    float concentration_factor; // Фактор концентрации
} pump_config_t;

/**
 * @brief Основная конфигурация системы
 */
typedef struct {
    bool auto_control_enabled;    // Включен ли автоматический контроль
    sensor_config_t sensor_config[SENSOR_COUNT];  // Конфигурация датчиков
    pump_config_t pump_config[PUMP_INDEX_COUNT];  // Конфигурация насосов
} system_config_t;

#ifdef __cplusplus
}
#endif
