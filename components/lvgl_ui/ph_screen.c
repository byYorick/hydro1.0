#include "ph_screen.h"
#include "lvgl.h"
#include "lvgl_ui.h" // Подключаем заголовок, определяющий типы LVGL и функции работы с группами
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

// Подключаем русский шрифт
LV_FONT_DECLARE(montserrat_ru)

static const char *TAG = "PH_SCREEN";

// NVS ключи для параметров pH
#define PH_NVS_NAMESPACE "ph_params"
#define NVS_KEY_TARGET "target"
#define NVS_KEY_NOTIF_HIGH "notif_hi"
#define NVS_KEY_NOTIF_LOW "notif_lo"
#define NVS_KEY_PUMP_HIGH "pump_hi"
#define NVS_KEY_PUMP_LOW "pump_lo"
#define NVS_KEY_CAL1_REF "cal1_ref"
#define NVS_KEY_CAL1_RAW "cal1_raw"
#define NVS_KEY_CAL2_REF "cal2_ref"
#define NVS_KEY_CAL2_RAW "cal2_raw"
#define NVS_KEY_CAL3_REF "cal3_ref"
#define NVS_KEY_CAL3_RAW "cal3_raw"
#define NVS_KEY_CAL_VALID "cal_valid"

// Цветовая палитра (совместима с lvgl_ui.c)
#define COLOR_BG            lv_color_hex(0x0F1419)
#define COLOR_SURFACE       lv_color_hex(0x1A2332)
#define COLOR_CARD          lv_color_hex(0x2D3E50)
#define COLOR_ACCENT        lv_color_hex(0x00D4AA)
#define COLOR_TEXT          lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_MUTED    lv_color_hex(0xB0BEC5)
#define COLOR_WARNING       lv_color_hex(0xFF9800)
#define COLOR_DANGER        lv_color_hex(0xF44336)

// Глобальные переменные
static ph_params_t ph_params = {
    .current_value = 7.0f,
    .target_value = 6.5f,
    .notification_high = 7.2f,
    .notification_low = 5.8f,
    .pump_high = 7.5f,
    .pump_low = 5.5f,
    .cal_point1_ref = 4.0f,
    .cal_point1_raw = 4.0f,
    .cal_point2_ref = 7.0f,
    .cal_point2_raw = 7.0f,
    .cal_point3_ref = 10.0f,
    .cal_point3_raw = 10.0f,
    .calibration_valid = false
};

static lv_obj_t *detail_screen = NULL;
static lv_obj_t *settings_screen = NULL;
static lv_obj_t *calibration_screen = NULL;

// Группы для навигации энкодером
static lv_group_t *ph_detail_group = NULL;
static lv_group_t *ph_settings_group = NULL;
static lv_group_t *ph_calibration_group = NULL;

// Callback для возврата на главный экран
static ph_close_callback_t close_callback = NULL;

// Лейблы для отображения значений на детальном экране
static lv_obj_t *label_current = NULL;
static lv_obj_t *label_target = NULL;
static lv_obj_t *label_notif_high = NULL; // Используется для строки с обоими пределами уведомлений
static lv_obj_t *label_pump_high = NULL;  // Используется для строки с обоими пределами насосов

// Калибровка
static uint8_t calibration_step = 0;
static lv_obj_t *cal_status_label = NULL;
static lv_obj_t *cal_value_label = NULL;

/* =============================
 *  NVS ФУНКЦИИ
 * ============================= */

