/**
 * @file pid_controller.c
 * @brief Реализация классического PID контроллера
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "pid_controller.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "PID_CONTROLLER";

/**
 * @brief Инициализация PID
 */
esp_err_t pid_init(pid_controller_t *pid, const pid_config_t *config) {
    if (pid == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(pid, 0, sizeof(pid_controller_t));
    
    pid->config = *config;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->initialized = true;
    
    ESP_LOGD(TAG, "PID initialized: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             config->kp, config->ki, config->kd);
    
    return ESP_OK;
}

/**
 * @brief Вычисление выхода PID
 */
esp_err_t pid_compute(pid_controller_t *pid, float measured_value, float dt, pid_output_t *output) {
    if (pid == NULL || output == NULL || !pid->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (dt <= 0.0f) {
        ESP_LOGW(TAG, "Invalid dt: %.3f", dt);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Вычисление ошибки
    float error = pid->config.setpoint - measured_value;
    
    // P-компонента
    float p_term = pid->config.kp * error;
    
    // I-компонента с anti-windup clamping
    pid->integral += pid->config.ki * error * dt;
    
    // Anti-windup: ограничение интеграла
    if (pid->integral > pid->config.integral_max) {
        pid->integral = pid->config.integral_max;
    }
    if (pid->integral < pid->config.integral_min) {
        pid->integral = pid->config.integral_min;
    }
    
    float i_term = pid->integral;
    
    // D-компонента (производная ошибки)
    float d_term = 0.0f;
    if (pid->sample_count > 0) {
        d_term = pid->config.kd * (error - pid->prev_error) / dt;
    }
    
    // Итоговый выход
    float pid_output = p_term + i_term + d_term;
    
    // Ограничение выхода
    if (pid_output > pid->config.output_max) {
        pid_output = pid->config.output_max;
    }
    if (pid_output < pid->config.output_min) {
        pid_output = pid->config.output_min;
    }
    
    // Сохранение результата
    output->p_term = p_term;
    output->i_term = i_term;
    output->d_term = d_term;
    output->output = pid_output;
    output->error = error;
    
    pid->last_output = *output;
    pid->prev_error = error;
    pid->sample_count++;
    
    // Обновление истории
    pid->history.values[pid->history.index] = measured_value;
    pid->history.index = (pid->history.index + 1) % 10;
    if (pid->history.count < 10) {
        pid->history.count++;
    }
    
    ESP_LOGV(TAG, "PID: error=%.3f, P=%.3f, I=%.3f, D=%.3f, output=%.3f", 
             error, p_term, i_term, d_term, pid_output);
    
    return ESP_OK;
}

/**
 * @brief Сброс PID
 */
esp_err_t pid_reset(pid_controller_t *pid) {
    if (pid == NULL || !pid->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->sample_count = 0;
    memset(&pid->history, 0, sizeof(pid_history_t));
    memset(&pid->last_output, 0, sizeof(pid_output_t));
    
    ESP_LOGI(TAG, "PID reset");
    
    return ESP_OK;
}

/**
 * @brief Установка коэффициентов
 */
esp_err_t pid_set_tunings(pid_controller_t *pid, float kp, float ki, float kd) {
    if (pid == NULL || !pid->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    pid->config.kp = kp;
    pid->config.ki = ki;
    pid->config.kd = kd;
    
    ESP_LOGI(TAG, "PID tunings updated: Kp=%.2f, Ki=%.2f, Kd=%.2f", kp, ki, kd);
    
    return ESP_OK;
}

/**
 * @brief Установка setpoint
 */
esp_err_t pid_set_setpoint(pid_controller_t *pid, float setpoint) {
    if (pid == NULL || !pid->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    pid->config.setpoint = setpoint;
    
    ESP_LOGD(TAG, "Setpoint updated: %.2f", setpoint);
    
    return ESP_OK;
}

/**
 * @brief Установка ограничений выхода
 */
esp_err_t pid_set_output_limits(pid_controller_t *pid, float min, float max) {
    if (pid == NULL || !pid->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (min >= max) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pid->config.output_min = min;
    pid->config.output_max = max;
    
    ESP_LOGD(TAG, "Output limits: %.2f - %.2f", min, max);
    
    return ESP_OK;
}

/**
 * @brief Получение последнего выхода
 */
pid_output_t pid_get_last_output(const pid_controller_t *pid) {
    if (pid == NULL) {
        pid_output_t empty = {0};
        return empty;
    }
    
    return pid->last_output;
}

