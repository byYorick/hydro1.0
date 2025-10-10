/**
 * @file iot_integration.h
 * @brief Интеграция всех IoT компонентов
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef IOT_INTEGRATION_H
#define IOT_INTEGRATION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация IoT системы
 * 
 * Инициализирует все включенные компоненты в правильной последовательности:
 * 1. Network Manager
 * 2. SD Card (если включено)
 * 3. MQTT Client (если включено)
 * 4. Telegram Bot (если включено)
 * 5. Mesh Network (если включено)
 * 
 * @return ESP_OK при успехе
 */
esp_err_t iot_system_init(void);

/**
 * @brief Запуск IoT системы
 * 
 * Запускает все сервисы
 * 
 * @return ESP_OK при успехе
 */
esp_err_t iot_system_start(void);

/**
 * @brief Остановка IoT системы
 * 
 * @return ESP_OK при успехе
 */
esp_err_t iot_system_stop(void);

/**
 * @brief Деинициализация IoT системы
 * 
 * @return ESP_OK при успехе
 */
esp_err_t iot_system_deinit(void);

/**
 * @brief Публикация данных датчиков во все каналы
 * 
 * Отправляет данные в MQTT, логирует на SD, уведомляет через Telegram при алармах
 * 
 * @param ph pH значение
 * @param ec EC значение
 * @param temperature Температура
 * @param humidity Влажность
 * @param lux Освещённость
 * @param co2 CO2
 * @return ESP_OK при успехе
 */
esp_err_t iot_publish_sensor_data(float ph, float ec, float temperature, 
                                   float humidity, float lux, uint16_t co2);

/**
 * @brief Публикация аларма
 * 
 * @param type Тип аларма
 * @param message Сообщение
 * @param severity Уровень важности
 * @return ESP_OK при успехе
 */
esp_err_t iot_publish_alarm(const char *type, const char *message, const char *severity);

/**
 * @brief Получение статистики IoT системы
 * 
 * @param buffer Буфер для JSON строки со статистикой
 * @param max_len Максимальная длина
 * @return ESP_OK при успехе
 */
esp_err_t iot_get_system_stats(char *buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // IOT_INTEGRATION_H

