/**
 * @file pid_auto_tuner.c
 * @brief Реализация автонастройки PID контроллеров
 */

#include "pid_auto_tuner.h"
#include "pump_manager.h"
#include "notification_system.h"
#include "config_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>

static const char *TAG = "PID_AUTO_TUNER";

// Глобальные данные для всех 6 насосов
static tuning_result_t g_results[PUMP_INDEX_COUNT];
static TaskHandle_t g_tuning_tasks[PUMP_INDEX_COUNT] = {NULL};
static SemaphoreHandle_t g_mutexes[PUMP_INDEX_COUNT];
static bool g_initialized = false;
static bool g_cancel_requested[PUMP_INDEX_COUNT] = {false};

// Callback для получения текущего значения датчика
static float (*g_get_sensor_value_cb)(pump_index_t pump_idx) = NULL;

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Определение индекса датчика по индексу насоса
 */
static sensor_index_t pump_to_sensor_index(pump_index_t pump_idx) {
    // pH насосы
    if (pump_idx == PUMP_INDEX_PH_UP || pump_idx == PUMP_INDEX_PH_DOWN) {
        return SENSOR_INDEX_PH;
    }
    // EC насосы и Water
    else {
        return SENSOR_INDEX_EC;
    }
}

/**
 * @brief Получение текущего значения датчика
 */
static float get_sensor_value(pump_index_t pump_idx) {
    // Если установлен callback - использовать его
    if (g_get_sensor_value_cb != NULL) {
        return g_get_sensor_value_cb(pump_idx);
    }
    
    // Иначе возвращаем примерное значение
    sensor_index_t sensor = pump_to_sensor_index(pump_idx);
    if (sensor == SENSOR_INDEX_PH) {
        return 7.0f;
    } else {
        return 1.5f;
    }
}

/**
 * @brief Relay автонастройка (Ziegler-Nichols)
 * 
 * Алгоритм:
 * 1. Включаем relay режим: если значение выше setpoint → насос ON, ниже → OFF
 * 2. Ждем установившихся осцилляций (минимум 3 цикла)
 * 3. Измеряем амплитуду и период колебаний
 * 4. Вычисляем Ku = 4d/(πa), где d - амплитуда relay, a - амплитуда осцилляций
 * 5. Находим Tu (период)
 * 6. Рассчитываем PID по Ziegler-Nichols: Kp=0.6*Ku, Ki=1.2*Ku/Tu, Kd=0.075*Ku*Tu
 */
