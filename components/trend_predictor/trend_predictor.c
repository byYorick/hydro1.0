/**
 * @file trend_predictor.c
 * @brief Реализация алгоритмов предсказания трендов
 */

#include "trend_predictor.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "TREND_PREDICTOR";

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

esp_err_t trend_linear_regression(const float *y_values, 
                                   const uint32_t *timestamps,
                                   size_t count,
                                   linear_regression_result_t *result) {
    if (y_values == NULL || timestamps == NULL || result == NULL || count < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(linear_regression_result_t));
    
    // Нормализация времени (относительные часы)
    uint32_t t0 = timestamps[0];
    
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
    float n = (float)count;
    
    for (size_t i = 0; i < count; i++) {
        float x = (float)(timestamps[i] - t0) / 3600.0f; // Часы
        float y = y_values[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }
    
    // Расчет slope и intercept
    float denom = (n * sum_x2) - (sum_x * sum_x);
    if (fabsf(denom) < 0.0001f) {
        ESP_LOGW(TAG, "Линейная регрессия: знаменатель близок к нулю");
        return ESP_FAIL;
    }
    
    result->slope = ((n * sum_xy) - (sum_x * sum_y)) / denom;
    result->intercept = (sum_y - (result->slope * sum_x)) / n;
    
    // Расчет R² (коэффициент детерминации)
    float mean_y = sum_y / n;
    float ss_tot = sum_y2 - (n * mean_y * mean_y);
    
    if (fabsf(ss_tot) < 0.0001f) {
        result->r_squared = 0.0f;
    } else {
        float ss_res = 0;
        for (size_t i = 0; i < count; i++) {
            float x = (float)(timestamps[i] - t0) / 3600.0f;
            float y = y_values[i];
            float y_pred = (result->slope * x) + result->intercept;
            ss_res += (y - y_pred) * (y - y_pred);
        }
        
        result->r_squared = 1.0f - (ss_res / ss_tot);
        
        // Ограничение 0-1
        if (result->r_squared < 0.0f) result->r_squared = 0.0f;
        if (result->r_squared > 1.0f) result->r_squared = 1.0f;
    }
    
    ESP_LOGD(TAG, "Регрессия: slope=%.4f intercept=%.2f R²=%.3f", 
             result->slope, result->intercept, result->r_squared);
    
    return ESP_OK;
}

esp_err_t trend_moving_average(const float *data, 
                                size_t count,
                                size_t window_size,
                                float *result) {
    if (data == NULL || result == NULL || count == 0 || window_size == 0 || window_size > count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    float sum = 0;
    size_t start_index = (count > window_size) ? (count - window_size) : 0;
    size_t actual_window = count - start_index;
    
    for (size_t i = start_index; i < count; i++) {
        sum += data[i];
    }
    
    *result = sum / (float)actual_window;
    
    return ESP_OK;
}

esp_err_t trend_exponential_smoothing(const float *data,
                                       size_t count,
                                       float alpha,
                                       float *result) {
    if (data == NULL || result == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (alpha < 0.0f || alpha > 1.0f) {
        ESP_LOGW(TAG, "alpha должен быть в диапазоне [0,1], используется 0.3");
        alpha = 0.3f;
    }
    
    // Инициализация первым значением
    float smoothed = data[0];
    
    // Экспоненциальное сглаживание
    for (size_t i = 1; i < count; i++) {
        smoothed = alpha * data[i] + (1.0f - alpha) * smoothed;
    }
    
    *result = smoothed;
    
    return ESP_OK;
}

esp_err_t trend_detect_anomaly(const float *data,
                                size_t count,
                                float sigma_threshold,
                                anomaly_detection_result_t *result) {
    if (data == NULL || result == NULL || count < 3) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(anomaly_detection_result_t));
    result->threshold_used = sigma_threshold;
    
    // Расчет среднего
    float mean = 0;
    for (size_t i = 0; i < count; i++) {
        mean += data[i];
    }
    mean /= (float)count;
    
    // Расчет стандартного отклонения
    float variance = 0;
    for (size_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)count;
    float std_dev = sqrtf(variance);
    
    // Проверка каждой точки
    for (size_t i = 0; i < count; i++) {
        float deviation = fabsf(data[i] - mean);
        
        if (deviation > sigma_threshold * std_dev) {
            // Аномалия обнаружена!
            result->anomaly_detected = true;
            result->anomaly_index = i;
            result->anomaly_value = data[i];
            result->deviation_from_trend = deviation;
            
            ESP_LOGW(TAG, "Аномалия! Индекс=%d Значение=%.2f Откл=%.2f (порог=%.2f σ)",
                     i, data[i], deviation, sigma_threshold);
            
            return ESP_OK; // Возвращаем первую найденную аномалию
        }
    }
    
    // Аномалий не найдено
    return ESP_ERR_NOT_FOUND;
}

esp_err_t trend_simple_prediction(const float *data,
                                   const uint32_t *timestamps,
                                   size_t count,
                                   float hours_ahead,
                                   simple_prediction_t *result) {
    if (data == NULL || timestamps == NULL || result == NULL || count < 3) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(simple_prediction_t));
    
    // Линейная регрессия
    linear_regression_result_t reg;
    esp_err_t err = trend_linear_regression(data, timestamps, count, &reg);
    if (err != ESP_OK) {
        return err;
    }
    
    // Предсказание
    uint32_t t0 = timestamps[0];
    uint32_t current_time = timestamps[count - 1];
    float time_hours = (float)(current_time - t0) / 3600.0f;
    float future_time = time_hours + hours_ahead;
    
    result->predicted_value = (reg.slope * future_time) + reg.intercept;
    result->confidence = reg.r_squared;
    result->trend_strength = fabsf(reg.slope);
    
    // Определение направления тренда
    if (fabsf(reg.slope) < 0.01f) {
        result->is_stable = true;
    } else if (reg.slope > 0) {
        result->is_trending_up = true;
    } else {
        result->is_trending_down = true;
    }
    
    ESP_LOGD(TAG, "Прогноз на %.1fч: %.2f (уверенность=%.2f)", 
             hours_ahead, result->predicted_value, result->confidence);
    
    return ESP_OK;
}

esp_err_t trend_calculate_std_dev(const float *data,
                                   size_t count,
                                   float *std_dev) {
    if (data == NULL || std_dev == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Среднее
    float mean = 0;
    for (size_t i = 0; i < count; i++) {
        mean += data[i];
    }
    mean /= (float)count;
    
    // Дисперсия
    float variance = 0;
    for (size_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)count;
    
    // Стандартное отклонение
    *std_dev = sqrtf(variance);
    
    return ESP_OK;
}

esp_err_t trend_calculate_mean(const float *data,
                                size_t count,
                                float *mean) {
    if (data == NULL || mean == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    float sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    
    *mean = sum / (float)count;
    
    return ESP_OK;
}

