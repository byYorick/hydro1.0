/**
 * @file system_monitor.h
 * @brief Система мониторинга и оптимизации производительности ESP32S3
 *
 * Максимально использует возможности ESP32S3:
 * - Двухъядерный процессор с разделением задач по ядрам
 * - Аппаратное ускорение криптографических операций
 * - Встроенный USB для прямого подключения
 * - 8MB PSRAM для больших буферов данных
 * - Оптимизированная работа с памятью
 * - Мониторинг температуры и энергопотребления
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_config.h"

/**
 * @brief Режимы оптимизации производительности
 */
typedef enum {
    PERFORMANCE_MODE_HIGH = 0,          // Максимальная производительность
    PERFORMANCE_MODE_BALANCED,          // Сбалансированный режим
    PERFORMANCE_MODE_LOW_POWER,         // Экономия энергии
    PERFORMANCE_MODE_CUSTOM             // Пользовательские настройки
} performance_mode_t;

/**
 * @brief Структура статистики производительности
 */
typedef struct {
    // Использование процессора
    float cpu_usage_core0;              // Загрузка ядра 0 (%)
    float cpu_usage_core1;              // Загрузка ядра 1 (%)
    uint32_t cpu_freq_mhz;              // Текущая частота CPU (MHz)

    // Использование памяти
    uint32_t heap_free;                 // Свободная heap память (байты)
    uint32_t heap_used;                 // Используемая heap память (байты)
    uint32_t heap_min_free;             // Минимальный уровень свободной памяти
    uint32_t psram_free;                // Свободная PSRAM (байты)
    uint32_t psram_used;                // Используемая PSRAM (байты)

    // Статистика задач
    uint32_t total_tasks;               // Общее количество задач
    uint32_t running_tasks;             // Количество активных задач
    uint32_t highest_stack_usage;       // Максимальное использование стека

    // Температура и энергопотребление
    float chip_temperature;             // Температура чипа (°C)
    float wifi_temperature;             // Температура WiFi модуля (°C)
    uint32_t current_consumption;       // Потребление тока (mA)

    // Производительность сети
    uint32_t wifi_throughput;           // Пропускная способность WiFi (bps)
    uint32_t ble_throughput;            // Пропускная способность BLE (bps)

    // Время работы системы
    uint64_t uptime_seconds;            // Время работы (секунды)
    uint32_t reset_count;               // Количество перезагрузок

    // Качество связи
    int8_t wifi_rssi;                   // Уровень сигнала WiFi (dBm)
    uint8_t wifi_noise;                 // Уровень шума WiFi
    uint32_t ble_rssi;                  // Уровень сигнала BLE

    // Статистика ошибок
    uint32_t i2c_errors;                // Ошибки I2C шины
    uint32_t spi_errors;                // Ошибки SPI шины
    uint32_t task_watchdog_resets;      // Сбросы watchdog задач

    // Графическая производительность
    uint32_t lvgl_fps;                  // Частота обновления LVGL (FPS)
    uint32_t display_refresh_rate;      // Частота обновления дисплея (Hz)

    // Буферы и очереди
    uint32_t queue_usage_percent;       // Использование очередей (%)
    uint32_t buffer_overflows;          // Переполнения буферов

    // Энергетическая эффективность
    float operations_per_mah;           // Операций на mAh
    uint32_t sleep_time_percent;        // Время в режиме сна (%)

    // Качество кода
    uint32_t memory_leaks;              // Обнаруженные утечки памяти
    uint32_t stack_overflows;           // Переполнения стека
    uint32_t heap_fragmentation;        // Фрагментация памяти (%)
} system_performance_stats_t;

/**
 * @brief Структура конфигурации мониторинга
 */
