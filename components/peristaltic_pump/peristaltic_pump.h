/**
 * @file peristaltic_pump.h
 * @brief Библиотека управления перистальтическими насосами через оптопару и полевой транзистор
 * 
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef PERISTALTIC_PUMP_H
#define PERISTALTIC_PUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Инициализация насоса
 * 
 * Настраивает GPIO пин как выход и устанавливает начальное состояние LOW (насос выключен)
 * 
 * @param gpio_pin GPIO пин для управления насосом
 */
void pump_init(int gpio_pin);

/**
 * @brief Запуск насоса на заданное время
 * 
 * Включает насос (GPIO HIGH), ждет заданное время, затем выключает (GPIO LOW)
 * 
 * @param gpio_pin GPIO пин для управления насосом
 * @param ms Время работы в миллисекундах (макс 60000 мс)
 */
void pump_run_ms(int gpio_pin, uint32_t ms);

/**
 * @brief Принудительная остановка насоса
 * 
 * Немедленно выключает насос (GPIO LOW)
 * 
 * @param gpio_pin GPIO пин для управления насосом
 */
void pump_stop(int gpio_pin);

#ifdef __cplusplus
}
#endif

#endif // PERISTALTIC_PUMP_H
