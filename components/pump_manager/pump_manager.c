/**
 * @file pump_manager.c
 * @brief Реализация менеджера насосов с PID контроллером
 */

#include "pump_manager.h"
#include "peristaltic_pump.h"
#include "config_manager.h"
#include "data_logger.h"
#include "notification_system.h"
#include "adaptive_pid.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>
#include <time.h>

static const char *TAG = "PUMP_MANAGER";

// GPIO пины для насосов (из system_config.h)
// Каждый насос управляется одним GPIO через оптопару
static const int PUMP_PINS[PUMP_INDEX_COUNT] = {
    PUMP_PH_UP_PIN,
    PUMP_PH_DOWN_PIN,
    PUMP_EC_A_PIN,
    PUMP_EC_B_PIN,
    PUMP_EC_C_PIN,
    PUMP_WATER_PIN
};

// Глобальные данные
static pid_controller_t g_pid_controllers[PUMP_INDEX_COUNT];
static pump_stats_t g_pump_stats[PUMP_INDEX_COUNT];
static SemaphoreHandle_t g_pump_mutexes[PUMP_INDEX_COUNT];
static pump_config_t g_pump_configs[PUMP_INDEX_COUNT];
static pid_config_t g_pid_configs[PUMP_INDEX_COUNT];
static TaskHandle_t g_pump_manager_task_handle = NULL;
static bool g_initialized = false;

// Для cooldown таймеров
static uint64_t g_last_run_times[PUMP_INDEX_COUNT] = {0};

/**
 * @brief Получить текущее время в микросекундах
 */
static inline uint64_t get_time_us(void) {
    return esp_timer_get_time();
}

/**
 * @brief Получить текущее время в миллисекундах
 */
static inline uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

/**
 * @brief Проверка cooldown таймера
 */
static bool check_cooldown(pump_index_t pump_idx) {
    uint64_t now = get_time_ms();
    uint64_t elapsed = now - g_last_run_times[pump_idx];
    return elapsed >= g_pid_configs[pump_idx].cooldown_time_ms;
}

/**
 * @brief Проверка суточного лимита
 */
static bool check_daily_limit(pump_index_t pump_idx, float dose_ml) {
    float new_volume = g_pump_stats[pump_idx].daily_volume_ml + dose_ml;
    return new_volume <= g_pid_configs[pump_idx].max_daily_volume;
}

/**
 * @brief Выполнение PID расчета с использованием конфига
 */
static void compute_pid_internal(pump_index_t pump_idx, pid_controller_t *pid, float current, float target, pid_output_t *output) {
    uint64_t now_us = get_time_us();
    float dt = (now_us - pid->last_time_us) / 1000000.0f; // в секундах
    
    if (dt < 0.001f) dt = 0.001f; // Минимум 1 мс
    
    // Получение конфигурации
    pid_config_t *config = &g_pid_configs[pump_idx];
    
    // Расчет ошибки
    float error = target - current;
    output->error = error;
    
    // P-компонента
    output->p_term = pid->kp * error;
    
    // I-компонента с anti-windup и auto_reset
    if (config->auto_reset_integral && (fabsf(pid->prev_error) > 0.001f)) {
        // Сброс интеграла при смене знака ошибки
        if ((pid->prev_error > 0 && error < 0) || (pid->prev_error < 0 && error > 0)) {
            pid->integral = 0;
            ESP_LOGD(TAG, "PID %d: auto reset integral (смена знака)", pump_idx);
        }
    }
    
    pid->integral += error * dt;
    
    // Ограничение интеграла по integral_max
    float integral_limit = config->integral_max;
    if (pid->integral > integral_limit) {
        pid->integral = integral_limit;
    } else if (pid->integral < -integral_limit) {
        pid->integral = -integral_limit;
    }
    
    output->i_term = pid->ki * pid->integral;
    
    // D-компонента с опциональным фильтром
    float derivative = (error - pid->prev_error) / dt;
    if (config->use_derivative_filter) {
        // Простой фильтр низких частот (alpha = 0.5)
        derivative = (derivative + pid->prev_derivative) * 0.5f;
        pid->prev_derivative = derivative;
    }
    output->d_term = pid->kd * derivative;
    
    // Общий выход
    output->output = output->p_term + output->i_term + output->d_term;
    
    // Ограничение выхода
    if (output->output > pid->output_max) {
        output->output = pid->output_max;
    } else if (output->output < pid->output_min) {
        output->output = pid->output_min;
    }
    
    // Если выход отрицательный и мы управляем насосом (не может качать назад), обнулить
    if (output->output < 0) {
        output->output = 0;
    }
    
    // Обновление состояния
    pid->prev_error = error;
    pid->last_time_us = now_us;
}