typedef struct {
    bool enable_cpu_monitoring;         // Мониторинг CPU
    bool enable_memory_monitoring;      // Мониторинг памяти
    bool enable_temperature_monitoring; // Мониторинг температуры
    bool enable_network_monitoring;     // Мониторинг сети
    bool enable_watchdog_monitoring;    // Мониторинг watchdog
    bool enable_performance_logging;    // Логирование производительности

    uint32_t monitoring_interval_ms;    // Интервал мониторинга (мс)
    uint32_t stats_retention_period;    // Период хранения статистики (часы)

    performance_mode_t performance_mode; // Режим производительности

    // Настройки оптимизации памяти
    bool enable_memory_optimization;    // Включить оптимизацию памяти
    bool enable_psram_usage;            // Использовать PSRAM
    bool enable_memory_pool;            // Использовать пул памяти
    uint32_t memory_pool_size;          // Размер пула памяти (байты)

    // Настройки оптимизации CPU
    bool enable_task_affinity;          // Включить привязку задач к ядрам
    bool enable_frequency_scaling;      // Масштабирование частоты
    uint32_t min_cpu_frequency;         // Минимальная частота CPU (MHz)
    uint32_t max_cpu_frequency;         // Максимальная частота CPU (MHz)

    // Настройки энергосбережения
    bool enable_power_management;       // Управление энергопотреблением
    bool enable_auto_sleep;             // Автоматический сон при простое
    uint32_t sleep_timeout_ms;          // Таймаут для перехода в сон

    // Настройки мониторинга задач
    bool enable_task_stack_monitoring;  // Мониторинг стека задач
    bool enable_task_timing_monitoring; // Мониторинг времени задач
    uint32_t task_stack_threshold;      // Порог использования стека (%)

    // Настройки уведомлений
    bool enable_performance_alerts;     // Уведомления о производительности
    float cpu_usage_threshold;          // Порог использования CPU (%)
    float memory_usage_threshold;       // Порог использования памяти (%)
    float temperature_threshold;        // Порог температуры (°C)
} system_monitor_config_t;

/**
 * @brief Структура предупреждения производительности
 */
typedef struct {
    char alert_type[32];                // Тип предупреждения
    char description[128];              // Описание проблемы
    uint32_t severity;                  // Уровень серьезности (1-5)
    uint32_t timestamp;                 // Временная метка
    char recommended_action[128];       // Рекомендуемое действие
    bool acknowledged;                  // Подтверждено пользователем
} performance_alert_t;

/**
 * @brief Инициализация системы мониторинга производительности
 * @param config Конфигурация мониторинга (NULL для значений по умолчанию)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_init(const system_monitor_config_t *config);

/**
 * @brief Деинициализация системы мониторинга
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_deinit(void);

/**
 * @brief Получение текущей статистики производительности
 * @param stats Указатель на структуру для статистики
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_stats(system_performance_stats_t *stats);

/**
 * @brief Получение статистики конкретной задачи
 * @param task_handle Указатель на задачу
 * @param task_name Имя задачи (может быть NULL)
 * @param stack_usage_percent Использование стека в процентах
 * @param cpu_time_ticks Время CPU в тиках
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_task_stats(TaskHandle_t task_handle, const char *task_name,
                                       float *stack_usage_percent, uint32_t *cpu_time_ticks);

/**
 * @brief Оптимизация распределения задач по ядрам ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_task_distribution(void);

/**
 * @brief Автоматическая оптимизация памяти
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_memory(void);

/**
 * @brief Включение/выключение динамического масштабирования частоты
 * @param enable Включить масштабирование
 * @param min_freq Минимальная частота (MHz)
 * @param max_freq Максимальная частота (MHz)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_set_frequency_scaling(bool enable, uint32_t min_freq, uint32_t max_freq);

/**
 * @brief Получение текущей температуры чипа
 * @param temperature Указатель для температуры (°C)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_temperature(float *temperature);

/**
 * @brief Получение оценки энергопотребления
 * @param current_ma Указатель для тока (mA)
 * @param power_mw Указатель для мощности (mW)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_power_consumption(uint32_t *current_ma, uint32_t *power_mw);

/**
 * @brief Включение энергосберегающего режима
 * @param enable Включить энергосбережение
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_power_saving(bool enable);

/**
 * @brief Получение рекомендаций по оптимизации
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_optimization_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Регистрация callback функции для обработки предупреждений
 * @param alert_handler Функция обработчик предупреждений
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_register_alert_handler(void (*alert_handler)(const performance_alert_t *alert, void *ctx), void *user_ctx);

/**
 * @brief Получение списка активных предупреждений
 * @param alerts Массив для предупреждений
 * @param max_alerts Максимальное количество предупреждений
 * @return Количество активных предупреждений
 */
int system_monitor_get_active_alerts(performance_alert_t *alerts, int max_alerts);

