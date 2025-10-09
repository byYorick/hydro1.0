#pragma once

/**
 * @file screen_manager.h
 * @brief Screen Manager - главный API системы управления экранами
 * 
 * Единая точка входа для всех операций с экранами.
 * Объединяет Registry, Lifecycle и Navigator в простой и удобный API.
 * 
 * ПРИМЕР ИСПОЛЬЗОВАНИЯ:
 * 
 * // 1. Инициализация
 * screen_manager_init(NULL);
 * 
 * // 2. Регистрация экрана
 * screen_config_t config = {
 *     .id = "my_screen",
 *     .title = "My Screen",
 *     .category = SCREEN_CATEGORY_MENU,
 *     .parent_id = "main",
 *     .create_fn = my_screen_create,
 *     .lazy_load = true,
 * };
 * screen_register(&config);
 * 
 * // 3. Навигация
 * screen_show("my_screen", NULL);     // Показать
 * screen_go_back();                    // Назад
 * screen_go_home();                    // На главный
 * 
 * @author Hydroponics Monitor Team
 * @date 2025-10-08
 * @version 1.0
 */

#include "screen_types.h"
#include "screen_registry.h"
#include "screen_lifecycle.h"
#include "screen_navigator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================= */

/**
 * @brief Инициализация Screen Manager
 * 
 * Должна быть вызвана один раз при старте системы перед
 * регистрацией экранов.
 * 
 * @param config Конфигурация менеджера (NULL = настройки по умолчанию)
 * @return ESP_OK при успехе
 */
esp_err_t screen_manager_init(const screen_manager_config_t *config);

/**
 * @brief Деинициализация Screen Manager
 * 
 * Уничтожает все экземпляры, освобождает память.
 * Вызывать при завершении работы системы.
 * 
 * @return ESP_OK при успехе
 */
esp_err_t screen_manager_deinit(void);

/* =============================
 *  НАВИГАЦИЯ (Упрощенный API)
 * ============================= */

/**
 * @brief Показать экран
 * 
 * Основная функция для показа экранов. Автоматически управляет
 * историей, скрывает предыдущий экран, загружает новый.
 * 
 * @param screen_id ID экрана (например, "main", "detail_ph")
 * @param params Параметры для передачи в экран (опционально)
 * @return ESP_OK при успехе
 */
esp_err_t screen_show(const char *screen_id, void *params);

/**
 * @brief Скрыть экран
 * 
 * Скрывает указанный экран. В зависимости от конфигурации может
 * уничтожить или закэшировать его.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 */
esp_err_t screen_hide(const char *screen_id);

/**
 * @brief Вернуться назад
 * 
 * Возвращает к предыдущему экрану из истории навигации.
 * Эквивалентно нажатию кнопки "Назад".
 * 
 * @return ESP_OK при успехе
 */
esp_err_t screen_go_back(void);

/**
 * @brief Очистить группу энкодера от скрытых элементов
 * 
 * Удаляет из группы энкодера все элементы, которые:
 * - Имеют флаг LV_OBJ_FLAG_HIDDEN
 * - Имеют нулевой размер (width <= 0 или height <= 0)
 * - Имеют скрытых родителей
 * 
 * @param screen_id ID экрана (NULL = текущий экран)
 * @return Количество удаленных элементов
 */
int screen_cleanup_hidden_elements(const char *screen_id);

/**
 * @brief Перейти к родительскому экрану
 * 
 * Использует parent_id из конфигурации.
 * 
 * @return ESP_OK при успехе
 */
esp_err_t screen_go_to_parent(void);

/**
 * @brief Перейти на главный экран
 * 
 * Очищает историю и показывает корневой экран.
 * 
 * @return ESP_OK при успехе
 */
esp_err_t screen_go_home(void);

/**
 * @brief Обновить данные экрана без пересоздания
 * 
 * Вызывает on_update callback для обновления отображаемых данных.
 * Полезно для обновления значений датчиков.
 * 
 * @param screen_id ID экрана
 * @param data Данные для обновления
 * @return ESP_OK при успехе
 */
esp_err_t screen_update(const char *screen_id, void *data);

/* =============================
 *  УПРАВЛЕНИЕ ЖИЗНЕННЫМ ЦИКЛОМ
 * ============================= */

/**
 * @brief Создать экран вручную
 * 
 * Обычно не требуется (используется lazy_load).
 * Может быть полезно для предварительного создания.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 */
esp_err_t screen_create(const char *screen_id);

/**
 * @brief Уничтожить экран
 * 
 * Освобождает память. Нельзя уничтожить текущий видимый экран.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 */
esp_err_t screen_destroy(const char *screen_id);

/**
 * @brief Перезагрузить экран
 * 
 * Уничтожает и создает заново. Полезно для сброса состояния.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 */
esp_err_t screen_reload(const char *screen_id);

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

/**
 * @brief Получить текущий экран
 * 
 * @return Указатель на экземпляр текущего экрана или NULL
 */
screen_instance_t* screen_get_current(void);

/**
 * @brief Получить экран по ID
 * 
 * @param screen_id ID экрана
 * @return Указатель на экземпляр или NULL если не создан
 */
screen_instance_t* screen_get_by_id(const char *screen_id);

/**
 * @brief Проверить, виден ли экран
 * 
 * @param screen_id ID экрана
 * @return true если экран видим
 */
bool screen_is_visible_check(const char *screen_id);

/**
 * @brief Получить количество экранов в истории
 * 
 * @return Размер истории навигации
 */
uint8_t screen_get_history_count(void);

/* =============================
 *  УПРАВЛЕНИЕ ГРУППОЙ ЭНКОДЕРА
 * ============================= */

/**
 * @brief Добавить виджет в группу энкодера экрана
 * 
 * Удобная функция для ручного добавления виджетов в группу энкодера.
 * 
 * @param screen_id ID экрана (если NULL - используется текущий)
 * @param widget Виджет для добавления
 * @return ESP_OK при успехе
 */
esp_err_t screen_add_to_group(const char *screen_id, lv_obj_t *widget);

/**
 * @brief Рекурсивно добавить все интерактивные элементы виджета
 * 
 * Полезно для сложных виджетов с глубокой иерархией.
 * 
 * @param screen_id ID экрана (если NULL - используется текущий)
 * @param widget Корневой виджет для обхода
 * @return Количество добавленных элементов
 */
int screen_add_widget_tree(const char *screen_id, lv_obj_t *widget);

#ifdef __cplusplus
}
#endif