static esp_err_t relay_auto_tune(pump_index_t pump_idx, tuning_result_t *result) {
    ESP_LOGI(TAG, "Запуск Relay автонастройки для насоса %d", pump_idx);
    
    const float setpoint = get_sensor_value(pump_idx); // Текущее значение как setpoint
    const float relay_amplitude = AUTO_TUNE_RELAY_AMPLITUDE;
    const uint32_t max_duration_sec = AUTO_TUNE_MAX_DURATION_SEC;
    
    // Массивы для хранения пиков и периодов
    float peaks[10] = {0};
    uint32_t peak_times[10] = {0};
    uint8_t peak_count = 0;
    
    bool relay_state = false;
    uint64_t start_time = esp_timer_get_time() / 1000000ULL;
    uint64_t last_peak_time = start_time;
    float last_value = setpoint;
    bool looking_for_peak = false;
    float current_peak = 0;
    
    // Фаза 1: Инициализация
    snprintf(result->status_message, sizeof(result->status_message), 
             "Инициализация relay теста...");
    result->progress_percent = 5;
    result->state = TUNING_STATE_RELAY_TEST;
    
    // Основной цикл relay теста
    while (peak_count < (AUTO_TUNE_MIN_OSCILLATIONS * 2)) { // *2 потому что считаем max и min
        // Проверка отмены
        if (g_cancel_requested[pump_idx]) {
            ESP_LOGW(TAG, "Автонастройка отменена пользователем");
            result->state = TUNING_STATE_CANCELLED;
            snprintf(result->status_message, sizeof(result->status_message), 
                     "Отменено");
            return ESP_FAIL;
        }
        
        // Проверка таймаута
        uint64_t now = esp_timer_get_time() / 1000000ULL;
        if ((now - start_time) > max_duration_sec) {
            ESP_LOGE(TAG, "Превышен таймаут автонастройки");
            result->state = TUNING_STATE_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Таймаут (>%lu сек)", (unsigned long)max_duration_sec);
            return ESP_ERR_TIMEOUT;
        }
        
        // Получить текущее значение
        float current_value = get_sensor_value(pump_idx);
        
        // Relay логика
        if (current_value > setpoint) {
            relay_state = false; // Насос OFF (значение выше цели)
        } else {
            relay_state = true;  // Насос ON (значение ниже цели)
        }
        
        // Детектирование пиков
        // TODO: Реализация детектирования max/min
        // Упрощенная версия для проверки концепции
        
        // Обновление прогресса
        result->progress_percent = 10 + (peak_count * 60 / (AUTO_TUNE_MIN_OSCILLATIONS * 2));
        
        snprintf(result->status_message, sizeof(result->status_message),
                 "Анализ осцилляций... %d/%d", 
                 peak_count, AUTO_TUNE_MIN_OSCILLATIONS * 2);
        
        // Задержка
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 секунда
        
        last_value = current_value;
    }
    
    // Фаза 2: Анализ собранных данных
    result->state = TUNING_STATE_ANALYZING;
    result->progress_percent = 75;
    snprintf(result->status_message, sizeof(result->status_message), 
             "Анализ данных...");
    
    // Расчет амплитуды осцилляций (среднее расстояние между пиками)
    float amplitude_sum = 0;
    uint8_t amplitude_count = 0;
    
    for (uint8_t i = 0; i < peak_count - 1; i++) {
        amplitude_sum += fabsf(peaks[i+1] - peaks[i]);
        amplitude_count++;
    }
    
    float oscillation_amplitude = (amplitude_count > 0) ? (amplitude_sum / amplitude_count) : 0.1f;
    
    // Расчет периода (среднее время между пиками одного знака)
    float period_sum = 0;
    uint8_t period_count = 0;
    
    for (uint8_t i = 0; i < peak_count - 2; i += 2) {
        period_sum += (float)(peak_times[i+2] - peak_times[i]);
        period_count++;
    }
    
    float period_sec = (period_count > 0) ? (period_sum / period_count) : 120.0f;
    
    // Расчет Ku (критическое усиление)
    // Ku = 4d / (π * a), где d - амплитуда relay, a - амплитуда осцилляций
    float ku = (4.0f * relay_amplitude) / (M_PI * oscillation_amplitude);
    
    // Tu (период)
    float tu = period_sec;
    
    result->ultimate_gain = ku;
    result->ultimate_period_sec = tu;
    
    ESP_LOGI(TAG, "Relay тест завершен: Ku=%.2f Tu=%.1f сек", ku, tu);
    
    // Фаза 3: Расчет PID коэффициентов по Ziegler-Nichols
    result->state = TUNING_STATE_CALCULATING;
    result->progress_percent = 85;
    snprintf(result->status_message, sizeof(result->status_message), 
             "Расчет коэффициентов...");
    
    // Классические формулы Ziegler-Nichols для PID
    result->kp_calculated = 0.6f * ku;
    result->ki_calculated = 1.2f * ku / tu;
    result->kd_calculated = 0.075f * ku * tu;
    
    // Проверка адекватности результатов
    if (result->kp_calculated < 0.1f || result->kp_calculated > 20.0f ||
        result->ki_calculated < 0.0f || result->ki_calculated > 10.0f ||
        result->kd_calculated < 0.0f || result->kd_calculated > 5.0f) {
        
        ESP_LOGW(TAG, "Рассчитанные коэффициенты вне допустимого диапазона");
        result->state = TUNING_STATE_FAILED;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Коэффициенты вне диапазона");
        return ESP_FAIL;
    }
    
    // Завершено!
    result->state = TUNING_STATE_COMPLETED;
    result->tuning_successful = true;
    result->progress_percent = 100;
    result->oscillations_detected = peak_count / 2;
    
    uint64_t end_time = esp_timer_get_time() / 1000000ULL;
    result->tuning_duration_sec = (uint32_t)(end_time - start_time);
    
    snprintf(result->status_message, sizeof(result->status_message),
             "Автонастройка завершена!");
    
    ESP_LOGI(TAG, "Автонастройка успешна: Kp=%.2f Ki=%.2f Kd=%.2f",
             result->kp_calculated, result->ki_calculated, result->kd_calculated);
    
    return ESP_OK;
}

