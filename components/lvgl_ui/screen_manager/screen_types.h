#pragma once

/**
 * @file screen_types.h
 * @brief Типы данных для Screen Manager System
 * 
 * Определяет все типы данных, используемые системой управления экранами:
 * - Конфигурация экранов
 * - Экземпляры экранов  
 * - Менеджер экранов
 * - Callback типы
 * 
 * @author Hydroponics Monitor Team
 * @date 2025-10-08
 * @version 1.0
 */

#include "lvgl.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  КОНСТАНТЫ
 * ============================= */
#define MAX_SCREENS         40    // Максимум зарегистрированных экранов
#define MAX_INSTANCES       15    // Максимум одновременно созданных экземпляров
#define MAX_HISTORY         10    // Глубина истории навигации
#define MAX_CHILDREN        8     // Максимум дочерних экранов у одного родителя
#define MAX_SCREEN_ID_LEN   32    // Максимальная длина ID экрана

/* =============================
 *  КАТЕГОРИИ ЭКРАНОВ
 * ============================= */

/**
 * @brief Категории экранов для классификации и применения шаблонов
 */
typedef enum {
    SCREEN_CATEGORY_MAIN,         ///< Главный экран системы
    SCREEN_CATEGORY_DETAIL,       ///< Экран детализации данных
    SCREEN_CATEGORY_SETTINGS,     ///< Экран настроек
    SCREEN_CATEGORY_MENU,         ///< Экран меню с списком опций
    SCREEN_CATEGORY_FORM,         ///< Экран формы ввода данных
    SCREEN_CATEGORY_DIALOG,       ///< Диалоговое окно
    SCREEN_CATEGORY_INFO,         ///< Информационный экран
} screen_category_t;

/* =============================
 *  FORWARD DECLARATIONS
 * ============================= */

// Forward declaration для взаимных ссылок
struct screen_instance_t;
typedef struct screen_instance_t screen_instance_t;

/* =============================
 *  CALLBACK ТИПЫ
 * ============================= */

/**
 * @brief Функция создания экрана
 * @param params Параметры создания (опционально)
 * @return Созданный LVGL объект экрана или NULL при ошибке
 */
typedef lv_obj_t* (*screen_create_fn_t)(void *params);

/**
 * @brief Функция уничтожения экрана
 * @param screen_obj LVGL объект экрана
 * @return ESP_OK при успехе
 */
typedef esp_err_t (*screen_destroy_fn_t)(lv_obj_t *screen_obj);

/**
 * @brief Callback при показе экрана
 * @param screen_obj LVGL объект экрана
 * @param params Параметры показа
 * @return ESP_OK при успехе
 */
typedef esp_err_t (*screen_show_fn_t)(lv_obj_t *screen_obj, void *params);

/**
 * @brief Callback при скрытии экрана
 * @param screen_obj LVGL объект экрана
 * @return ESP_OK при успехе
 */
typedef esp_err_t (*screen_hide_fn_t)(lv_obj_t *screen_obj);

/**
 * @brief Callback при обновлении данных экрана
 * @param screen_obj LVGL объект экрана
 * @param data Данные для обновления
 * @return ESP_OK при успехе
 */
typedef esp_err_t (*screen_update_fn_t)(lv_obj_t *screen_obj, void *data);

/**
 * @brief Функция проверки возможности показа экрана
 * @return true если экран можно показать
 */
typedef bool (*screen_can_show_fn_t)(void);

/* =============================
 *  КОНФИГУРАЦИЯ ЭКРАНА
 * ============================= */

/**
 * @brief Декларативная конфигурация экрана
 * 
 * Описывает все свойства и поведение экрана в одном месте.
 * Экран регистрируется один раз при инициализации системы.
 */
typedef struct {
    // ===== ИДЕНТИФИКАЦИЯ =====
    char id[MAX_SCREEN_ID_LEN];   ///< Уникальный ID экрана (например, "main", "detail_ph")
    const char *title;             ///< Заголовок экрана для отображения
    screen_category_t category;    ///< Категория экрана
    
    // ===== НАВИГАЦИЯ =====
    char parent_id[MAX_SCREEN_ID_LEN];  ///< ID родительского экрана (для кнопки "Назад")
    bool can_go_back;              ///< Разрешен ли возврат назад
    bool is_root;                  ///< Является ли корневым экраном (обычно только "main")
    
    // ===== ЖИЗНЕННЫЙ ЦИКЛ =====
    bool lazy_load;                ///< Создавать при первом показе (экономия памяти)
    bool cache_on_hide;            ///< Сохранять в памяти при скрытии
    bool destroy_on_hide;          ///< Уничтожать при скрытии (освобождение памяти)
    uint32_t cache_timeout_ms;     ///< Таймаут кэша в мс (0 = бесконечно)
    
    // ===== UI ОПЦИИ =====
    bool has_status_bar;           ///< Включить статус-бар
    bool has_back_button;          ///< Включить кнопку назад
    
    // ===== CALLBACKS =====
    screen_create_fn_t create_fn;  ///< Обязательно: функция создания UI
    screen_destroy_fn_t destroy_fn; ///< Опционально: custom уничтожение
    screen_show_fn_t on_show;      ///< Опционально: callback при показе
    screen_hide_fn_t on_hide;      ///< Опционально: callback при скрытии
    screen_update_fn_t on_update;  ///< Опционально: callback при обновлении данных
    screen_can_show_fn_t can_show_fn; ///< Опционально: проверка прав/условий
    
    // ===== ПОЛЬЗОВАТЕЛЬСКИЕ ДАННЫЕ =====
    void *user_data;               ///< Произвольные данные для передачи в callbacks
} screen_config_t;

