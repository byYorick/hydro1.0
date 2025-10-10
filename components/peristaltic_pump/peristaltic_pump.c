/**
 * @file peristaltic_pump.c
 * @brief Управление перистальтическими насосами через оптопару и полевой транзистор
 * 
 * Схема подключения:
 * ESP32-S3 GPIO → Оптопара (PC817/4N35) → Полевой транзистор (IRLZ44N/IRF540) → Насос 12V
 * 
 * Логика работы:
 * - GPIO HIGH (3.3V) → Оптопара открыта → Транзистор открыт → Насос работает
 * - GPIO LOW (0V) → Оптопара закрыта → Транзистор закрыт → Насос остановлен
 */

#include "peristaltic_pump.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PUMP";

/**
 * @brief Инициализация пина насоса
 * @param gpio_pin GPIO пин для управления насосом
 */
void pump_init(int gpio_pin)
{
    // Проверка корректности номера пина
    if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Неверный GPIO пин: %d", gpio_pin);
        return;
    }
    
    // Настройка GPIO как выход
    esp_err_t err = gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка настройки GPIO%d: %s", gpio_pin, esp_err_to_name(err));
        return;
    }
    
    // Установка начального состояния - LOW (насос выключен)
    gpio_set_level(gpio_pin, 0);
    
    ESP_LOGI(TAG, "Насос инициализирован на GPIO%d", gpio_pin);
}

/**
 * @brief Запуск насоса на заданное время
 * @param gpio_pin GPIO пин управления насосом
 * @param ms Время работы в миллисекундах
 */
void pump_run_ms(int gpio_pin, uint32_t ms)
{
    // Проверка корректности номера пина
    if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Неверный GPIO пин: %d", gpio_pin);
        return;
    }
    
    // Проверка времени
    if (ms == 0) {
        ESP_LOGW(TAG, "GPIO%d: время работы = 0 мс, насос не запущен", gpio_pin);
        return;
    }
    
    if (ms > 60000) {  // Максимум 60 секунд для безопасности
        ESP_LOGW(TAG, "GPIO%d: время %lu мс слишком большое, ограничено до 60000 мс", gpio_pin, (unsigned long)ms);
        ms = 60000;
    }
    
    // Включение насоса (HIGH на GPIO)
    gpio_set_level(gpio_pin, 1);
    ESP_LOGI(TAG, "GPIO%d: насос включен на %lu мс", gpio_pin, (unsigned long)ms);
    
    // Задержка на заданное время
    vTaskDelay(pdMS_TO_TICKS(ms));
    
    // Выключение насоса (LOW на GPIO)
    gpio_set_level(gpio_pin, 0);
    ESP_LOGI(TAG, "GPIO%d: насос выключен", gpio_pin);
}

/**
 * @brief Остановка насоса
 * @param gpio_pin GPIO пин управления насосом
 */
void pump_stop(int gpio_pin)
{
    // Проверка корректности номера пина
    if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Неверный GPIO пин: %d", gpio_pin);
        return;
    }
    
    // Выключение насоса
    gpio_set_level(gpio_pin, 0);
    ESP_LOGI(TAG, "GPIO%d: насос принудительно остановлен", gpio_pin);
}
