#pragma once

/**
 * @file screen_navigator.h
 * @brief Навигация между экранами с историей
 * 
 * Управляет переходами между экранами, поддерживает историю навигации
 * для кнопки "Назад" и навигацию по иерархии экранов.
 * 
 * @author Hydroponics Monitor Team
 * @date 2025-10-08
 * @version 1.0
 */

#include "screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  НАВИГАЦИЯ
 * ============================= */

/**
 * @brief Показать экран с добавлением в историю
 * 
 * Основная функция навигации. Сохраняет текущий экран в историю,
 * затем показывает новый экран.
 * 
 * @param screen_id ID целевого экрана
 * @param params Параметры для передачи в экран
 * @return ESP_OK при успехе
 */
esp_err_t navigator_show(const char *screen_id, void *params);

/**
 * @brief Вернуться к предыдущему экрану из истории
 * 
 * Берет предыдущий экран из стека истории и показывает его.
 * 
 * @return ESP_OK при успехе
 * @return ESP_ERR_INVALID_STATE если история пуста
 */
esp_err_t navigator_go_back(void);

/**
 * @brief Перейти к родительскому экрану
 * 
 * Использует parent_id из конфигурации текущего экрана.
 * 
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_SUPPORTED если у экрана нет родителя
 */
esp_err_t navigator_go_to_parent(void);

/**
 * @brief Перейти к корневому экрану (главный экран)
 * 
 * Очищает всю историю и показывает экран с is_root=true.
 * 
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если корневой экран не найден
 */
esp_err_t navigator_go_home(void);

/* =============================
 *  ИСТОРИЯ
 * ============================= */

/**
 * @brief Получить размер истории навигации
 * 
 * @return Количество экранов в истории
 */
uint8_t navigator_get_history_count(void);

/**
 * @brief Очистить всю историю навигации
 */
void navigator_clear_history(void);

#ifdef __cplusplus
}
#endif