/**
 * @brief Запуск насоса с 3 попытками
 */
static esp_err_t run_pump_with_retry(pump_index_t pump_idx, uint32_t duration_ms) {
    const int MAX_RETRIES = 3;
    
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        ESP_LOGD(TAG, "Запуск насоса %s, попытка %d/%d, длительность %lu мс",
                 PUMP_NAMES[pump_idx], attempt + 1, MAX_RETRIES, (unsigned long)duration_ms);
        
        // Попытка запуска через оптопару (один пин)
        pump_run_ms(PUMP_PINS[pump_idx], duration_ms);
        
        // Проверка успешности (в данной реализации pump_run_ms не возвращает ошибки)
        // Считаем успешным если не было исключения
        
        // Обновление статистики
        g_pump_stats[pump_idx].total_runs++;
        g_pump_stats[pump_idx].total_time_ms += duration_ms;
        g_pump_stats[pump_idx].last_run_time = get_time_ms();
        g_last_run_times[pump_idx] = get_time_ms();
        
        ESP_LOGD(TAG, "Насос %s успешно запущен", PUMP_NAMES[pump_idx]);
        return ESP_OK;
    }
    
    // Все попытки неудачны
    ESP_LOGE(TAG, "Не удалось запустить насос %s после %d попыток", PUMP_NAMES[pump_idx], MAX_RETRIES);
    
    // Критическое уведомление
    char msg[128];
    snprintf(msg, sizeof(msg), "Ошибка насоса %s!", PUMP_NAMES[pump_idx]);
    notification_create(NOTIF_TYPE_CRITICAL, NOTIF_PRIORITY_URGENT, NOTIF_SOURCE_PUMP, msg);
    
    // Отключение PID для этого насоса
    g_pid_controllers[pump_idx].enabled = false;
    g_pid_configs[pump_idx].enabled = false;
    
    // Логирование ошибки
    data_logger_log_pid_correction(pump_idx, 0, 0, 0, 0, 0, 0, "PUMP_FAILURE");
    
    return ESP_FAIL;
}

/**
 * @brief Задача мониторинга pump_manager
 */
static void pump_manager_task(void *pvParameters) {
    const TickType_t TASK_DELAY = pdMS_TO_TICKS(60000); // 1 минута
    uint32_t nvs_save_counter = 0;
    uint32_t pid_log_flush_counter = 0;
    
    ESP_LOGI(TAG, "Задача pump_manager запущена");
    
    while (1) {
        vTaskDelay(TASK_DELAY);
        
        nvs_save_counter++;
        pid_log_flush_counter++;
        
        // Сохранение суточных счетчиков в NVS каждые 10 минут
        if (nvs_save_counter >= 10) {
            nvs_save_counter = 0;
            ESP_LOGI(TAG, "Сохранение суточных счетчиков в NVS");
            
            // TODO: Сохранение daily_volume в NVS (требует расширения config_manager)
            // Пока просто логируем
            for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
                ESP_LOGD(TAG, "Насос %s: суточный объем = %.2f мл", 
                         PUMP_NAMES[i], g_pump_stats[i].daily_volume_ml);
            }
        }
        
        // Flush PID логов каждые 5 минут
        if (pid_log_flush_counter >= 5) {
            pid_log_flush_counter = 0;
            ESP_LOGI(TAG, "Flush PID логов на SD");
            data_logger_flush_pid_logs();
        }
        
        // Проверка полуночи для сброса суточных счетчиков
        time_t now_time;
        struct tm timeinfo;
        time(&now_time);
        localtime_r(&now_time, &timeinfo);
        
        if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
            ESP_LOGI(TAG, "Полночь: сброс суточных счетчиков");
            
            for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
                if (xSemaphoreTake(g_pump_mutexes[i], pdMS_TO_TICKS(5000)) == pdTRUE) {
                    // Сохранение старого значения на SD
                    data_logger_log_pump_stats(i, g_pump_stats[i].daily_volume_ml, 0);
                    
                    // Сброс счетчика
                    g_pump_stats[i].daily_volume_ml = 0;
                    g_pump_stats[i].daily_reset_time = get_time_ms();
                    
                    xSemaphoreGive(g_pump_mutexes[i]);
                } else {
                    ESP_LOGW(TAG, "Не удалось взять мьютекс для насоса %s при сбросе счетчика", 
                             PUMP_NAMES[i]);
                }
            }
            
            // Задержка, чтобы не сбрасывать несколько раз
            vTaskDelay(pdMS_TO_TICKS(120000)); // 2 минуты
        }
    }
}