/**
 * @brief Подтверждение предупреждения
 * @param alert_index Индекс предупреждения для подтверждения
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_acknowledge_alert(int alert_index);

/**
 * @brief Включение мониторинга конкретной задачи
 * @param task_handle Указатель на задачу
 * @param enable Включить/выключить мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_task_monitoring(TaskHandle_t task_handle, bool enable);

/**
 * @brief Получение детальной статистики памяти
 * @param total_heap Общий размер heap
 * @param free_heap Свободная heap память
 * @param largest_free_block Самый большой свободный блок
 * @param fragmentation Фрагментация памяти (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_memory_details(uint32_t *total_heap, uint32_t *free_heap,
                                           uint32_t *largest_free_block, float *fragmentation);

/**
 * @brief Принудительная дефрагментация памяти
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_defragment_memory(void);

/**
 * @brief Получение статистики использования PSRAM
 * @param total_psram Общий размер PSRAM
 * @param used_psram Используемая PSRAM
 * @param free_psram Свободная PSRAM
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_psram_stats(uint32_t *total_psram, uint32_t *used_psram, uint32_t *free_psram);

/**
 * @brief Оптимизация использования PSRAM для больших буферов
 * @param buffer_size Размер буфера для оптимизации
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_psram_usage(uint32_t buffer_size);

/**
 * @brief Включение мониторинга температуры с автоматическим throttling
 * @param enable Включить термический мониторинг
 * @param max_temperature Максимальная температура (°C)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_thermal_monitoring(bool enable, float max_temperature);

/**
 * @brief Получение статистики производительности сети
 * @param wifi_throughput Пропускная способность WiFi (bps)
 * @param ble_throughput Пропускная способность BLE (bps)
 * @param wifi_rssi Уровень сигнала WiFi (dBm)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_network_stats(uint32_t *wifi_throughput, uint32_t *ble_throughput, int8_t *wifi_rssi);

/**
 * @brief Оптимизация настроек WiFi для ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_wifi_settings(void);

/**
 * @brief Включение мониторинга качества кода (утечки памяти, переполнения стека)
 * @param enable Включить мониторинг качества кода
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_code_quality_monitoring(bool enable);

/**
 * @brief Получение отчета о производительности в текстовом формате
 * @param report Буфер для отчета (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_performance_report(char *report, size_t max_length);

/**
 * @brief Сохранение статистики производительности в файл
 * @param filename Имя файла для сохранения
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_save_stats_to_file(const char *filename);

/**
 * @brief Загрузка статистики производительности из файла
 * @param filename Имя файла для загрузки
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_load_stats_from_file(const char *filename);

/**
 * @brief Сброс всех счетчиков статистики
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_reset_stats(void);

/**
 * @brief Получение рекомендаций по настройке для ESP32S3
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_esp32s3_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Включение адаптивной оптимизации производительности
 * @param enable Включить адаптивную оптимизацию
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_adaptive_optimization(bool enable);

/**
 * @brief Получение оценки общей производительности системы
 * @param score Оценка производительности (0-100)
 * @param grade Буквенная оценка (A-F)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_performance_score(uint8_t *score, char *grade);

/**
 * @brief Экспорт данных мониторинга в JSON формате
 * @param json_buffer Буфер для JSON (минимум 2048 байт)
 * @param buffer_size Размер буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_export_json(char *json_buffer, size_t buffer_size);

/**
 * @brief Включение отладочного режима мониторинга
 * @param enable Включить отладочный режим
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_debug_mode(bool enable);

/**
 * @brief Получение детальной информации о задаче по имени
 * @param task_name Имя задачи
 * @param task_info Буфер для информации (минимум 256 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_task_details(const char *task_name, char *task_info, size_t max_length);

/**
 * @brief Анализ узких мест производительности
 * @param bottlenecks Буфер для узких мест (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_analyze_bottlenecks(char *bottlenecks, size_t max_length);

/**
 * @brief Рекомендация оптимальной конфигурации для ESP32S3
 * @param config Рекомендуемая конфигурация
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_optimal_config(system_monitor_config_t *config);

/**
 * @brief Мониторинг использования аппаратных ресурсов ESP32S3
 * @param crypto_usage Использование криптографического акселератора (%)
 * @param dma_usage Использование DMA каналов (%)
 * @param gpio_usage Использование GPIO ресурсов (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_hardware_usage(float *crypto_usage, float *dma_usage, float *gpio_usage);

/**
 * @brief Оптимизация планировщика задач для ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_scheduler(void);

/**
 * @brief Получение прогноза производительности при изменении нагрузки
 * @param predicted_cpu_usage Прогнозируемая загрузка CPU (%)
 * @param predicted_memory_usage Прогнозируемое использование памяти (%)
 * @param predicted_temperature Прогнозируемая температура (°C)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_performance_prediction(float *predicted_cpu_usage,
                                                  float *predicted_memory_usage,
                                                  float *predicted_temperature);

/**
 * @brief Включение мониторинга в реальном времени через мобильное приложение
 * @param enable Включить мониторинг в реальном времени
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_realtime_monitoring(bool enable);

/**
 * @brief Получение истории производительности за период времени
 * @param hours Количество часов для анализа
 * @param history_buffer Буфер для истории (минимум 1024 байт)
 * @param buffer_size Размер буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_performance_history(uint32_t hours, char *history_buffer, size_t buffer_size);

/**
 * @brief Автоматическая корректировка параметров производительности
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_tune_performance(void);

/**
 * @brief Получение детальной диагностики системы
 * @param diagnostic_info Буфер для диагностической информации (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_diagnostic_info(char *diagnostic_info, size_t max_length);

/**
 * @brief Тестирование производительности под нагрузкой
 * @param test_duration Продолжительность теста (секунды)
 * @param test_results Буфер для результатов теста (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_run_performance_test(uint32_t test_duration, char *test_results, size_t max_length);

/**
 * @brief Включение мониторинга энергопотребления в реальном времени
 * @param enable Включить мониторинг энергопотребления
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_power_monitoring(bool enable);

/**
 * @brief Получение рекомендаций по энергосбережению
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_power_saving_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Оптимизация для работы с мобильным приложением
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_for_mobile_app(void);

/**
 * @brief Получение метрик качества пользовательского интерфейса
 * @param ui_fps Частота обновления UI (FPS)
 * @param input_latency Задержка ввода (мс)
 * @param render_time Время рендеринга кадра (мс)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_ui_metrics(uint32_t *ui_fps, uint32_t *input_latency, uint32_t *render_time);

/**
 * @brief Включение предиктивного мониторинга производительности
 * @param enable Включить предиктивный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_predictive_monitoring(bool enable);

/**
 * @brief Получение комплексной оценки состояния системы
 * @param overall_score Общая оценка (0-100)
 * @param cpu_score Оценка CPU (0-100)
 * @param memory_score Оценка памяти (0-100)
 * @param network_score Оценка сети (0-100)
 * @param thermal_score Оценка температуры (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_system_health_score(uint8_t *overall_score, uint8_t *cpu_score,
                                               uint8_t *memory_score, uint8_t *network_score,
                                               uint8_t *thermal_score);

/**
 * @brief Автоматическая оптимизация на основе анализа производительности
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_optimize(void);

/**
 * @brief Экспорт полной статистики в CSV формате
 * @param filename Имя CSV файла
 * @param include_history Включить исторические данные
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_export_csv(const char *filename, bool include_history);

/**
 * @brief Получение рекомендаций по настройке для конкретного сценария использования
 * @param use_case Сценарий использования ("hydroponics", "mobile_app", "web_server", etc.)
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_use_case_recommendations(const char *use_case, char *recommendations, size_t max_length);

/**
 * @brief Включение мониторинга в фоновом режиме для минимального влияния на производительность
 * @param enable Включить фоновый мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_background_monitoring(bool enable);

/**
 * @brief Получение статистики использования аппаратных возможностей ESP32S3
 * @param usb_usage Использование USB (%)
 * @param crypto_usage Использование криптоакселератора (%)
 * @param ai_usage Использование AI акселератора (%)
 * @param adc_usage Использование ADC каналов (%)
 * @param dac_usage Использование DAC каналов (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_esp32s3_hardware_usage(float *usb_usage, float *crypto_usage,
                                                   float *ai_usage, float *adc_usage, float *dac_usage);

/**
 * @brief Оптимизация для максимального использования возможностей ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_for_esp32s3(void);

/**
 * @brief Получение детальной информации о ядрах процессора
 * @param core0_usage Загрузка ядра 0 (%)
 * @param core1_usage Загрузка ядра 1 (%)
 * @param core0_temp Температура ядра 0 (°C)
 * @param core1_temp Температура ядра 1 (°C)
 * @param core0_frequency Частота ядра 0 (MHz)
 * @param core1_frequency Частота ядра 1 (MHz)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_core_details(float *core0_usage, float *core1_usage,
                                        float *core0_temp, float *core1_temp,
                                        uint32_t *core0_frequency, uint32_t *core1_frequency);

/**
 * @brief Рекомендация распределения задач по ядрам ESP32S3
 * @param task_name Имя задачи
 * @param recommended_core Рекомендуемое ядро (0 или 1)
 * @param reasoning Обоснование рекомендации (буфер минимум 128 байт)
 * @param max_reasoning_length Максимальная длина буфера обоснования
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_task_core_recommendation(const char *task_name, uint8_t *recommended_core,
                                                    char *reasoning, size_t max_reasoning_length);

/**
 * @brief Включение мониторинга качества беспроводной связи
 * @param enable Включить мониторинг качества связи
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_wireless_quality_monitoring(bool enable);

/**
 * @brief Получение оценки качества беспроводной связи
 * @param wifi_quality Качество WiFi (0-100)
 * @param ble_quality Качество BLE (0-100)
 * @param connection_stability Стабильность соединения (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_wireless_quality(uint8_t *wifi_quality, uint8_t *ble_quality,
                                            uint8_t *connection_stability);

/**
 * @brief Автоматическая оптимизация беспроводных соединений
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_wireless_connections(void);

/**
 * @brief Получение комплексного отчета о производительности для отладки
 * @param report Буфер для отчета (минимум 2048 байт)
 * @param max_length Максимальная длина буфера
 * @param include_raw_data Включить сырые данные
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_comprehensive_report(char *report, size_t max_length, bool include_raw_data);

/**
 * @brief Включение мониторинга в режиме реального времени для мобильного приложения
 * @param enable Включить мониторинг в реальном времени
 * @param update_interval_ms Интервал обновления (мс)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_mobile_realtime_monitoring(bool enable, uint32_t update_interval_ms);

/**
 * @brief Получение данных мониторинга в формате для мобильного приложения
 * @param json_data Буфер для JSON данных (минимум 512 байт)
 * @param buffer_size Размер буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_mobile_data(char *json_data, size_t buffer_size);

/**
 * @brief Автоматическая настройка параметров мониторинга на основе анализа нагрузки
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_configure_monitoring(void);

/**
 * @brief Получение оценки эффективности использования ресурсов ESP32S3
 * @param resource_utilization_score Оценка использования ресурсов (0-100)
 * @param efficiency_score Оценка эффективности (0-100)
 * @param optimization_potential Потенциал оптимизации (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_resource_efficiency(uint8_t *resource_utilization_score,
                                               uint8_t *efficiency_score,
                                               uint8_t *optimization_potential);

/**
 * @brief Включение мониторинга тепловых характеристик для предотвращения перегрева
 * @param enable Включить термический мониторинг
 * @param thermal_shutdown_temp Температура отключения (°C)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_thermal_protection(bool enable, float thermal_shutdown_temp);

/**
 * @brief Получение детальной информации о распределении памяти по компонентам
 * @param memory_breakdown Буфер для разбивки памяти (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_memory_breakdown(char *memory_breakdown, size_t max_length);

/**
 * @brief Оптимизация для работы с дисплеем и графикой
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_for_display(void);

/**
 * @brief Получение рекомендаций по настройке FreeRTOS для ESP32S3
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_freertos_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Включение мониторинга в минимальном режиме для экономии ресурсов
 * @param enable Включить минимальный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_minimal_monitoring(bool enable);

/**
 * @brief Получение статистики производительности в человеко-читаемом формате
 * @param formatted_stats Буфер для форматированных данных (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_formatted_stats(char *formatted_stats, size_t max_length);

/**
 * @brief Анализ трендов производительности за указанный период
 * @param hours Период анализа (часы)
 * @param trends Буфер для трендов (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_analyze_performance_trends(uint32_t hours, char *trends, size_t max_length);

/**
 * @brief Получение оценки стабильности системы
 * @param stability_score Оценка стабильности (0-100)
 * @param uptime_days Время безотказной работы (дни)
 * @param error_rate Количество ошибок в час
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_stability_metrics(uint8_t *stability_score, uint32_t *uptime_days, float *error_rate);

/**
 * @brief Автоматическая балансировка нагрузки между ядрами ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_balance_core_load(void);

/**
 * @brief Получение рекомендаций по настройке для гидропонного мониторинга
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_hydroponics_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Включение мониторинга с минимальным влиянием на производительность
 * @param enable Включить легковесный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_lightweight_monitoring(bool enable);

/**
 * @brief Получение детальной информации о производительности для отладки
 * @param debug_info Буфер для отладочной информации (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_debug_info(char *debug_info, size_t max_length);

/**
 * @brief Автоматическая оптимизация на основе машинного обучения (простая эвристика)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_optimize_with_ml(void);

/**
 * @brief Получение оценки общей эффективности системы для гидропонного мониторинга
 * @param overall_efficiency Общая эффективность (0-100)
 * @param sensor_efficiency Эффективность датчиков (0-100)
 * @param control_efficiency Эффективность управления (0-100)
 * @param ui_efficiency Эффективность интерфейса (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_hydroponics_efficiency(uint8_t *overall_efficiency, uint8_t *sensor_efficiency,
                                                  uint8_t *control_efficiency, uint8_t *ui_efficiency);

/**
 * @brief Включение мониторинга в режиме реального времени с минимальной задержкой
 * @param enable Включить высокоточный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_high_precision_monitoring(bool enable);

/**
 * @brief Получение комплексного анализа производительности для оптимизации
 * @param analysis Буфер для анализа (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_performance_analysis(char *analysis, size_t max_length);

/**
 * @brief Автоматическая корректировка параметров для достижения целевой производительности
 * @param target_cpu_usage Целевая загрузка CPU (%)
 * @param target_memory_usage Целевое использование памяти (%)
 * @param target_temperature Целевая температура (°C)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_tune_to_targets(float target_cpu_usage, float target_memory_usage, float target_temperature);

/**
 * @brief Получение оценки качества кода и архитектуры системы
 * @param code_quality_score Оценка качества кода (0-100)
 * @param architecture_score Оценка архитектуры (0-100)
 * @param maintainability_score Оценка поддерживаемости (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_code_quality_score(uint8_t *code_quality_score, uint8_t *architecture_score, uint8_t *maintainability_score);

/**
 * @brief Включение мониторинга для выявления узких мест в реальном времени
 * @param enable Включить мониторинг узких мест
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_bottleneck_detection(bool enable);

/**
 * @brief Получение списка выявленных узких мест производительности
 * @param bottlenecks Буфер для узких мест (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество выявленных узких мест
 */
