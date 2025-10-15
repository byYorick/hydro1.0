/**
 * @file trend_predictor.h
 * @brief Предсказание трендов для гидропонных параметров
 * 
 * Компонент предоставляет математические алгоритмы для:
 * - Линейной регрессии (метод наименьших квадратов)
 * - Скользящего среднего
 * - Экспоненциального сглаживания
 * - Детектора аномалий
 * 
 * @author Hydro Team
 * @date 2025-10-15
 */

#ifndef TREND_PREDICTOR_H
#define TREND_PREDICTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * СТРУКТУРЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Результат линейной регрессии
 */
typedef struct {
    float slope;        // Наклон (a в y = ax + b)
    float intercept;    // Пересечение (b)
    float r_squared;    // Коэффициент детерминации (0-1, качество fit)
} linear_regression_result_t;

/**
 * @brief Результат детектора аномалий
 */
typedef struct {
    bool anomaly_detected;              // Обнаружена ли аномалия
    uint8_t anomaly_index;              // Индекс аномальной точки
    float anomaly_value;                // Аномальное значение
    float deviation_from_trend;         // Отклонение от тренда
    float threshold_used;               // Использованный порог (кратность σ)
} anomaly_detection_result_t;

/**
 * @brief Простое предсказание
 */
typedef struct {
    float predicted_value;              // Предсказанное значение
    float confidence;                   // Уверенность (0-1)
    float trend_strength;               // Сила тренда
    bool is_trending_up;                // Тренд вверх?
    bool is_trending_down;              // Тренд вниз?
    bool is_stable;                     // Стабильно?
} simple_prediction_t;

/*******************************************************************************
 * API ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Линейная регрессия (метод наименьших квадратов)
 * 
 * Находит линию наилучшего fit y = ax + b для набора данных
 * 
 * @param y_values Массив значений Y
 * @param timestamps Массив временных меток (секунды)
 * @param count Количество точек данных
 * @param result Указатель на структуру результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_linear_regression(const float *y_values, 
                                   const uint32_t *timestamps,
                                   size_t count,
                                   linear_regression_result_t *result);

/**
 * @brief Скользящее среднее (Simple Moving Average)
 * 
 * @param data Массив данных
 * @param count Количество точек
 * @param window_size Размер окна (количество точек для усреднения)
 * @param result Указатель для результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_moving_average(const float *data, 
                                size_t count,
                                size_t window_size,
                                float *result);

/**
 * @brief Экспоненциальное сглаживание (Exponential Smoothing)
 * 
 * @param data Массив данных
 * @param count Количество точек
 * @param alpha Коэффициент сглаживания (0-1), рекомендуется 0.3
 * @param result Указатель для результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_exponential_smoothing(const float *data,
                                       size_t count,
                                       float alpha,
                                       float *result);

/**
 * @brief Детектор аномалий (на основе стандартного отклонения)
 * 
 * Обнаруживает точки, которые значительно отклоняются от тренда
 * 
 * @param data Массив данных
 * @param count Количество точек
 * @param sigma_threshold Порог (кратность σ), рекомендуется 2.0-3.0
 * @param result Указатель на структуру результата
 * @return ESP_OK при успехе, ESP_ERR_NOT_FOUND если аномалий нет
 */
esp_err_t trend_detect_anomaly(const float *data,
                                size_t count,
                                float sigma_threshold,
                                anomaly_detection_result_t *result);

/**
 * @brief Простое предсказание на основе последних N точек
 * 
 * Использует линейную регрессию для прогноза
 * 
 * @param data Массив данных
 * @param timestamps Массив временных меток
 * @param count Количество точек
 * @param hours_ahead Сколько часов вперед предсказать
 * @param result Указатель на структуру результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_simple_prediction(const float *data,
                                   const uint32_t *timestamps,
                                   size_t count,
                                   float hours_ahead,
                                   simple_prediction_t *result);

/**
 * @brief Расчет стандартного отклонения
 * 
 * @param data Массив данных
 * @param count Количество точек
 * @param std_dev Указатель для результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_calculate_std_dev(const float *data,
                                   size_t count,
                                   float *std_dev);

/**
 * @brief Расчет среднего значения
 * 
 * @param data Массив данных
 * @param count Количество точек
 * @param mean Указатель для результата
 * @return ESP_OK при успехе
 */
esp_err_t trend_calculate_mean(const float *data,
                                size_t count,
                                float *mean);

#ifdef __cplusplus
}
#endif

#endif // TREND_PREDICTOR_H

