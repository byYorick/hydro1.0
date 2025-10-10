/**
 * @file telegram_bot.c
 * @brief Реализация Telegram Bot для IoT системы
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "telegram_bot.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdarg.h>

/// Тег для логирования
static const char *TAG = "TELEGRAM_BOT";

/// Глобальные переменные
static char bot_token[64] = {0};
static char chat_id[32] = {0};
static SemaphoreHandle_t telegram_mutex = NULL;
static bool telegram_initialized = false;
static TaskHandle_t telegram_task_handle = NULL;
static telegram_command_callback_t command_callback = NULL;
static void *command_callback_ctx = NULL;
static uint32_t poll_interval = 10000; // 10 секунд по умолчанию
static bool enable_commands = false;

/// URL API Telegram
#define TELEGRAM_API_URL "https://api.telegram.org/bot"
#define MAX_MESSAGE_LEN  4096
#define HTTP_BUFFER_SIZE 2048

/// Буфер для HTTP ответов
static char http_response_buffer[HTTP_BUFFER_SIZE];
static int http_response_len = 0;

/**
 * @brief HTTP event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len < HTTP_BUFFER_SIZE - http_response_len) {
                memcpy(http_response_buffer + http_response_len, evt->data, evt->data_len);
                http_response_len += evt->data_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Отправка HTTP POST запроса к Telegram API
 */