esp_err_t pump_manager_init(void) {
    if (g_initialized) {
        ESP_LOGW(TAG, "pump_manager уже инициализирован");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Инициализация pump_manager...");
    
    // Создание мьютексов для каждого насоса
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        g_pump_mutexes[i] = xSemaphoreCreateMutex();
        if (g_pump_mutexes[i] == NULL) {
            ESP_LOGE(TAG, "Не удалось создать мьютекс для насоса %d", i);
            
            // Освобождение всех ранее созданных мьютексов
            for (int j = 0; j < i; j++) {
                if (g_pump_mutexes[j] != NULL) {
                    vSemaphoreDelete(g_pump_mutexes[j]);
                    g_pump_mutexes[j] = NULL;
                }
            }
            
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Инициализация насосов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pump_init(PUMP_PINS[i]);
        
        // Инициализация PID контроллеров с дефолтными значениями
        memset(&g_pid_controllers[i], 0, sizeof(pid_controller_t));
        g_pid_controllers[i].kp = 1.0f;
        g_pid_controllers[i].ki = 0.1f;
        g_pid_controllers[i].kd = 0.0f;
        g_pid_controllers[i].output_min = 1.0f;
        g_pid_controllers[i].output_max = 50.0f;
        g_pid_controllers[i].enabled = false;
        g_pid_controllers[i].last_time_us = get_time_us();
        
        // Инициализация статистики
        memset(&g_pump_stats[i], 0, sizeof(pump_stats_t));
        g_pump_stats[i].daily_reset_time = get_time_ms();
        
        // Инициализация конфигурации насосов
        g_pump_configs[i].enabled = true;
        g_pump_configs[i].flow_rate_ml_per_sec = 10.0f;
        g_pump_configs[i].min_duration_ms = 100;
        g_pump_configs[i].max_duration_ms = 30000;
        g_pump_configs[i].cooldown_ms = 60000;
        g_pump_configs[i].concentration_factor = 1.0f;
        strncpy(g_pump_configs[i].name, PUMP_NAMES[i], sizeof(g_pump_configs[i].name) - 1);
        
        // Инициализация PID конфигурации
        memset(&g_pid_configs[i], 0, sizeof(pid_config_t));
        g_pid_configs[i].kp = 1.0f;
        g_pid_configs[i].ki = 0.1f;
        g_pid_configs[i].kd = 0.0f;
        g_pid_configs[i].output_min = 1.0f;
        g_pid_configs[i].output_max = 50.0f;
        g_pid_configs[i].deadband = 0.05f;
        g_pid_configs[i].integral_max = 100.0f;
        g_pid_configs[i].sample_time_ms = 5000;
        g_pid_configs[i].max_dose_per_cycle = 10.0f;
        g_pid_configs[i].cooldown_time_ms = 60000;
        g_pid_configs[i].max_daily_volume = 500;
        g_pid_configs[i].enabled = false;
        g_pid_configs[i].auto_reset_integral = true;
        g_pid_configs[i].use_derivative_filter = false;
    }
    
    // Создание задачи мониторинга
    BaseType_t result = xTaskCreate(
        pump_manager_task,
        "pump_mgr_task",
        3072,
        NULL,
        8, // Приоритет 8
        &g_pump_manager_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу pump_manager");
        return ESP_FAIL;
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "pump_manager успешно инициализирован");
    
    return ESP_OK;
}

esp_err_t pump_manager_set_pid_tunings(pump_index_t pump_idx, float kp, float ki, float kd) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_pid_controllers[pump_idx].kp = kp;
    g_pid_controllers[pump_idx].ki = ki;
    g_pid_controllers[pump_idx].kd = kd;
    
    g_pid_configs[pump_idx].kp = kp;
    g_pid_configs[pump_idx].ki = ki;
    g_pid_configs[pump_idx].kd = kd;
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "PID настройки для %s: Kp=%.2f Ki=%.2f Kd=%.2f", 
             PUMP_NAMES[pump_idx], kp, ki, kd);
    
    return ESP_OK;
}