/**
 * @brief Задача автонастройки (выполняется в отдельном потоке)
 */
static void auto_tune_task(void *arg) {
    pump_index_t pump_idx = (pump_index_t)(intptr_t)arg;
    
    ESP_LOGI(TAG, "Задача автонастройки запущена для насоса %d", pump_idx);
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Не удалось взять мьютекс");
        vTaskDelete(NULL);
        return;
    }
    
    tuning_result_t *result = &g_results[pump_idx];
    
    // Сохранение старых коэффициентов
    pump_manager_get_pid_tunings(pump_idx, &result->kp_old, &result->ki_old, &result->kd_old);
    
    // Выполнение автонастройки
    esp_err_t err = ESP_OK;
    
    switch (result->method) {
        case TUNING_METHOD_RELAY:
            err = relay_auto_tune(pump_idx, result);
            break;
            
        case TUNING_METHOD_STEP_RESPONSE:
            // TODO: Реализовать позже
            ESP_LOGW(TAG, "Метод Step Response еще не реализован");
            result->state = TUNING_STATE_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Метод не реализован");
            err = ESP_ERR_NOT_SUPPORTED;
            break;
            
        case TUNING_METHOD_ADAPTIVE:
            // TODO: Реализовать позже
            ESP_LOGW(TAG, "Адаптивный метод еще не реализован");
            result->state = TUNING_STATE_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Метод не реализован");
            err = ESP_ERR_NOT_SUPPORTED;
            break;
            
        default:
            result->state = TUNING_STATE_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Неизвестный метод");
            err = ESP_ERR_INVALID_ARG;
            break;
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    // Уведомление о завершении
    if (err == ESP_OK && result->tuning_successful) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Автонастройка %s завершена", PUMP_NAMES[pump_idx]);
        notification_create(NOTIF_TYPE_INFO, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SYSTEM, msg);
    } else {
        char msg[96];
        snprintf(msg, sizeof(msg), "Автонастройка %s: ошибка", PUMP_NAMES[pump_idx]);
        notification_create(NOTIF_TYPE_WARNING, NOTIF_PRIORITY_HIGH, NOTIF_SOURCE_SYSTEM, msg);
    }
    
    g_tuning_tasks[pump_idx] = NULL;
    vTaskDelete(NULL);
}

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

esp_err_t pid_auto_tuner_init(void) {
    if (g_initialized) {
        ESP_LOGW(TAG, "pid_auto_tuner уже инициализирован");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Инициализация pid_auto_tuner...");
    
    // Создание мьютексов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        g_mutexes[i] = xSemaphoreCreateMutex();
        if (g_mutexes[i] == NULL) {
            ESP_LOGE(TAG, "Не удалось создать мьютекс для насоса %d", i);
            return ESP_ERR_NO_MEM;
        }
        
        // Инициализация результатов
        memset(&g_results[i], 0, sizeof(tuning_result_t));
        g_results[i].state = TUNING_STATE_IDLE;
        g_tuning_tasks[i] = NULL;
        g_cancel_requested[i] = false;
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "pid_auto_tuner инициализирован");
    
    return ESP_OK;
}

