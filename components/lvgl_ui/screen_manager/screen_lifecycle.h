#pragma once

/**
 * @file screen_lifecycle.h
 * @brief Управление жизненным циклом экранов
 * 
 * Отвечает за создание, уничтожение, показ и скрытие экземпляров экранов.
 * Реализует ленивую загрузку, кэширование и автоматическую очистку памяти.
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
 *  СОЗДАНИЕ/УНИЧТОЖЕНИЕ
 * ============================= */

/**
 * @brief Создать экземпляр экрана
 * 
 * Создает LVGL объекты и группу энкодера для экрана.
 * Вызывает create_fn из конфигурации.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экран не зарегистрирован
 * @return ESP_ERR_NO_MEM если нет памяти или достигнут лимит экземпляров
 */
esp_err_t screen_create_instance(const char *screen_id);

/**
 * @brief Уничтожить экземпляр экрана
 * 
 * Освобождает всю память, занятую экраном.
 * Нельзя уничтожить текущий видимый экран.
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экземпляр не найден
 * @return ESP_ERR_INVALID_STATE если это текущий экран
 */
esp_err_t screen_destroy_instance(const char *screen_id);


/* =============================
 *  ПОКАЗ/СКРЫТИЕ
 * ============================= */

/**
 * @brief Показать экран
 * 
 * Если экземпляр не создан и lazy_load=true, создает его автоматически.
 * Скрывает предыдущий экран, загружает новый, устанавливает группу энкодера.
 * 
 * @param screen_id ID экрана
 * @param params Параметры для передачи в on_show callback (опционально)
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экран не найден
 * @return ESP_ERR_NOT_ALLOWED если can_show_fn вернула false
 */
esp_err_t screen_show_instance(const char *screen_id, void *params);

/**
 * @brief Скрыть экран
 * 
 * Вызывает on_hide callback. В зависимости от конфигурации может:
 * - Уничтожить экран (destroy_on_hide=true)
 * - Закэшировать (cache_on_hide=true)
 * - Просто скрыть
 * 
 * @param screen_id ID экрана
 * @return ESP_OK при успехе
 */
esp_err_t screen_hide_instance(const char *screen_id);

/* =============================
 *  ОБНОВЛЕНИЕ
 * ============================= */

/**
 * @brief Обновить данные экрана без пересоздания
 * 
 * Вызывает on_update callback для обновления отображаемых данных.
 * 
 * @param screen_id ID экрана
 * @param data Данные для обновления
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экран не найден
 * @return ESP_ERR_NOT_SUPPORTED если on_update не определен
 */
esp_err_t screen_update_instance(const char *screen_id, void *data);

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

/**
 * @brief Получить текущий видимый экран
 * 
 * @return Указатель на экземпляр или NULL
 */
screen_instance_t* screen_get_current_instance(void);

/**
 * @brief Получить экземпляр экрана по ID
 * 
 * @param screen_id ID экрана
 * @return Указатель на экземпляр или NULL если не создан
 */
screen_instance_t* screen_get_instance_by_id(const char *screen_id);

/**
 * @brief Проверить, виден ли экран
 * 
 * @param screen_id ID экрана
 * @return true если экран видим
 */
bool screen_is_visible(const char *screen_id);

/**
 * @brief Получить количество созданных экземпляров
 * 
 * @return Количество активных экземпляров в памяти
 */
uint8_t screen_get_instance_count(void);

/* =============================
 *  УПРАВЛЕНИЕ ГРУППОЙ ЭНКОДЕРА
 * ============================= */

/**
 * @brief Добавить виджет в группу энкодера текущего экрана
 * 
 * Удобная функция для ручного добавления виджетов в группу энкодера.
 * Используется когда автоматическое добавление не срабатывает или нужен
 * контроль над порядком элементов.
 * 
 * @param screen_id ID экрана (если NULL - используется текущий)
 * @param widget Виджет для добавления
 * @return ESP_OK при успехе
 * @return ESP_ERR_NOT_FOUND если экран не найден
 * @return ESP_ERR_INVALID_STATE если группа не создана
 */
esp_err_t screen_add_to_encoder_group(const char *screen_id, lv_obj_t *widget);

/**
 * @brief Рекурсивно добавить все интерактивные элементы виджета в группу энкодера
 * 
 * Полезно для сложных виджетов с глубокой иерархией.
 * 
 * @param screen_id ID экрана (если NULL - используется текущий)
 * @param widget Корневой виджет для рекурсивного обхода
 * @return Количество добавленных элементов
 */
int screen_add_widget_tree(const char *screen_id, lv_obj_t *widget);

/**
 * @brief Автоматически настроить группу энкодера для экрана
 * 
 * Универсальная функция для автоматического обхода всех виджетов экрана
 * и добавления интерактивных элементов в группу энкодера.
 * Заменяет необходимость ручного обхода виджетов в каждом on_show callback.
 * 
 * Поддерживаемые типы: кнопки, слайдеры, dropdown, checkbox и др.
 * Автоматически пропускает скрытые и неинтерактивные элементы.
 * 
 * @param screen_obj Объект экрана для обхода
 * @param group Группа энкодера для добавления элементов
 * @return Количество добавленных элементов (или отрицательное значение при ошибке)
 */
int screen_auto_setup_encoder_group(lv_obj_t *screen_obj, lv_group_t *group);

/**
 * @brief Добавить элементы главного экрана в правильном порядке
 * 
 * Добавляет элементы главного экрана в следующем порядке:
 * 1. Карточки датчиков (0-5)
 * 2. Кнопка SET
 * 
 * @param screen_obj Объект главного экрана
 * @param group Группа энкодера
 * @return Количество добавленных элементов
 */
int screen_lifecycle_add_main_screen_elements(lv_obj_t *screen_obj, lv_group_t *group);

#ifdef __cplusplus
}
#endif