esp_err_t pump_manager_compute_pid(pump_index_t pump_idx, float current, float target, pid_output_t *output) {
    if (pump_idx >= PUMP_INDEX_COUNT || output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    compute_pid_internal(pump_idx, &g_pid_controllers[pump_idx], current, target, output);
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_compute_and_execute(pump_index_t pump_idx, float current, float target) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "%s: Не удалось взять мьютекс (timeout)", PUMP_NAMES[pump_idx]);
        return ESP_ERR_TIMEOUT;
    }
    
    // Проверка enabled
    if (!g_pid_configs[pump_idx].enabled || !g_pid_controllers[pump_idx].enabled) {
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        ESP_LOGD(TAG, "PID для %s отключен", PUMP_NAMES[pump_idx]);
        return ESP_OK;
    }
    
    // Проверка deadband
    float error = fabsf(target - current);
    if (error < g_pid_configs[pump_idx].deadband) {
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        ESP_LOGD(TAG, "%s: в пределах deadband (%.3f < %.3f)", 
                 PUMP_NAMES[pump_idx], error, g_pid_configs[pump_idx].deadband);
        return ESP_OK;
    }
    
    // Проверка activation threshold (запуск PID только при большом отклонении)
    if (error < g_pid_configs[pump_idx].activation_threshold) {
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        ESP_LOGD(TAG, "%s: ниже порога активации (%.3f < %.3f)", 
                 PUMP_NAMES[pump_idx], error, g_pid_configs[pump_idx].activation_threshold);
        return ESP_OK;
    }
    
    // Проверка cooldown
    if (!check_cooldown(pump_idx)) {
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        ESP_LOGD(TAG, "%s: ожидание cooldown", PUMP_NAMES[pump_idx]);
        return ESP_OK;
    }
    
    // Расчет PID
    pid_output_t output;
    compute_pid_internal(pump_idx, &g_pid_controllers[pump_idx], current, target, &output);
    
    // Проверка минимального выхода
    if (output.output < g_pid_configs[pump_idx].output_min) {
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        ESP_LOGD(TAG, "%s: выход слишком мал (%.2f < %.2f)", 
                 PUMP_NAMES[pump_idx], output.output, g_pid_configs[pump_idx].output_min);
        return ESP_OK;
    }
    
    // Ограничение по max_dose_per_cycle
    if (output.output > g_pid_configs[pump_idx].max_dose_per_cycle) {
        output.output = g_pid_configs[pump_idx].max_dose_per_cycle;
    }
    
    // Проверка суточного лимита
    if (!check_daily_limit(pump_idx, output.output)) {
        ESP_LOGW(TAG, "%s: превышен суточный лимит!", PUMP_NAMES[pump_idx]);
        
        // Критическое уведомление
        char msg[128];
        snprintf(msg, sizeof(msg), "Лимит насоса %s превышен!", PUMP_NAMES[pump_idx]);
        notification_create(NOTIF_TYPE_CRITICAL, NOTIF_PRIORITY_URGENT, NOTIF_SOURCE_PUMP, msg);
        
        // Логирование
        data_logger_log_pid_correction(pump_idx, target, current, 
                                        output.p_term, output.i_term, output.d_term, 
                                        output.output, "DAILY_LIMIT_EXCEEDED");
        
        // Блокировка PID
        g_pid_controllers[pump_idx].enabled = false;
        g_pid_configs[pump_idx].enabled = false;
        
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
        return ESP_FAIL;
    }
    
    // Конвертация мл в мс (защита от деления на 0)
    float flow_rate = g_pump_configs[pump_idx].flow_rate_ml_per_sec;
    if (flow_rate < 0.001f) {
        ESP_LOGE(TAG, "%s: flow_rate слишком мал (%.3f), используется 0.1", 
                 PUMP_NAMES[pump_idx], flow_rate);
        flow_rate = 0.1f;
    }
    uint32_t duration_ms = (uint32_t)((output.output / flow_rate) * 1000.0f);
    
    // Ограничение длительности
    if (duration_ms < g_pump_configs[pump_idx].min_duration_ms) {
        duration_ms = g_pump_configs[pump_idx].min_duration_ms;
    }
    if (duration_ms > g_pump_configs[pump_idx].max_duration_ms) {
        duration_ms = g_pump_configs[pump_idx].max_duration_ms;
    }
    
    ESP_LOGD(TAG, "%s: PID коррекция - Текущ=%.2f Цель=%.2f P=%.2f I=%.2f D=%.2f Выход=%.2f мл (%lu мс)",
             PUMP_NAMES[pump_idx], current, target, 
             output.p_term, output.i_term, output.d_term, 
             output.output, (unsigned long)duration_ms);
    
    // КРИТИЧНО: Не отпускаем мьютекс до завершения операции!
    // Запуск насоса с retry (выполняется ПОД мьютексом для атомарности)
    esp_err_t result = run_pump_with_retry(pump_idx, duration_ms);
    
    // Обновление суточного объема (уже под мьютексом)
    if (result == ESP_OK) {
        g_pump_stats[pump_idx].total_volume_ml += output.output;
        g_pump_stats[pump_idx].daily_volume_ml += output.output;
        
        // Логирование успешной коррекции
        data_logger_log_pid_correction(pump_idx, target, current,
                                        output.p_term, output.i_term, output.d_term,
                                        output.output, "OK");
    } else {
        // Логирование ошибки
        data_logger_log_pid_correction(pump_idx, target, current,
                                        output.p_term, output.i_term, output.d_term,
                                        output.output, "PUMP_ERROR");
    }
    
    // Освобождаем мьютекс ПОСЛЕ завершения всей операции
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return result;
}