esp_err_t pid_auto_tuner_start(pump_index_t pump_idx, tuning_method_t method) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_initialized) {
        ESP_LOGE(TAG, "pid_auto_tuner не инициализирован");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Проверка не запущена ли уже
    if (g_tuning_tasks[pump_idx] != NULL) {
        ESP_LOGW(TAG, "Автонастройка для насоса %d уже запущена", pump_idx);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Инициализация результата
    memset(&g_results[pump_idx], 0, sizeof(tuning_result_t));
    g_results[pump_idx].method = method;
    g_results[pump_idx].state = TUNING_STATE_INIT;
    g_cancel_requested[pump_idx] = false;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    // Создание задачи автонастройки
    char task_name[32];
    snprintf(task_name, sizeof(task_name), "auto_tune_%d", pump_idx);
    
    BaseType_t task_result = xTaskCreate(
        auto_tune_task,
        task_name,
        4096,  // Стек 16KB
        (void*)(intptr_t)pump_idx,
        5,     // Средний приоритет
        &g_tuning_tasks[pump_idx]
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу автонастройки");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Автонастройка запущена для насоса %d методом %d", pump_idx, method);
    
    // Уведомление
    char msg[96];
    snprintf(msg, sizeof(msg), "Автонастройка %s начата", PUMP_NAMES[pump_idx]);
    notification_create(NOTIF_TYPE_INFO, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SYSTEM, msg);
    
    return ESP_OK;
}

bool pid_auto_tuner_is_running(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return false;
    }
    
    return (g_tuning_tasks[pump_idx] != NULL);
}

uint8_t pid_auto_tuner_get_progress(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return 0;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    uint8_t progress = g_results[pump_idx].progress_percent;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return progress;
}

esp_err_t pid_auto_tuner_get_result(pump_index_t pump_idx, tuning_result_t *result) {
    if (pump_idx >= PUMP_INDEX_COUNT || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(result, &g_results[pump_idx], sizeof(tuning_result_t));
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t pid_auto_tuner_cancel(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!pid_auto_tuner_is_running(pump_idx)) {
        ESP_LOGW(TAG, "Автонастройка для насоса %d не запущена", pump_idx);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Запрос отмены автонастройки для насоса %d", pump_idx);
    
    g_cancel_requested[pump_idx] = true;
    
    // Ждем завершения задачи (макс 5 сек)
    for (int i = 0; i < 50; i++) {
        if (!pid_auto_tuner_is_running(pump_idx)) {
            ESP_LOGI(TAG, "Автонастройка отменена");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGW(TAG, "Задача не завершилась, принудительное удаление");
    if (g_tuning_tasks[pump_idx] != NULL) {
        vTaskDelete(g_tuning_tasks[pump_idx]);
        g_tuning_tasks[pump_idx] = NULL;
    }
    
    return ESP_OK;
}

esp_err_t pid_auto_tuner_apply_result(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    tuning_result_t *result = &g_results[pump_idx];
    
    if (!result->tuning_successful || result->state != TUNING_STATE_COMPLETED) {
        ESP_LOGW(TAG, "Автонастройка не завершена или неуспешна");
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Применение коэффициентов в pump_manager
    esp_err_t err = pump_manager_set_pid_tunings(
        pump_idx,
        result->kp_calculated,
        result->ki_calculated,
        result->kd_calculated
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось применить коэффициенты");
        xSemaphoreGive(g_mutexes[pump_idx]);
        return err;
    }
    
    // Сохранение в конфигурацию
    system_config_t config;
    err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_pid[pump_idx].kp = result->kp_calculated;
        config.pump_pid[pump_idx].ki = result->ki_calculated;
        config.pump_pid[pump_idx].kd = result->kd_calculated;
        
        config_save(&config);
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Коэффициенты применены и сохранены: Kp=%.2f Ki=%.2f Kd=%.2f",
             result->kp_calculated, result->ki_calculated, result->kd_calculated);
    
    // ЭТАП 10: Сохраняем адаптивные параметры после автонастройки
    extern esp_err_t adaptive_pid_save_to_nvs(pump_index_t pump_idx);
    adaptive_pid_save_to_nvs(pump_idx);
    
    // Уведомление
    char msg[96];
    snprintf(msg, sizeof(msg), "PID %s: новые коэффициенты применены", PUMP_NAMES[pump_idx]);
    notification_create(NOTIF_TYPE_INFO, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SYSTEM, msg);
    
    return ESP_OK;
}

