/**
 * @file intelligent_pid_card.h
 * @brief Виджет адаптивной карточки PID для dashboard
 * 
 * Отображает состояние PID контроллера с:
 * - Цветовой индикацией статуса
 * - Прогресс-баром до цели
 * - PID компонентами
 * - Адаптивной информацией
 */

#ifndef INTELLIGENT_PID_CARD_H
#define INTELLIGENT_PID_CARD_H

#include "lvgl.h"
#include "system_config.h"
#include "adaptive_pid.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Статус PID для цветовой индикации
 */
typedef enum {
    PID_STATUS_IDLE,            // Неактивен (серый)
    PID_STATUS_ACTIVE,          // Активная коррекция (желтый)
    PID_STATUS_LEARNING,        // Режим обучения (синий)
    PID_STATUS_PREDICTING,      // Упреждающая коррекция (фиолетовый)
    PID_STATUS_AUTO_TUNING,     // Автонастройка (оранжевый)
    PID_STATUS_TARGET_REACHED,  // Цель достигнута (зеленый)
    PID_STATUS_ERROR            // Ошибка (красный)
} pid_card_status_t;

/**
 * @brief Структура виджета карточки PID
 */
typedef struct {
    lv_obj_t *container;            // Контейнер карточки
    lv_obj_t *name_label;           // Название насоса
    lv_obj_t *status_indicator;     // Индикатор ON/OFF
    lv_obj_t *values_label;         // "7.2 → 6.5"
    lv_obj_t *progress_bar;         // Прогресс до цели
    lv_obj_t *pid_components_label; // "P:+0.4 I:-0.2 D:+0.1"
    lv_obj_t *adaptive_label;       // "Kp:2.1(↑)"
    lv_obj_t *trend_label;          // "Тренд: ↘"
    
    pump_index_t pump_idx;
    pid_card_status_t status;
    bool is_focused;
} intelligent_pid_card_t;

/**
 * @brief Создание виджета адаптивной карточки PID
 * 
 * @param parent Родительский контейнер
 * @param pump_idx Индекс насоса
 * @return Указатель на созданную карточку
 */
intelligent_pid_card_t* widget_intelligent_pid_card_create(lv_obj_t *parent, pump_index_t pump_idx);

/**
 * @brief Обновление данных карточки
 * 
 * @param card Указатель на карточку
 * @param current Текущее значение
 * @param target Целевое значение
 * @param p_term P компонента
 * @param i_term I компонента
 * @param d_term D компонента
 */
void widget_intelligent_pid_card_update(intelligent_pid_card_t *card,
                                         float current,
                                         float target,
                                         float p_term,
                                         float i_term,
                                         float d_term);

/**
 * @brief Установка статуса карточки
 * 
 * Изменяет цвет рамки в соответствии со статусом
 * 
 * @param card Указатель на карточку
 * @param status Статус PID
 */
void widget_intelligent_pid_card_set_status(intelligent_pid_card_t *card, pid_card_status_t status);

/**
 * @brief Установка фокуса
 * 
 * @param card Указатель на карточку
 * @param focused true - в фокусе, false - нет
 */
void widget_intelligent_pid_card_set_focused(intelligent_pid_card_t *card, bool focused);

/**
 * @brief Удаление виджета
 * 
 * @param card Указатель на карточку
 */
void widget_intelligent_pid_card_delete(intelligent_pid_card_t *card);

#ifdef __cplusplus
}
#endif

#endif // INTELLIGENT_PID_CARD_H