/* =============================
 *  ЭКЗЕМПЛЯР ЭКРАНА
 * ============================= */

/**
 * @brief Экземпляр экрана в runtime
 * 
 * Создается когда экран впервые показывается или создается вручную.
 * Содержит реальные LVGL объекты и состояние выполнения.
 */
struct screen_instance_t {
    // ===== КОНФИГУРАЦИЯ =====
    screen_config_t *config;       ///< Ссылка на конфигурацию (из реестра)
    
    // ===== LVGL ОБЪЕКТЫ =====
    lv_obj_t *screen_obj;          ///< Корневой LVGL объект экрана
    lv_group_t *encoder_group;     ///< Группа для навигации энкодером
    
    // ===== СОСТОЯНИЕ =====
    bool is_created;               ///< Создан ли экземпляр
    bool is_visible;               ///< Виден ли экран сейчас
    bool is_cached;                ///< Находится ли в кэше
    
    // ===== ВРЕМЕННЫЕ МЕТКИ =====
    uint32_t create_time;          ///< Timestamp создания (мс)
    uint32_t last_show_time;       ///< Timestamp последнего показа (мс)
    uint32_t cache_time;           ///< Timestamp кэширования (мс)
    
    // ===== НАВИГАЦИОННОЕ ДЕРЕВО =====
    screen_instance_t *parent;     ///< Родительский экземпляр
    screen_instance_t *children[MAX_CHILDREN]; ///< Дочерние экземпляры
    uint8_t children_count;        ///< Количество дочерних
    
    // ===== ПАРАМЕТРЫ RUNTIME =====
    void *show_params;             ///< Параметры последнего показа
};

/* =============================
 *  КОНФИГУРАЦИЯ МЕНЕДЖЕРА
 * ============================= */

/**
 * @brief Конфигурация Screen Manager
 */
typedef struct {
    // ===== КЭШИРОВАНИЕ =====
    bool enable_cache;             ///< Включить кэширование экранов
    uint8_t max_cache_size;        ///< Максимум экранов в кэше
    
    // ===== ИСТОРИЯ =====
    bool enable_history;           ///< Включить историю навигации
    
    // ===== АНИМАЦИИ =====
    uint32_t transition_time;      ///< Время перехода между экранами (мс)
    bool enable_animations;        ///< Включить анимации переходов
} screen_manager_config_t;

/* =============================
 *  МЕНЕДЖЕР ЭКРАНОВ
 * ============================= */

/**
 * @brief Главная структура менеджера экранов
 * 
 * Singleton - единственный экземпляр в системе.
 * Содержит все зарегистрированные экраны, активные экземпляры,
 * историю навигации и конфигурацию.
 */
typedef struct {
    // ===== РЕЕСТР =====
    screen_config_t *screens[MAX_SCREENS];  ///< Массив конфигураций экранов
    uint8_t screen_count;                    ///< Количество зарегистрированных
    
    // ===== АКТИВНЫЕ ЭКЗЕМПЛЯРЫ =====
    screen_instance_t *instances[MAX_INSTANCES]; ///< Массив активных экземпляров
    uint8_t instance_count;                      ///< Количество созданных
    
    // ===== ТЕКУЩЕЕ СОСТОЯНИЕ =====
    screen_instance_t *current_screen;      ///< Текущий видимый экран
    
    // ===== ИСТОРИЯ НАВИГАЦИИ =====
    screen_instance_t *history[MAX_HISTORY]; ///< Стек истории
    uint8_t history_index;                   ///< Текущий индекс в истории
    uint8_t history_count;                   ///< Количество записей в истории
    
    // ===== КОНФИГУРАЦИЯ =====
    screen_manager_config_t config;         ///< Конфигурация менеджера
    
    // ===== СОСТОЯНИЕ ИНИЦИАЛИЗАЦИИ =====
    bool is_initialized;                    ///< Инициализирован ли менеджер
    
    // ===== THREAD SAFETY =====
    SemaphoreHandle_t mutex;                ///< Мьютекс для потокобезопасности
} screen_manager_t;

#ifdef __cplusplus
}
#endif

