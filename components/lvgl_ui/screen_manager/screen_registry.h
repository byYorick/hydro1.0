#pragma once

/**
 * @file screen_registry.h
 * @brief Реестр экранов - регистрация и управление конфигурациями
 * 
 * Registry Pattern - централизованное хранилище конфигураций экранов.
 * Позволяет регистрировать экраны декларативно и находить их по ID.
 * 
 * @author Hydroponics Monitor Team
 * @date 2025-10-08
 * @version 1.0
 */

#include "screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация реестра экранов
 * 
 * Должна быть вызвана один раз при старте системы.
 * Создает мьютекс и устанавливает конфигурацию по умолчанию.
 * 
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t screen_registry_init(void);

/**
 * @brief Регистрация экрана в системе
 * 
 * Добавляет конфигурацию экрана в реестр. После регистрации экран
 * можно создавать и показывать по его ID.
 * 
 * @param config Указатель на конфигурацию экрана
 * @return ESP_OK при успехе
 * @return ESP_ERR_INVALID_ARG если config == NULL или невалидный ID
 * @return ESP_ERR_INVALID_STATE если экран уже зарегистрирован
 * @return ESP_ERR_NO_MEM если достигнут лимит экранов
 */
esp_err_t screen_register(const screen_config_t *config);

/**
 * @brief Удаление экрана из реестра
 * 
 * Отменяет регистрацию экрана. Активные экземпляры должны быть
 * уничтожены перед вызовом этой функции.
 * 
 * @param screen_id ID экрана для удаления
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экран не найден
 */
esp_err_t screen_unregister(const char *screen_id);

/**
 * @brief Получить конфигурацию экрана по ID
 * 
 * @param screen_id ID экрана
 * @return Указатель на конфигурацию или NULL если не найден
 */
screen_config_t* screen_get_config(const char *screen_id);

/**
 * @brief Получить количество зарегистрированных экранов
 * 
 * @return Количество экранов в реестре
 */
uint8_t screen_get_registered_count(void);

/**
 * @brief Получить экземпляр менеджера (для внутреннего использования)
 * 
 * @return Указатель на singleton менеджера
 */
screen_manager_t* screen_manager_get_instance(void);

#ifdef __cplusplus
}
#endif