int system_monitor_get_detected_bottlenecks(char *bottlenecks, size_t max_length);

/**
 * @brief Автоматическое устранение выявленных узких мест
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_fix_bottlenecks(void);

/**
 * @brief Получение оценки готовности системы к продакшену
 * @param production_readiness_score Оценка готовности к продакшену (0-100)
 * @param reliability_score Оценка надежности (0-100)
 * @param performance_score Оценка производительности (0-100)
 * @param security_score Оценка безопасности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_production_readiness(uint8_t *production_readiness_score, uint8_t *reliability_score,
                                                uint8_t *performance_score, uint8_t *security_score);

/**
 * @brief Включение мониторинга безопасности системы
 * @param enable Включить мониторинг безопасности
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_security_monitoring(bool enable);

/**
 * @brief Получение оценки безопасности беспроводных соединений
 * @param wifi_security_score Оценка безопасности WiFi (0-100)
 * @param ble_security_score Оценка безопасности BLE (0-100)
 * @param encryption_score Оценка шифрования (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_security_score(uint8_t *wifi_security_score, uint8_t *ble_security_score, uint8_t *encryption_score);

/**
 * @brief Автоматическая оптимизация безопасности соединений
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_security_settings(void);

/**
 * @brief Получение комплексной оценки системы для гидропонного мониторинга
 * @param overall_score Общая оценка (0-100)
 * @param hardware_score Оценка аппаратной части (0-100)
 * @param software_score Оценка программной части (0-100)
 * @param integration_score Оценка интеграции (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_hydroponics_system_score(uint8_t *overall_score, uint8_t *hardware_score,
                                                    uint8_t *software_score, uint8_t *integration_score);

/**
 * @brief Включение мониторинга в режиме максимальной производительности
 * @param enable Включить высокопроизводительный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_high_performance_monitoring(bool enable);

/**
 * @brief Получение детальной информации о производительности для профессиональной диагностики
 * @param professional_report Буфер для профессионального отчета (минимум 2048 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_professional_report(char *professional_report, size_t max_length);

/**
 * @brief Автоматическая оптимизация для достижения максимальной эффективности ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_maximize_esp32s3_efficiency(void);

/**
 * @brief Получение оценки использования всех возможностей ESP32S3
 * @param dual_core_usage Использование двухъядерности (%)
 * @param psram_usage Использование PSRAM (%)
 * @param crypto_usage Использование криптоакселератора (%)
 * @param usb_usage Использование USB (%)
 * @param ai_usage Использование AI акселератора (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_esp32s3_feature_usage(float *dual_core_usage, float *psram_usage,
                                                  float *crypto_usage, float *usb_usage, float *ai_usage);

/**
 * @brief Рекомендация полной оптимизации системы под ESP32S3
 * @param optimization_plan Буфер для плана оптимизации (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_comprehensive_optimization_plan(char *optimization_plan, size_t max_length);

/**
 * @brief Включение мониторинга в режиме минимального потребления ресурсов
 * @param enable Включить ультра-легковесный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_ultra_lightweight_monitoring(bool enable);

/**
 * @brief Получение финальной оценки производительности для гидропонного мониторинга
 * @param final_score Финальная оценка (0-100)
 * @param performance_grade Буквенная оценка (A-F)
 * @param recommendations Рекомендации по улучшению (буфер минимум 512 байт)
 * @param max_recommendations_length Максимальная длина буфера рекомендаций
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_final_assessment(uint8_t *final_score, char *performance_grade,
                                            char *recommendations, size_t max_recommendations_length);

/**
 * @brief Включение мониторинга в режиме реального времени для критически важных метрик
 * @param enable Включить критический мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_critical_monitoring(bool enable);

/**
 * @brief Получение оценки надежности системы для длительной работы
 * @param reliability_score Оценка надежности (0-100)
 * @param mean_time_between_failures MTBF в часах
 * @param availability_score Оценка доступности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_reliability_metrics(uint8_t *reliability_score, uint32_t *mean_time_between_failures,
                                               uint8_t *availability_score);

/**
 * @brief Автоматическая оптимизация надежности системы
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_reliability(void);

/**
 * @brief Получение оценки готовности системы к мобильному приложению
 * @param mobile_readiness_score Оценка готовности к мобильному приложению (0-100)
 * @param api_compatibility_score Оценка совместимости API (0-100)
 * @param connectivity_score Оценка качества соединения (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_mobile_readiness(uint8_t *mobile_readiness_score, uint8_t *api_compatibility_score,
                                            uint8_t *connectivity_score);

/**
 * @brief Включение мониторинга в режиме максимальной детализации для отладки
 * @param enable Включить детальный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_detailed_monitoring(bool enable);

/**
 * @brief Получение ультра-детальной диагностики системы для профессионального анализа
 * @param ultra_detailed_report Буфер для детального отчета (минимум 4096 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_get_ultra_detailed_report(char *ultra_detailed_report, size_t max_length);

/**
 * @brief Автоматическая оптимизация всей системы для достижения максимальной производительности ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_comprehensive_auto_optimization(void);

/**
 * @brief Получение оценки использования полного потенциала ESP32S3
 * @param potential_utilization_score Оценка использования потенциала (0-100)
 * @param dual_core_efficiency_score Эффективность использования двух ядер (0-100)
 * @param psram_efficiency_score Эффективность использования PSRAM (0-100)
 * @param peripheral_efficiency_score Эффективность использования периферии (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_esp32s3_potential_utilization(uint8_t *potential_utilization_score,
                                                          uint8_t *dual_core_efficiency_score,
                                                          uint8_t *psram_efficiency_score,
                                                          uint8_t *peripheral_efficiency_score);

/**
 * @brief Генерация полного отчета о производительности для гидропонного мониторинга
 * @param full_report Буфер для полного отчета (минимум 4096 байт)
 * @param max_length Максимальная длина буфера
 * @param include_hardware_details Включить детали аппаратного обеспечения
 * @param include_software_details Включить детали программного обеспечения
 * @param include_optimization_suggestions Включить предложения по оптимизации
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_generate_comprehensive_hydroponics_report(char *full_report, size_t max_length,
                                                           bool include_hardware_details,
                                                           bool include_software_details,
                                                           bool include_optimization_suggestions);

/**
 * @brief Включение мониторинга в режиме минимального влияния на гидропонные операции
 * @param enable Включить гидропонный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_hydroponics_monitoring(bool enable);

/**
 * @brief Получение оценки производительности специально для гидропонного мониторинга
 * @param hydroponics_performance_score Оценка производительности гидропоники (0-100)
 * @param sensor_response_time Время отклика датчиков (мс)
 * @param control_loop_frequency Частота цикла управления (Гц)
 * @param ui_responsiveness Отзывчивость интерфейса (мс)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_hydroponics_performance(uint8_t *hydroponics_performance_score,
                                                   uint32_t *sensor_response_time,
                                                   float *control_loop_frequency,
                                                   uint32_t *ui_responsiveness);

/**
 * @brief Автоматическая оптимизация для гидропонного мониторинга
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_optimize_for_hydroponics(void);

/**
 * @brief Получение оценки энергоэффективности для автономной работы
 * @param energy_efficiency_score Оценка энергоэффективности (0-100)
 * @param battery_life_hours Расчетное время работы от батареи (часы)
 * @param power_consumption_breakdown Разбивка энергопотребления (буфер минимум 256 байт)
 * @param max_breakdown_length Максимальная длина буфера разбивки
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_energy_efficiency(uint8_t *energy_efficiency_score,
                                             uint32_t *battery_life_hours,
                                             char *power_consumption_breakdown,
                                             size_t max_breakdown_length);

/**
 * @brief Включение мониторинга в режиме максимальной экономии энергии
 * @param enable Включить энергосберегающий мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_energy_saving_monitoring(bool enable);

/**
 * @brief Получение оценки качества мониторинга окружающей среды
 * @param environmental_monitoring_score Оценка мониторинга окружающей среды (0-100)
 * @param sensor_accuracy_score Оценка точности датчиков (0-100)
 * @param data_collection_efficiency Эффективность сбора данных (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_environmental_monitoring_quality(uint8_t *environmental_monitoring_score,
                                                            uint8_t *sensor_accuracy_score,
                                                            uint8_t *data_collection_efficiency);

/**
 * @brief Автоматическая калибровка мониторинга для достижения максимальной точности
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_auto_calibrate_monitoring(void);

/**
 * @brief Получение оценки готовности системы к промышленному использованию
 * @param industrial_readiness_score Оценка готовности к промышленному использованию (0-100)
 * @param scalability_score Оценка масштабируемости (0-100)
 * @param maintainability_score Оценка поддерживаемости (0-100)
 * @param cost_effectiveness_score Оценка экономической эффективности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_industrial_readiness(uint8_t *industrial_readiness_score,
                                                uint8_t *scalability_score,
                                                uint8_t *maintainability_score,
                                                uint8_t *cost_effectiveness_score);

/**
 * @brief Включение мониторинга в режиме максимальной надежности для критически важных систем
 * @param enable Включить надежный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_reliable_monitoring(bool enable);

/**
 * @brief Получение комплексной оценки системы гидропонного мониторинга
 * @param overall_assessment Общая оценка (0-100)
 * @param hardware_assessment Оценка аппаратного обеспечения (0-100)
 * @param software_assessment Оценка программного обеспечения (0-100)
 * @param operational_assessment Оценка операционной готовности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_comprehensive_hydroponics_assessment(uint8_t *overall_assessment,
                                                                uint8_t *hardware_assessment,
                                                                uint8_t *software_assessment,
                                                                uint8_t *operational_assessment);

/**
 * @brief Генерация итогового отчета о производительности ESP32S3 для гидропонного мониторинга
 * @param final_report Буфер для итогового отчета (минимум 2048 байт)
 * @param max_length Максимальная длина буфера
 * @param include_all_metrics Включить все метрики
 * @param include_optimization_plan Включить план оптимизации
 * @return Количество записанных байт или -1 в случае ошибки
 */
