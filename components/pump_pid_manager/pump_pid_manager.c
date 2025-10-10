/**
 * @file pump_pid_manager.c
 * @brief Реализация менеджера PID для насосов
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "pump_pid_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "PUMP_PID_MGR";

// Экземпляры PID для каждого насоса
static pump_pid_instance_t pid_instances[PUMP_PID_COUNT] = {0};
static SemaphoreHandle_t pid_mutex = NULL;
static bool pid_manager_initialized = false;

// Названия насосов для логирования
static const char *pump_names[PUMP_PID_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "WATER"
};

/**
 * @brief Инициализация менеджера PID
 */
esp_err_t pump_pid_manager_init(void) {
    if (pid_manager_initialized) {
        ESP_LOGW(TAG, "PID manager already initialized");
        return ESP_OK;
    }
    
    // Создание мьютекса
    if (pid_mutex == NULL) {
        pid_mutex = xSemaphoreCreateMutex();
        if (pid_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    
    // Загрузка конфигурации из NVS
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
        xSemaphoreGive(pid_mutex);
        return ESP_FAIL;
    }
    
    // Инициализация каждого PID
    for (int i = 0; i < PUMP_PID_COUNT; i++) {
        pid_config_t pid_cfg = {
            .kp = sys_config.pump_pid[i].kp,
            .ki = sys_config.pump_pid[i].ki,
            .kd = sys_config.pump_pid[i].kd,
            .setpoint = 0.0f,
            .output_min = sys_config.pump_pid[i].output_min,
            .output_max = sys_config.pump_pid[i].output_max,
            .integral_min = -100.0f,
            .integral_max = 100.0f,
        };
        
        if (pid_init(&pid_instances[i].pid, &pid_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init PID for pump %d", i);
            continue;
        }
        
        pid_instances[i].enabled = sys_config.pump_pid[i].enabled;
        pid_instances[i].auto_mode = sys_config.pump_pid[i].auto_mode;
        
        ESP_LOGI(TAG, "PID %s initialized: Kp=%.2f Ki=%.2f Kd=%.2f [%s]",
                 pump_names[i],
                 pid_cfg.kp, pid_cfg.ki, pid_cfg.kd,
                 pid_instances[i].auto_mode ? "AUTO" : "MANUAL");
    }
    
    pid_manager_initialized = true;
    ESP_LOGI(TAG, "PID manager initialized (%d controllers)", PUMP_PID_COUNT);
    
    xSemaphoreGive(pid_mutex);
    return ESP_OK;
}

/**
 * @brief Вычисление PID
 */
esp_err_t pump_pid_compute(pump_pid_index_t pump_idx, float measured_value, float target_value, pid_output_t *output) {
    if (!pid_manager_initialized || pump_idx >= PUMP_PID_COUNT || output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    
    pump_pid_instance_t *instance = &pid_instances[pump_idx];
    
    if (!instance->enabled || !instance->auto_mode) {
        xSemaphoreGive(pid_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Обновление setpoint
    pid_set_setpoint(&instance->pid, target_value);
    
    // Вычисление dt
    uint32_t current_time = esp_timer_get_time() / 1000000; // секунды
    float dt = 1.0f; // По умолчанию 1 секунда
    
    if (instance->last_compute_time > 0) {
        dt = (float)(current_time - instance->last_compute_time);
    }
    
    // Вычисление PID
    esp_err_t ret = pid_compute(&instance->pid, measured_value, dt, output);
    
    if (ret == ESP_OK) {
        instance->last_compute_time = current_time;
        instance->last_measured_value = measured_value;
        
        ESP_LOGD(TAG, "PID %s: target=%.2f, measured=%.2f, output=%.2f ml",
                 pump_names[pump_idx], target_value, measured_value, output->output);
    }
    
    xSemaphoreGive(pid_mutex);
    return ret;
}

/**
 * @brief Вычисление и выполнение
 */
esp_err_t pump_pid_compute_and_execute(pump_pid_index_t pump_idx, float measured_value, float target_value) {
    pid_output_t output;
    
    // Вычисление PID
    esp_err_t ret = pump_pid_compute(pump_idx, measured_value, target_value, &output);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Выполнение если доза положительная и достаточная
    if (output.output > 0.5f) { // Минимум 0.5 мл
        return pump_pid_execute(pump_idx, output.output);
    }
    
    return ESP_OK;
}

/**
 * @brief Выполнение дозы
 */
esp_err_t pump_pid_execute(pump_pid_index_t pump_idx, float dose_ml) {
    if (pump_idx >= PUMP_PID_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Выполнение через pump_manager
    pump_index_t pump_index = (pump_index_t)pump_idx;
    esp_err_t ret = pump_manager_dose(pump_index, dose_ml);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PID %s executed: %.2f ml", pump_names[pump_idx], dose_ml);
        
        // Обновление статистики
        xSemaphoreTake(pid_mutex, portMAX_DELAY);
        pump_manager_get_stats(pump_index, &pid_instances[pump_idx].pump_stats);
        xSemaphoreGive(pid_mutex);
    }
    
    return ret;
}

/**
 * @brief Получение выхода PID
 */
pid_output_t pump_pid_get_output(pump_pid_index_t pump_idx) {
    if (pump_idx >= PUMP_PID_COUNT || !pid_manager_initialized) {
        pid_output_t empty = {0};
        return empty;
    }
    
    return pid_get_last_output(&pid_instances[pump_idx].pid);
}

/**
 * @brief Получение статистики
 */
esp_err_t pump_pid_get_stats(pump_pid_index_t pump_idx, pump_stats_t *stats) {
    if (pump_idx >= PUMP_PID_COUNT || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    *stats = pid_instances[pump_idx].pump_stats;
    xSemaphoreGive(pid_mutex);
    
    return ESP_OK;
}

/**
 * @brief Сброс интеграла
 */
esp_err_t pump_pid_reset(pump_pid_index_t pump_idx) {
    if (pump_idx >= PUMP_PID_COUNT || !pid_manager_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    esp_err_t ret = pid_reset(&pid_instances[pump_idx].pid);
    xSemaphoreGive(pid_mutex);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PID %s reset", pump_names[pump_idx]);
    }
    
    return ret;
}

/**
 * @brief Установка режима
 */
esp_err_t pump_pid_set_mode(pump_pid_index_t pump_idx, bool auto_mode) {
    if (pump_idx >= PUMP_PID_COUNT || !pid_manager_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    pid_instances[pump_idx].auto_mode = auto_mode;
    xSemaphoreGive(pid_mutex);
    
    ESP_LOGI(TAG, "PID %s mode: %s", pump_names[pump_idx], auto_mode ? "AUTO" : "MANUAL");
    
    return ESP_OK;
}

/**
 * @brief Установка коэффициентов
 */
esp_err_t pump_pid_set_tunings(pump_pid_index_t pump_idx, float kp, float ki, float kd) {
    if (pump_idx >= PUMP_PID_COUNT || !pid_manager_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    esp_err_t ret = pid_set_tunings(&pid_instances[pump_idx].pid, kp, ki, kd);
    xSemaphoreGive(pid_mutex);
    
    return ret;
}

/**
 * @brief Получение экземпляра PID
 */
pump_pid_instance_t* pump_pid_get_instance(pump_pid_index_t pump_idx) {
    if (pump_idx >= PUMP_PID_COUNT || !pid_manager_initialized) {
        return NULL;
    }
    
    return &pid_instances[pump_idx];
}

