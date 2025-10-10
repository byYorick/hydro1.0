# Руководство по созданию экранов настроек

## ✅ Уже создано

1. **system_config.h** - расширен для IoT (WiFi, MQTT, Telegram, SD, Mesh, AI)
2. **config_manager.c** - добавлены значения по умолчанию для IoT
3. **settings_main_screen** - главное меню настроек (6 категорий)
4. **settings_iot_menu** - подменю IoT (MQTT, Telegram, SD, Mesh)
5. **settings_wifi_screen** - полный экран настроек WiFi с сохранением

## 📋 Оставшиеся экраны для создания

### Приоритет 1: Основные датчики

**settings_sensors_screen.c** - Настройки датчиков (pH, EC, Temp)
```
- pH Target (textarea, 0.0-14.0)
- pH Alarm Low (textarea)
- pH Alarm High (textarea)
- EC Target (textarea, 0.0-5.0)
- EC Alarm Low
- EC Alarm High
- Temp Target (textarea, 0-50°C)
- Temp Alarm Low
- Temp Alarm High
- [Кнопка Сохранить]
```

### Приоритет 2: MQTT (критично для IoT)

**settings_mqtt_screen.c** - Настройки MQTT
```
- Enabled (switch)
- Broker URI (textarea) - "mqtt://192.168.1.100:1883"
- Client ID (textarea) - "hydro_gateway_001"
- Username (textarea)
- Password (textarea)
- Keepalive (число 30-300 сек)
- Auto Reconnect (switch)
- Publish Interval (число 1-60 сек)
- [Кнопка Сохранить]
```

### Приоритет 3: Telegram

**settings_telegram_screen.c** - Настройки Telegram
```
- Enabled (switch)
- Bot Token (textarea, длинный)
- Chat ID (textarea)
- Enable Commands (switch)
- Report Hour (число 0-23)
- Notify Critical (switch)
- Notify Warnings (switch)
- [Кнопка Test] - отправить тестовое сообщение
- [Кнопка Сохранить]
```

### Приоритет 4: SD Card

**settings_sd_screen.c** - Настройки SD-карты
```
- Enabled (switch)
- Log Interval (число 10-300 сек)
- Cleanup Days (число 7-90)
- Auto Sync (switch)
- SD Mode (dropdown: SPI/SDMMC_1BIT/SDMMC_4BIT)
- [Показать статистику] - свободное место
- [Кнопка Сохранить]
```

### Приоритет 5: Mesh Network

**settings_mesh_screen.c** - Настройки Mesh-сети
```
- Enabled (switch)
- Role (dropdown: Gateway/Slave)
- Device ID (число 1-254)
- Heartbeat Interval (число 10-300 сек)
- [Показать peers] - список подключенных узлов
- [Кнопка Сохранить]
```

### Приоритет 6: AI Controller

**settings_ai_screen.c** - Настройки AI
```
- Enabled (switch)
- Min Confidence (slider 0.0-1.0)
- Correction Interval (число 60-600 сек)
- Use ML Model (switch) - если false то PID
- [Показать статистику] - количество коррекций
- [Кнопка Сохранить]
```

### Приоритет 7: Pumps

**settings_pumps_screen.c** - Настройки насосов
```
Для каждого из 6 насосов:
- Name (label)
- Enabled (switch)
- Flow Rate (число 0.1-10.0 мл/сек)
- Concentration Factor (slider 0.5-2.0)
- [Кнопка Test] - запуск на 2 секунды
[Кнопка Сохранить все]
```

### Приоритет 8: System

**settings_system_screen.c** - Системные настройки
```
- Auto Control (switch)
- Device Name (textarea)
- Display Brightness (slider 0-100)
- Firmware Version (label, read-only)
- [Кнопка Factory Reset]
- [Кнопка Сохранить]
```

## 🎨 Шаблон для создания экрана настроек