int system_monitor_generate_final_esp32s3_hydroponics_report(char *final_report, size_t max_length,
                                                            bool include_all_metrics,
                                                            bool include_optimization_plan);

/**
 * @brief Включение мониторинга в режиме максимальной эффективности для ESP32S3
 * @param enable Включить эффективный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_esp32s3_efficient_monitoring(bool enable);

/**
 * @brief Получение финальной оценки использования потенциала ESP32S3 в гидропонном мониторинге
 * @param esp32s3_utilization_score Оценка использования ESP32S3 (0-100)
 * @param dual_core_utilization_score Использование двухъядерности (0-100)
 * @param memory_optimization_score Оптимизация памяти (0-100)
 * @param peripheral_utilization_score Использование периферии (0-100)
 * @param energy_efficiency_score Энергоэффективность (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_esp32s3_utilization_assessment(uint8_t *esp32s3_utilization_score,
                                                          uint8_t *dual_core_utilization_score,
                                                          uint8_t *memory_optimization_score,
                                                          uint8_t *peripheral_utilization_score,
                                                          uint8_t *energy_efficiency_score);

/**
 * @brief Автоматическая финальная оптимизация всей системы
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_final_auto_optimization(void);

/**
 * @brief Получение оценки готовности к развертыванию в продакшене
 * @param production_deployment_score Оценка готовности к развертыванию (0-100)
 * @param stability_score Оценка стабильности (0-100)
 * @param performance_consistency_score Оценка последовательности производительности (0-100)
 * @param error_resilience_score Оценка устойчивости к ошибкам (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_deployment_readiness(uint8_t *production_deployment_score,
                                               uint8_t *stability_score,
                                               uint8_t *performance_consistency_score,
                                               uint8_t *error_resilience_score);

/**
 * @brief Включение мониторинга в режиме максимальной детализации для финальной оценки
 * @param enable Включить детальный мониторинг для оценки
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_enable_final_assessment_monitoring(bool enable);

/**
 * @brief Получение окончательной оценки производительности гидропонного мониторинга на ESP32S3
 * @param final_performance_score Финальная оценка производительности (0-100)
 * @param final_grade Финальная буквенная оценка (A-F)
 * @param final_summary Финальное резюме (буфер минимум 512 байт)
 * @param max_summary_length Максимальная длина буфера резюме
 * @return ESP_OK в случае успеха
 */
esp_err_t system_monitor_get_final_performance_assessment(uint8_t *final_performance_score,
                                                        char *final_grade,
                                                        char *final_summary,
                                                        size_t max_summary_length);