esp_err_t ph_save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PH_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка открытия NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Сохраняем все параметры как float (4 байта каждый)
    nvs_set_blob(nvs_handle, NVS_KEY_TARGET, &ph_params.target_value, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_NOTIF_HIGH, &ph_params.notification_high, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_NOTIF_LOW, &ph_params.notification_low, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_PUMP_HIGH, &ph_params.pump_high, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_PUMP_LOW, &ph_params.pump_low, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL1_REF, &ph_params.cal_point1_ref, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL1_RAW, &ph_params.cal_point1_raw, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL2_REF, &ph_params.cal_point2_ref, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL2_RAW, &ph_params.cal_point2_raw, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL3_REF, &ph_params.cal_point3_ref, sizeof(float));
    nvs_set_blob(nvs_handle, NVS_KEY_CAL3_RAW, &ph_params.cal_point3_raw, sizeof(float));
    
    uint8_t cal_valid = ph_params.calibration_valid ? 1 : 0;
    nvs_set_u8(nvs_handle, NVS_KEY_CAL_VALID, cal_valid);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Параметры pH сохранены в NVS");
    } else {
        ESP_LOGE(TAG, "Ошибка сохранения в NVS: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t ph_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PH_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS не открыт (возможно первый запуск): %s", esp_err_to_name(err));
        return err;
    }

    size_t size = sizeof(float);
    nvs_get_blob(nvs_handle, NVS_KEY_TARGET, &ph_params.target_value, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_NOTIF_HIGH, &ph_params.notification_high, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_NOTIF_LOW, &ph_params.notification_low, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_PUMP_HIGH, &ph_params.pump_high, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_PUMP_LOW, &ph_params.pump_low, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL1_REF, &ph_params.cal_point1_ref, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL1_RAW, &ph_params.cal_point1_raw, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL2_REF, &ph_params.cal_point2_ref, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL2_RAW, &ph_params.cal_point2_raw, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL3_REF, &ph_params.cal_point3_ref, &size);
    nvs_get_blob(nvs_handle, NVS_KEY_CAL3_RAW, &ph_params.cal_point3_raw, &size);
    
    uint8_t cal_valid = 0;
    if (nvs_get_u8(nvs_handle, NVS_KEY_CAL_VALID, &cal_valid) == ESP_OK) {
        ph_params.calibration_valid = (cal_valid != 0);
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Параметры pH загружены из NVS");
    return ESP_OK;
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================= */

esp_err_t ph_get_params(ph_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;
    *params = ph_params;
    return ESP_OK;
}

esp_err_t ph_set_params(const ph_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;
    
    // Сохраняем текущее значение (оно обновляется отдельно)
    float current = ph_params.current_value;
    ph_params = *params;
    ph_params.current_value = current;
    
    return ph_save_to_nvs();
}

esp_err_t ph_update_current_value(float value)
{
    ph_params.current_value = value;
    
    // Обновляем отображение на детальном экране
    if (detail_screen && label_current) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Сейчас: %.2f pH", value);
        lv_label_set_text(label_current, buf);
    }
    
    // Обновляем на экране калибровки
    if (calibration_screen && cal_value_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Измерено: %.2f", value);
        lv_label_set_text(cal_value_label, buf);
    }
    
    return ESP_OK;
}

/* =============================
 *  ОБРАБОТЧИКИ СОБЫТИЙ
 * ============================= */

static void btn_back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ph_close_screen();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            ph_close_screen();
        }
    }
}

static void btn_settings_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ph_show_settings_screen();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            ph_show_settings_screen();
        }
    }
}

static void btn_calibration_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ph_show_calibration_screen();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            ph_show_calibration_screen();
        }
    }
}

static void btn_save_settings_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ph_save_to_nvs();
        ph_show_detail_screen();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            ph_save_to_nvs();
            ph_show_detail_screen();
        }
    }
}

static void btn_cal_next_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED || 
        (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
        
        // Сохраняем текущую точку
        float ref_val = calibration_step == 1 ? 4.0f : (calibration_step == 2 ? 7.0f : 10.0f);
        ph_calibration_set_point(calibration_step, ref_val);
        
        calibration_step++;
        
        if (calibration_step > 3) {
            ph_calibration_finish();
            ESP_LOGI(TAG, "Калибровка завершена");
            ph_show_detail_screen();
        } else {
            // Обновляем статус калибровки
            if (cal_status_label) {
                char buf[48];
                snprintf(buf, sizeof(buf), "3 точки: 4.0, 7.0, 10.0\nТочка %d", calibration_step);
                lv_label_set_text(cal_status_label, buf);
            }
        }
    }
}