static esp_err_t telegram_api_request(const char *method, const char *post_data) {
    char url[256];
    snprintf(url, sizeof(url), "%s%s/%s", TELEGRAM_API_URL, bot_token, method);
    
    http_response_len = 0;
    memset(http_response_buffer, 0, sizeof(http_response_buffer));
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Ошибка создания HTTP клиента");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGD(TAG, "HTTP Status = %d, Response length = %d", status, http_response_len);
        
        if (status >= 200 && status < 300) {
            http_response_buffer[http_response_len] = '\0';
            ESP_LOGV(TAG, "Response: %s", http_response_buffer);
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status %d", status);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
    return err;
}

/**
 * @brief Задача опроса команд (если включено)
 */
static void telegram_poll_task(void *pvParameters) {
    uint32_t last_update_id = 0;
    
    while (1) {
        if (enable_commands && command_callback) {
            // Формируем запрос getUpdates
            char post_data[128];
            snprintf(post_data, sizeof(post_data), 
                    "{\"offset\":%lu,\"timeout\":30}", (unsigned long)(last_update_id + 1));
            
            if (telegram_api_request("getUpdates", post_data) == ESP_OK) {
                // Парсим ответ
                cJSON *root = cJSON_Parse(http_response_buffer);
                if (root) {
                    cJSON *ok = cJSON_GetObjectItem(root, "ok");
                    cJSON *result = cJSON_GetObjectItem(root, "result");
                    
                    if (ok && ok->valueint && result && cJSON_IsArray(result)) {
                        int array_size = cJSON_GetArraySize(result);
                        
                        for (int i = 0; i < array_size; i++) {
                            cJSON *update = cJSON_GetArrayItem(result, i);
                            cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
                            cJSON *message = cJSON_GetObjectItem(update, "message");
                            
                            if (update_id && message) {
                                last_update_id = update_id->valueint;
                                
                                cJSON *text = cJSON_GetObjectItem(message, "text");
                                if (text && cJSON_IsString(text)) {
                                    ESP_LOGI(TAG, "Получена команда: %s", text->valuestring);
                                    command_callback(text->valuestring, command_callback_ctx);
                                }
                            }
                        }
                    }
                    
                    cJSON_Delete(root);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(poll_interval));
    }
}

/**
 * @brief Инициализация Telegram Bot
 */
esp_err_t telegram_bot_init(const telegram_config_t *config) {
    if (telegram_initialized) {
        ESP_LOGW(TAG, "Telegram Bot уже инициализирован");
        return ESP_OK;
    }
    
    if (config == NULL || strlen(config->bot_token) == 0) {
        ESP_LOGE(TAG, "Некорректная конфигурация Telegram");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Создание мьютекса
    if (telegram_mutex == NULL) {
        telegram_mutex = xSemaphoreCreateMutex();
        if (telegram_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(telegram_mutex, portMAX_DELAY);
    
    // Сохранение конфигурации
    strncpy(bot_token, config->bot_token, sizeof(bot_token) - 1);
    strncpy(chat_id, config->chat_id, sizeof(chat_id) - 1);
    poll_interval = 10000; // Фиксированный интервал 10 сек
    enable_commands = config->enable_commands;
    
    telegram_initialized = true;
    
    ESP_LOGI(TAG, "Telegram Bot инициализирован");
    
    xSemaphoreGive(telegram_mutex);
    return ESP_OK;
}

/**
 * @brief Деинициализация Telegram Bot
 */
esp_err_t telegram_bot_deinit(void) {
    if (!telegram_initialized) {
        return ESP_OK;
    }
    
    telegram_bot_stop();
    
    xSemaphoreTake(telegram_mutex, portMAX_DELAY);
    
    telegram_initialized = false;
    memset(bot_token, 0, sizeof(bot_token));
    memset(chat_id, 0, sizeof(chat_id));
    
    ESP_LOGI(TAG, "Telegram Bot деинициализирован");
    
    xSemaphoreGive(telegram_mutex);
    
    if (telegram_mutex != NULL) {
        vSemaphoreDelete(telegram_mutex);
        telegram_mutex = NULL;
    }
    
    return ESP_OK;
}

/**
 * @brief Запуск Telegram Bot
 */
esp_err_t telegram_bot_start(void) {
    if (!telegram_initialized) {
        ESP_LOGE(TAG, "Telegram Bot не инициализирован");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (telegram_task_handle != NULL) {
        ESP_LOGW(TAG, "Telegram Bot уже запущен");
        return ESP_OK;
    }
    
    // Создание задачи опроса команд
    if (enable_commands) {
        BaseType_t ret = xTaskCreate(
            telegram_poll_task,
            "telegram_poll",
            4096,
            NULL,
            5,
            &telegram_task_handle
        );
        
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Ошибка создания задачи опроса");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Telegram Bot запущен с опросом команд");
    } else {
        ESP_LOGI(TAG, "Telegram Bot запущен (только отправка)");
    }
    
    return ESP_OK;
}

/**
 * @brief Остановка Telegram Bot
 */
esp_err_t telegram_bot_stop(void) {
    if (telegram_task_handle != NULL) {
        vTaskDelete(telegram_task_handle);
        telegram_task_handle = NULL;
        ESP_LOGI(TAG, "Telegram Bot остановлен");
    }
    
    return ESP_OK;
}

/**
 * @brief Отправка текстового сообщения
 */
esp_err_t telegram_send_message(const char *message) {
    if (!telegram_initialized || message == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (strlen(chat_id) == 0) {
        ESP_LOGW(TAG, "Chat ID не установлен");
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(telegram_mutex, portMAX_DELAY);
    
    // Формируем JSON для отправки
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "chat_id", chat_id);
    cJSON_AddStringToObject(root, "text", message);
    cJSON_AddStringToObject(root, "parse_mode", "Markdown");
    
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    esp_err_t ret = ESP_OK;
    
    if (post_data) {
        ret = telegram_api_request("sendMessage", post_data);
        free(post_data);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Сообщение отправлено");
        } else {
            ESP_LOGE(TAG, "Ошибка отправки сообщения");
        }
    } else {
        ret = ESP_ERR_NO_MEM;
    }
    
    xSemaphoreGive(telegram_mutex);
    return ret;
}

/**
 * @brief Отправка аларма
 */
esp_err_t telegram_send_alarm(const char *type, const char *message, telegram_severity_t severity) {
    if (!telegram_initialized || type == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char formatted_message[MAX_MESSAGE_LEN];
    const char *emoji;
    const char *severity_str;
    
    switch (severity) {
        case TELEGRAM_SEVERITY_CRITICAL:
            emoji = "🔴";
            severity_str = "КРИТИЧНО";
            break;
        case TELEGRAM_SEVERITY_ERROR:
            emoji = "⚠️";
            severity_str = "ОШИБКА";
            break;
        case TELEGRAM_SEVERITY_WARNING:
            emoji = "⚡";
            severity_str = "ВНИМАНИЕ";
            break;
        default:
            emoji = "ℹ️";
            severity_str = "ИНФО";
            break;
    }
    
    snprintf(formatted_message, sizeof(formatted_message),
             "%s *%s: %s*\n\n%s",
             emoji, severity_str, type, message);
    
    return telegram_send_message(formatted_message);
}

/**
 * @brief Отправка форматированного сообщения
 */
esp_err_t telegram_send_formatted(const char *format, ...) {
    if (!telegram_initialized || format == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char message[MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    return telegram_send_message(message);
}

/**
 * @brief Отправка статуса системы
 */
esp_err_t telegram_send_status(float ph, float ec, float temperature, const char *status) {
    char message[512];
    
    snprintf(message, sizeof(message),
             "📊 *Статус системы*\n\n"
             "🔵 pH: %.2f\n"
             "⚡ EC: %.2f mS/cm\n"
             "🌡 Температура: %.1f°C\n\n"
             "Статус: %s",
             ph, ec, temperature, status);
    
    return telegram_send_message(message);
}

/**
 * @brief Отправка ежедневного отчета
 */
esp_err_t telegram_send_daily_report(const char *summary) {
    if (!telegram_initialized || summary == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char message[MAX_MESSAGE_LEN];
    
    snprintf(message, sizeof(message),
             "📈 *Ежедневный отчет*\n\n%s",
             summary);
    
    return telegram_send_message(message);
}

/**
 * @brief Регистрация callback для команд
 */
esp_err_t telegram_register_command_callback(telegram_command_callback_t callback, void *user_ctx) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    command_callback = callback;
    command_callback_ctx = user_ctx;
    
    ESP_LOGI(TAG, "Зарегистрирован callback для команд");
    
    return ESP_OK;
}

/**
 * @brief Проверка подключения
 */
bool telegram_is_connected(void) {
    if (!telegram_initialized) {
        return false;
    }
    
    // Простая проверка через getMe
    char empty_data[] = "{}";
    esp_err_t ret = telegram_api_request("getMe", empty_data);
    
    return (ret == ESP_OK);
}

/**
 * @brief Установка ID чата
 */
esp_err_t telegram_set_chat_id(const char *new_chat_id) {
    if (new_chat_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(telegram_mutex, portMAX_DELAY);
    
    strncpy(chat_id, new_chat_id, sizeof(chat_id) - 1);
    chat_id[sizeof(chat_id) - 1] = '\0';
    
    ESP_LOGI(TAG, "Chat ID установлен: %s", chat_id);
    
    xSemaphoreGive(telegram_mutex);
    
    return ESP_OK;
}

/**
 * @brief Получение ID чата
 */
esp_err_t telegram_get_chat_id(char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(buffer, chat_id, max_len - 1);
    buffer[max_len - 1] = '\0';
    
    return ESP_OK;
}