esp_err_t pump_manager_get_stats(pump_index_t pump_idx, pump_stats_t *stats) {
    if (pump_idx >= PUMP_INDEX_COUNT || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &g_pump_stats[pump_idx], sizeof(pump_stats_t));
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_reset_pid(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_pid_controllers[pump_idx].integral = 0;
    g_pid_controllers[pump_idx].prev_error = 0;
    g_pid_controllers[pump_idx].last_time_us = get_time_us();
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "PID сброшен для %s", PUMP_NAMES[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_reset_daily_counter(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "Сброс суточного счетчика для %s (было %.2f мл)", 
             PUMP_NAMES[pump_idx], g_pump_stats[pump_idx].daily_volume_ml);
    
    g_pump_stats[pump_idx].daily_volume_ml = 0;
    g_pump_stats[pump_idx].daily_reset_time = get_time_ms();
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_get_daily_volume(pump_index_t pump_idx, float *volume) {
    if (pump_idx >= PUMP_INDEX_COUNT || volume == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *volume = g_pump_stats[pump_idx].daily_volume_ml;
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_apply_config(const system_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Применение конфигурации из system_config");
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        if (xSemaphoreTake(g_pump_mutexes[i], pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Копирование pump_config
            memcpy(&g_pump_configs[i], &config->pump_config[i], sizeof(pump_config_t));
            
            // Копирование pid_config
            memcpy(&g_pid_configs[i], &config->pump_pid[i], sizeof(pid_config_t));
            
            // Применение к PID контроллеру
            g_pid_controllers[i].kp = config->pump_pid[i].kp;
            g_pid_controllers[i].ki = config->pump_pid[i].ki;
            g_pid_controllers[i].kd = config->pump_pid[i].kd;
            g_pid_controllers[i].output_min = config->pump_pid[i].output_min;
            g_pid_controllers[i].output_max = config->pump_pid[i].output_max;
            g_pid_controllers[i].enabled = config->pump_pid[i].enabled;
            
            xSemaphoreGive(g_pump_mutexes[i]);
            
            ESP_LOGI(TAG, "Насос %s: Kp=%.2f Ki=%.2f Kd=%.2f enabled=%d",
                     PUMP_NAMES[i], config->pump_pid[i].kp, config->pump_pid[i].ki, 
                     config->pump_pid[i].kd, config->pump_pid[i].enabled);
        }
    }
    
    return ESP_OK;
}

esp_err_t pump_manager_run_direct(pump_index_t pump_idx, uint32_t duration_ms) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Прямой запуск насоса %s на %lu мс", 
             PUMP_NAMES[pump_idx], (unsigned long)duration_ms);
    
    // Прямой запуск без PID и большинства проверок (через оптопару)
    pump_run_ms(PUMP_PINS[pump_idx], duration_ms);
    
    // Обновление статистики
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_pump_stats[pump_idx].total_runs++;
        g_pump_stats[pump_idx].total_time_ms += duration_ms;
        g_pump_stats[pump_idx].last_run_time = get_time_ms();
        
        xSemaphoreGive(g_pump_mutexes[pump_idx]);
    }
    
    return ESP_OK;
}

esp_err_t pump_manager_get_pid_tunings(pump_index_t pump_idx, float *kp, float *ki, float *kd) {
    if (pump_idx >= PUMP_INDEX_COUNT || kp == NULL || ki == NULL || kd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *kp = g_pid_controllers[pump_idx].kp;
    *ki = g_pid_controllers[pump_idx].ki;
    *kd = g_pid_controllers[pump_idx].kd;
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pump_manager_run_with_dose(pump_index_t pump_idx, float dose_ml) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Конвертация мл в мс
    float flow_rate = g_pump_configs[pump_idx].flow_rate_ml_per_sec;
    if (flow_rate < 0.001f) flow_rate = 0.1f;
    
    uint32_t duration_ms = (uint32_t)((dose_ml / flow_rate) * 1000.0f);
    
    // Ограничение длительности
    if (duration_ms < g_pump_configs[pump_idx].min_duration_ms) {
        duration_ms = g_pump_configs[pump_idx].min_duration_ms;
    }
    if (duration_ms > g_pump_configs[pump_idx].max_duration_ms) {
        duration_ms = g_pump_configs[pump_idx].max_duration_ms;
    }
    
    ESP_LOGD(TAG, "%s: запуск с дозой %.2f мл (%lu мс)",
             PUMP_NAMES[pump_idx], dose_ml, (unsigned long)duration_ms);
    
    // Запуск насоса
    esp_err_t result = run_pump_with_retry(pump_idx, duration_ms);
    
    // Обновление статистики
    if (result == ESP_OK) {
        g_pump_stats[pump_idx].total_volume_ml += dose_ml;
        g_pump_stats[pump_idx].daily_volume_ml += dose_ml;
    }
    
    xSemaphoreGive(g_pump_mutexes[pump_idx]);
    
    return result;
}

esp_err_t pump_manager_compute_and_execute_adaptive(pump_index_t pump_idx, 
                                                     float current, 
                                                     float target) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "%s: адаптивная коррекция текущ=%.2f цель=%.2f",
             PUMP_NAMES[pump_idx], current, target);
    
    // 1. Обновить историю в adaptive_pid
    adaptive_pid_update_history(pump_idx, current);
    
    // 2. Получить предсказание
    prediction_result_t prediction;
    esp_err_t err = adaptive_pid_predict(pump_idx, current, target, &prediction);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Ошибка предсказания, используется базовый PID");
        return pump_manager_compute_and_execute(pump_idx, current, target);
    }
    
    // 3. Проверка упреждающей коррекции
    if (prediction.needs_preemptive_correction && 
        prediction.confidence > MIN_CONFIDENCE_FOR_PREDICTION) {
        
        ESP_LOGI(TAG, "%s: упреждающая коррекция! %s (уверенность=%.1f%%)",
                 PUMP_NAMES[pump_idx], prediction.recommendation, 
                 prediction.confidence * 100.0f);
        
        // Расчет дозы на основе буферной емкости
        float dose_ml;
        err = adaptive_pid_calculate_dose(pump_idx, current, target, &dose_ml);
        
        if (err == ESP_OK && dose_ml > 0.1f) {
            // Сохранить значение для обучения
            float value_before = current;
            
            // Выполнить упреждающую коррекцию
            err = pump_manager_run_with_dose(pump_idx, dose_ml);
            
            if (err == ESP_OK) {
                // Создать задачу обучения (проверка через 5 минут)
                // TODO: Реализовать задачу delayed learning
                
                ESP_LOGD(TAG, "Упреждающая коррекция выполнена: %.2f мл", dose_ml);
                return ESP_OK;
            }
        }
    }
    
    // 4. Получить адаптивные коэффициенты
    float kp_adapt, ki_adapt, kd_adapt;
    err = adaptive_pid_get_coefficients(pump_idx, &kp_adapt, &ki_adapt, &kd_adapt);
    
    if (err == ESP_OK) {
        // Временно установить адаптивные коэффициенты
        if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) == pdTRUE) {
            g_pid_controllers[pump_idx].kp = kp_adapt;
            g_pid_controllers[pump_idx].ki = ki_adapt;
            g_pid_controllers[pump_idx].kd = kd_adapt;
            
            xSemaphoreGive(g_pump_mutexes[pump_idx]);
        }
    }
    
    // 5. Обычная PID коррекция с адаптивными коэффициентами
    return pump_manager_compute_and_execute(pump_idx, current, target);
}