/* =============================
 *  СОЗДАНИЕ ЭКРАНОВ
 * ============================= */

static void create_detail_screen(void)
{
    if (detail_screen) {
        lv_obj_del(detail_screen);
        detail_screen = NULL;
    }

    // Создаем группу для энкодера
    if (ph_detail_group == NULL) {
        ph_detail_group = lv_group_create();
        lv_group_set_wrap(ph_detail_group, true);
    }
    lv_group_remove_all_objs(ph_detail_group);

    detail_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(detail_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(detail_screen, 8, 0);

    // Компактный заголовок
    lv_obj_t *header = lv_obj_create(detail_screen);
    lv_obj_set_size(header, lv_pct(100), 30);
    lv_obj_set_style_bg_color(header, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "pH");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_center(title);

    // Кнопка назад
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 30, 24);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 3, 0);
    lv_obj_set_style_outline_width(btn_back, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_back, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_detail_group, btn_back); // Добавляем в группу для энкодера
    
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Компактный контент
    lv_obj_t *content = lv_obj_create(detail_screen);
    lv_obj_set_size(content, lv_pct(100), 142);
    lv_obj_set_pos(content, 0, 36);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_radius(content, 4, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Компактная таблица параметров
    char buf[32];
    
    // Текущее
    snprintf(buf, sizeof(buf), "Сейчас: %.2f pH", ph_params.current_value);
    label_current = lv_label_create(content);
    lv_label_set_text(label_current, buf);
    lv_obj_set_style_text_color(label_current, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(label_current, &montserrat_ru, 0);
    
    // Целевое
    snprintf(buf, sizeof(buf), "Цель: %.2f", ph_params.target_value);
    label_target = lv_label_create(content);
    lv_label_set_text(label_target, buf);
    lv_obj_set_style_text_color(label_target, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label_target, &montserrat_ru, 0);
    
    // Разделитель
    lv_obj_t *div1 = lv_obj_create(content);
    lv_obj_set_size(div1, lv_pct(100), 1);
    lv_obj_set_style_bg_color(div1, COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(div1, 0, 0);
    lv_obj_set_style_pad_all(div1, 0, 0);
    
    // Уведомления
    snprintf(buf, sizeof(buf), "Увед: %.1f-%.1f", ph_params.notification_low, ph_params.notification_high);
    label_notif_high = lv_label_create(content);
    lv_label_set_text(label_notif_high, buf);
    lv_obj_set_style_text_color(label_notif_high, COLOR_WARNING, 0);
    lv_obj_set_style_text_font(label_notif_high, &montserrat_ru, 0);
    
    // Насосы
    snprintf(buf, sizeof(buf), "Насосы: %.1f-%.1f", ph_params.pump_low, ph_params.pump_high);
    label_pump_high = lv_label_create(content);
    lv_label_set_text(label_pump_high, buf);
    lv_obj_set_style_text_color(label_pump_high, COLOR_DANGER, 0);
    lv_obj_set_style_text_font(label_pump_high, &montserrat_ru, 0);

    // Компактные кнопки внизу
    lv_obj_t *btn_settings = lv_btn_create(detail_screen);
    lv_obj_set_size(btn_settings, 100, 32);
    lv_obj_set_style_outline_width(btn_settings, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_settings, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn_settings, 2, LV_STATE_FOCUSED);
    lv_obj_set_pos(btn_settings, 8, 186);
    lv_obj_set_style_bg_color(btn_settings, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_settings, 4, 0);
    // Визуальный эффект фокуса
    lv_obj_set_style_outline_width(btn_settings, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_settings, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn_settings, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_settings, btn_settings_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_detail_group, btn_settings);
    
    lv_obj_t *lbl_btn1 = lv_label_create(btn_settings);
    lv_label_set_text(lbl_btn1, "Настр.");
    lv_obj_set_style_text_font(lbl_btn1, &montserrat_ru, 0);
    lv_obj_center(lbl_btn1);

    lv_obj_t *btn_cal = lv_btn_create(detail_screen);
    lv_obj_set_size(btn_cal, 100, 32);
    lv_obj_set_style_outline_width(btn_cal, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_cal, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn_cal, 2, LV_STATE_FOCUSED);
    lv_obj_set_pos(btn_cal, 114, 186);
    lv_obj_set_style_bg_color(btn_cal, COLOR_WARNING, 0);
    lv_obj_set_style_radius(btn_cal, 4, 0);
    lv_obj_add_event_cb(btn_cal, btn_calibration_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_detail_group, btn_cal);
    
    lv_obj_t *lbl_btn2 = lv_label_create(btn_cal);
    lv_label_set_text(lbl_btn2, "Калибр.");
    lv_obj_set_style_text_font(lbl_btn2, &montserrat_ru, 0);
    lv_obj_center(lbl_btn2);
}

static void create_settings_screen(void)
{
    if (settings_screen) {
        lv_obj_del(settings_screen);
        settings_screen = NULL;
    }

    // Создаем группу для энкодера
    if (ph_settings_group == NULL) {
        ph_settings_group = lv_group_create();
        lv_group_set_wrap(ph_settings_group, true);
    }
    lv_group_remove_all_objs(ph_settings_group);

    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(settings_screen, 8, 0);

    // Компактный заголовок
    lv_obj_t *header = lv_obj_create(settings_screen);
    lv_obj_set_size(header, lv_pct(100), 30);
    lv_obj_set_style_bg_color(header, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Настройки pH");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_center(title);

    // Кнопка назад
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 30, 24);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 3, 0);
    lv_obj_set_style_outline_width(btn_back, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_back, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_settings_group, btn_back);
    
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Компактный контент
    lv_obj_t *content = lv_obj_create(settings_screen);
    lv_obj_set_size(content, lv_pct(100), 150);
    lv_obj_set_pos(content, 0, 36);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_radius(content, 4, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Компактный список параметров
    const char *items[] = {
        "Цель: %.1f",
        "Увед макс: %.1f",
        "Увед мин: %.1f",
        "pH- при: %.1f",
        "pH+ при: %.1f"
    };
    
    float values[] = {
        ph_params.target_value,
        ph_params.notification_high,
        ph_params.notification_low,
        ph_params.pump_high,
        ph_params.pump_low
    };

    for (int i = 0; i < 5; i++) {
        lv_obj_t *row = lv_btn_create(content);
        lv_obj_set_size(row, lv_pct(100), 32);
        lv_obj_set_style_bg_color(row, COLOR_CARD, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_outline_width(row, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_outline_color(row, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
        lv_obj_set_style_outline_pad(row, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_flex_main_place(row, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, 0);
        lv_group_add_obj(ph_settings_group, row);

        lv_obj_t *title_label = lv_label_create(row);
        lv_obj_set_style_text_color(title_label, COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_font(title_label, &montserrat_ru, 0);
        lv_label_set_text_fmt(title_label, "%s", items[i]);

        lv_obj_t *value_label = lv_label_create(row);
        lv_obj_set_style_text_color(value_label, COLOR_TEXT, 0);
        lv_obj_set_style_text_font(value_label, &montserrat_ru, 0);
        lv_label_set_text_fmt(value_label, "%.1f", values[i]);
    }

    // Кнопка сохранения
    lv_obj_t *btn_save = lv_btn_create(settings_screen);
    lv_obj_set_size(btn_save, 150, 32);
    lv_obj_set_pos(btn_save, 45, 192);
    lv_obj_set_style_bg_color(btn_save, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_save, 4, 0);
    lv_obj_set_style_outline_width(btn_save, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_save, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn_save, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_save, btn_save_settings_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_settings_group, btn_save);
    
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Сохранить");
    lv_obj_set_style_text_font(lbl_save, &montserrat_ru, 0);
    lv_obj_center(lbl_save);
}

static void create_calibration_screen(void)
{
    if (calibration_screen) {
        lv_obj_del(calibration_screen);
        calibration_screen = NULL;
    }

    // Создаем группу для энкодера
    if (ph_calibration_group == NULL) {
        ph_calibration_group = lv_group_create();
        lv_group_set_wrap(ph_calibration_group, true);
    }
    lv_group_remove_all_objs(ph_calibration_group);

    calibration_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(calibration_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(calibration_screen, 8, 0);

    // Компактный заголовок
    lv_obj_t *header = lv_obj_create(calibration_screen);
    lv_obj_set_size(header, lv_pct(100), 30);
    lv_obj_set_style_bg_color(header, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Калибровка pH");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_center(title);

    // Кнопка назад
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 30, 24);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 3, 0);
    lv_obj_set_style_outline_width(btn_back, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_back, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_calibration_group, btn_back);
    
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Компактный контент
    lv_obj_t *content = lv_obj_create(calibration_screen);
    lv_obj_set_size(content, lv_pct(100), 150);
    lv_obj_set_pos(content, 0, 36);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_radius(content, 4, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Статус калибровки
    cal_status_label = lv_label_create(content);
    lv_label_set_text(cal_status_label, "3 точки: 4.0, 7.0, 10.0\nТочка 1");
    lv_obj_set_style_text_color(cal_status_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(cal_status_label, &montserrat_ru, 0);
    lv_obj_set_style_text_align(cal_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(cal_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cal_status_label, lv_pct(95));

    // Текущее измеренное значение
    cal_value_label = lv_label_create(content);
    lv_label_set_text_fmt(cal_value_label, "Измерено: %.2f", ph_params.current_value);
    lv_obj_set_style_text_color(cal_value_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(cal_value_label, &montserrat_ru, 0);

    // Краткая инструкция
    lv_obj_t *instruction = lv_label_create(content);
    lv_label_set_text(instruction, "Поместите в буфер\nДождитесь стабилизации");
    lv_obj_set_style_text_color(instruction, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(instruction, &montserrat_ru, 0);
    lv_obj_set_style_text_align(instruction, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(instruction, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(instruction, lv_pct(95));

    // Кнопка "Далее"
    lv_obj_t *btn_next = lv_btn_create(calibration_screen);
    lv_obj_set_size(btn_next, 150, 32);
    lv_obj_set_pos(btn_next, 45, 192);
    lv_obj_set_style_bg_color(btn_next, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_next, 4, 0);
    lv_obj_set_style_outline_width(btn_next, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_next, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn_next, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_next, btn_cal_next_event_cb, LV_EVENT_ALL, NULL);
    lv_group_add_obj(ph_calibration_group, btn_next);
    
    lv_obj_t *lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, "Далее");
    lv_obj_set_style_text_font(lbl_next, &montserrat_ru, 0);
    lv_obj_center(lbl_next);

    calibration_step = 1;
}

/* =============================
 *  УПРАВЛЕНИЕ ЭКРАНАМИ
 * ============================= */

esp_err_t ph_show_detail_screen(void)
{
    if (!detail_screen) {
        create_detail_screen();
    }

    lv_screen_load_anim(detail_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);

    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, ph_detail_group);
            // Устанавливаем фокус на первый объект в группе
            if (ph_detail_group && lv_group_get_obj_count(ph_detail_group) > 0) {
                lv_group_focus_next(ph_detail_group);
            }
            break;
        }
        indev = lv_indev_get_next(indev);
    }
 
    return ESP_OK;
}

esp_err_t ph_show_settings_screen(void)
{
    if (!settings_screen) {
        create_settings_screen();
    }
    
    lv_screen_load_anim(settings_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    
    // Устанавливаем группу для энкодера
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, ph_settings_group);
            // Устанавливаем фокус на первый объект в группе
            if (ph_settings_group && lv_group_get_obj_count(ph_settings_group) > 0) {
                lv_group_focus_next(ph_settings_group);
            }
            break;
        }
        indev = lv_indev_get_next(indev);
    }
 
    return ESP_OK;
}

esp_err_t ph_show_calibration_screen(void)
{
    if (!calibration_screen) {
        create_calibration_screen();
    }
    
    calibration_step = 1;
    lv_screen_load_anim(calibration_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    
    // Устанавливаем группу для энкодера
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, ph_calibration_group);
            // Устанавливаем фокус на первый объект в группе
            if (ph_calibration_group && lv_group_get_obj_count(ph_calibration_group) > 0) {
                lv_group_focus_next(ph_calibration_group);
            }
            break;
        }
        indev = lv_indev_get_next(indev);
    }
 
    return ESP_OK;
}

esp_err_t ph_close_screen(void)
{
    // Вызываем callback для возврата на главный экран
    if (close_callback) {
        close_callback();
    }
    
    return ESP_OK;
}

void ph_set_close_callback(ph_close_callback_t callback)
{
    close_callback = callback;
}

/* =============================
 *  КАЛИБРОВКА
 * ============================= */

esp_err_t ph_calibration_start(void)
{
    calibration_step = 0;
    ESP_LOGI(TAG, "Калибровка pH начата");
    return ESP_OK;
}

esp_err_t ph_calibration_set_point(uint8_t point_num, float reference_value)
{
    if (point_num < 1 || point_num > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    float measured_value = ph_params.current_value;

    switch (point_num) {
        case 1:
            ph_params.cal_point1_ref = reference_value;
            ph_params.cal_point1_raw = measured_value;
            ESP_LOGI(TAG, "Точка 1: эталон=%.2f, измерено=%.2f", reference_value, measured_value);
            break;
        case 2:
            ph_params.cal_point2_ref = reference_value;
            ph_params.cal_point2_raw = measured_value;
            ESP_LOGI(TAG, "Точка 2: эталон=%.2f, измерено=%.2f", reference_value, measured_value);
            break;
        case 3:
            ph_params.cal_point3_ref = reference_value;
            ph_params.cal_point3_raw = measured_value;
            ESP_LOGI(TAG, "Точка 3: эталон=%.2f, измерено=%.2f", reference_value, measured_value);
            break;
    }

    return ESP_OK;
}

esp_err_t ph_calibration_finish(void)
{
    // Проверяем что все 3 точки заданы
    if (calibration_step >= 3) {
        ph_params.calibration_valid = true;
        ESP_LOGI(TAG, "Калибровка pH завершена успешно");
        return ph_save_to_nvs();
    }
    
    ESP_LOGW(TAG, "Калибровка не завершена (шаг %d/3)", calibration_step);
    return ESP_ERR_INVALID_STATE;
}

esp_err_t ph_calibration_cancel(void)
{
    calibration_step = 0;
    ESP_LOGI(TAG, "Калибровка pH отменена");
    return ESP_OK;
}

lv_group_t *ph_get_detail_group(void)
{
    return ph_detail_group;
}

lv_obj_t *ph_get_detail_screen(void)
{
    return detail_screen;
}

lv_group_t *ph_get_settings_group(void)
{
    return ph_settings_group;
}

lv_obj_t *ph_get_settings_screen(void)
{
    return settings_screen;
}

lv_group_t *ph_get_calibration_group(void)
{
    return ph_calibration_group;
}

lv_obj_t *ph_get_calibration_screen(void)
{
    return calibration_screen;
}

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================= */

esp_err_t ph_screen_init(void)
{
    ESP_LOGI(TAG, "Инициализация экранов pH");
    
    // Загружаем параметры из NVS
    esp_err_t err = ph_load_from_nvs();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Используем параметры по умолчанию");
        // Сохраняем значения по умолчанию
        ph_save_to_nvs();
    }

    ESP_LOGI(TAG, "Экраны pH инициализированы");
    ESP_LOGI(TAG, "  Целевое: %.2f", ph_params.target_value);
    ESP_LOGI(TAG, "  Уведомления: %.2f - %.2f", ph_params.notification_low, ph_params.notification_high);
    ESP_LOGI(TAG, "  Насосы: %.2f - %.2f", ph_params.pump_low, ph_params.pump_high);
    
    return ESP_OK;
}