```c
#include "esp_log.h"
#include "lvgl.h"
#include "config_manager.h"
#include "system_config.h"
#include "screen_manager/screen_manager.h"

static const char *TAG = "SETTINGS_XXX";

// UI элементы
static lv_obj_t *switch_enabled;
static lv_obj_t *textarea_param1;
// ... остальные ...
static lv_obj_t *btn_save;

// Локальная копия конфигурации
static xxx_config_t local_config;

static void save_settings(lv_event_t *e) {
    ESP_LOGI(TAG, "Saving settings...");
    
    // Загружаем полную конфигурацию
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load config");
        return;
    }
    
    // Обновляем нужную секцию
    sys_config.xxx.enabled = lv_obj_has_state(switch_enabled, LV_STATE_CHECKED);
    // ... остальные поля ...
    
    // Сохраняем
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI(TAG, "Settings saved");
        // Показать уведомление "Saved!"
    }
}

static lv_obj_t* settings_xxx_create(void *params) {
    // Загрузить текущую конфигурацию
    system_config_t sys_config;
    if (config_load(&sys_config) == ESP_OK) {
        local_config = sys_config.xxx;
    }
    
    // Создать UI с контейнером прокрутки
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    // ... создать все элементы UI ...
    
    // Кнопка сохранить
    btn_save = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_save, save_settings, LV_EVENT_CLICKED, NULL);
    
    return screen;
}

esp_err_t settings_xxx_screen_init(void) {
    screen_config_t config = {
        .id = "settings_xxx",
        .title = "XXX Settings",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot", // или settings_main
        .can_go_back = true,
        .lazy_load = true,
        .has_back_button = true,
        .create_fn = settings_xxx_create,
    };
    
    return screen_register(&config);
}
```

## 🔧 Регистрация всех экранов

В `lvgl_ui/lvgl_ui.c` добавить:

```c
#include "screens/settings/settings_main_screen.h"
#include "screens/settings/settings_wifi_screen.h"
#include "screens/settings/settings_iot_menu.h"
#include "screens/settings/settings_mqtt_screen.h"
#include "screens/settings/settings_telegram_screen.h"
#include "screens/settings/settings_sd_screen.h"
#include "screens/settings/settings_mesh_screen.h"
#include "screens/settings/settings_ai_screen.h"
#include "screens/settings/settings_pumps_screen.h"
#include "screens/settings/settings_sensors_screen.h"
#include "screens/settings/settings_system_screen.h"

esp_err_t lvgl_ui_init(...) {
    // ... существующий код ...
    
    // Регистрация экранов настроек
    settings_main_screen_init();
    settings_wifi_screen_init();
    settings_iot_menu_init();
    settings_mqtt_screen_init();
    settings_telegram_screen_init();
    settings_sd_screen_init();
    settings_mesh_screen_init();
    settings_ai_screen_init();
    settings_pumps_screen_init();
    settings_sensors_screen_init();
    settings_system_screen_init();
}
```

## 🔗 Добавить в system_menu

В `system_menu_screen.c` добавить пункт:

```c
{
    .text = "Settings",
    .icon = LV_SYMBOL_SETTINGS,
    .callback = on_settings_click,
    .user_data = NULL,
},

static void on_settings_click(lv_event_t *e) {
    screen_show("settings_main", NULL);
}
```

## 🎯 Очередность реализации

1. ✅ settings_main_screen - главное меню
2. ✅ settings_iot_menu - IoT подменю
3. ✅ settings_wifi_screen - WiFi (пример реализован)
4. ⏳ settings_mqtt_screen - MQTT (критично)
5. ⏳ settings_telegram_screen
6. ⏳ settings_sd_screen
7. ⏳ settings_mesh_screen
8. ⏳ settings_sensors_screen
9. ⏳ settings_ai_screen
10. ⏳ settings_pumps_screen
11. ⏳ settings_system_screen

**Всего: 11 экранов**

## 💡 Подсказки

### Работа с текстовыми полями
```c
// Создание
lv_obj_t *ta = lv_textarea_create(parent);
lv_textarea_set_text(ta, initial_value);
lv_textarea_set_max_length(ta, max_len);

// Чтение
const char *text = lv_textarea_get_text(ta);
```

### Switch (переключатель)
```c
lv_obj_t *sw = lv_switch_create(parent);
if (value) lv_obj_add_state(sw, LV_STATE_CHECKED);

// Чтение
bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
```

### Dropdown (выпадающий список)
```c
lv_obj_t *dd = lv_dropdown_create(parent);
lv_dropdown_set_options(dd, "Option 1\nOption 2\nOption 3");
lv_dropdown_set_selected(dd, index);

// Чтение
uint16_t selected = lv_dropdown_get_selected(dd);
```

### Slider (ползунок)
```c
lv_obj_t *slider = lv_slider_create(parent);
lv_slider_set_range(slider, 0, 100);
lv_slider_set_value(slider, value, LV_ANIM_OFF);

// Чтение
int32_t value = lv_slider_get_value(slider);
```

## 🔢 Числовой ввод через энкодер

Для ввода чисел создайте spinbox:

```c
lv_obj_t *spinbox = lv_spinbox_create(parent);
lv_spinbox_set_range(spinbox, 0, 100);
lv_spinbox_set_value(spinbox, current_value);
lv_spinbox_set_digit_format(spinbox, 3, 1); // 3 цифры, 1 после запятой
```

Продолжаю создавать остальные экраны...

