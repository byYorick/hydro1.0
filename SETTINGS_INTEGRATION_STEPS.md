# 📝 Шаги интеграции экранов настроек

## Шаг 1: Обновить CMakeLists.txt

Откройте `components/lvgl_ui/CMakeLists.txt` и добавьте новые файлы:

```cmake
idf_component_register(
    SRCS 
        # ... существующие файлы ...
        
        # Settings screens - добавьте после других экранов
        "screens/settings/settings_main_screen.c"
        "screens/settings/settings_wifi_screen.c"
        "screens/settings/settings_iot_menu.c"
        "screens/settings/settings_mqtt_screen.c"
        "screens/settings/ALL_SETTINGS_SCREENS.c"
        
    INCLUDE_DIRS "."
    # ... остальное без изменений ...
)
```

## Шаг 2: Зарегистрировать экраны в lvgl_ui.c

Найдите файл `components/lvgl_ui/lvgl_ui.c`

### 2.1 Добавьте includes (после других includes экранов):

```c
// Settings screens
#include "screens/settings/settings_main_screen.h"
#include "screens/settings/settings_wifi_screen.h"
#include "screens/settings/settings_iot_menu.h"
#include "screens/settings/settings_mqtt_screen.h"
// Прототипы из ALL_SETTINGS_SCREENS.c
esp_err_t settings_telegram_screen_init(void);
esp_err_t settings_sd_screen_init(void);
esp_err_t settings_mesh_screen_init(void);
```

### 2.2 Вызовите init функции (в lvgl_ui_init или screen_manager_init):

Найдите место где регистрируются экраны и добавьте:

```c
// Регистрация экранов настроек
ESP_LOGI(TAG, "Registering settings screens...");
ESP_ERROR_CHECK(settings_main_screen_init());
ESP_ERROR_CHECK(settings_wifi_screen_init());
ESP_ERROR_CHECK(settings_iot_menu_init());
ESP_ERROR_CHECK(settings_mqtt_screen_init());
ESP_ERROR_CHECK(settings_telegram_screen_init());
ESP_ERROR_CHECK(settings_sd_screen_init());
ESP_ERROR_CHECK(settings_mesh_screen_init());
ESP_LOGI(TAG, "Settings screens registered successfully");
```

## Шаг 3: Обновить config_manager зависимости

Файл `components/config_manager/CMakeLists.txt` уже OK, но убедитесь:

```cmake
idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "."
    REQUIRES main ph_ec_controller
    PRIV_REQUIRES
        nvs_flash
)
```

## Шаг 4: Компиляция

```bash
# В ESP-IDF терминале
idf.py build
```

### Ожидаемый вывод:

```
[100/1234] Linking CXX executable hydroponics.elf
...
Project build complete.
```

### Если ошибки компиляции:

1. **Неизвестные функции spinbox:**
   - Убедитесь что LVGL v8+ (spinbox появился в v8)
   - Или замените spinbox на slider

2. **Отсутствует system_config.h:**
   - Проверьте что `main` в REQUIRES CMakeLists

3. **Неизвестные экраны:**
   - Проверьте что все init функции вызваны

## Шаг 5: Прошивка и тест

```bash
idf.py flash monitor
```

### Проверка:

1. На дисплее перейдите: **Main → System → Settings**
2. Вы увидите:
   ```
   Settings
   ├ Sensors
   ├ Pumps
   ├ WiFi
   ├ IoT
   ├ AI Control
   └ System
   ```

3. Зайдите в **IoT → MQTT**
4. Измените Broker URI
5. Нажмите **Save MQTT**
6. Увидите "MQTT Saved!"

7. Перезагрузите ESP32 - настройки сохранены!

## Шаг 6: Применение настроек

Настройки загружаются при старте в `app_main.c`:

```c
// В init_system_components():
ret = config_load(&g_system_config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to load configuration");
    return ret;
}

// Применяем IoT настройки
if (g_system_config.mqtt.enabled) {
    mqtt_client_config_t mqtt_cfg = {
        .broker_uri = g_system_config.mqtt.broker_uri,
        .client_id = g_system_config.mqtt.client_id,
        .username = g_system_config.mqtt.username,
        .password = g_system_config.mqtt.password,
        .keepalive = g_system_config.mqtt.keepalive,
        .auto_reconnect = g_system_config.mqtt.auto_reconnect,
    };
    mqtt_client_init(&mqtt_cfg);
}
```

## Шаг 7: Добавление новых настроек

Хотите добавить новый параметр? Легко!

### Пример: Добавить "MQTT QoS"

**1. В system_config.h:**
```c
typedef struct {
    // ... существующие поля ...
    uint8_t qos;  // 0, 1, или 2
} mqtt_config_t;
```

**2. В config_manager.c (defaults):**
```c
config->mqtt.qos = 1;
```

**3. В settings_mqtt_screen.c:**
```c
static lv_obj_t *spinbox_qos;

// В create:
spinbox_qos = lv_spinbox_create(cont);
lv_spinbox_set_range(spinbox_qos, 0, 2);
lv_spinbox_set_value(spinbox_qos, mqtt_cfg.qos);

// В save:
sys_config.mqtt.qos = lv_spinbox_get_value(spinbox_qos);
```

Готово! Новый параметр в UI и NVS.

## 🎓 Советы

### Отладка NVS

```c
// Очистка NVS для теста
nvs_flash_erase();
nvs_flash_init();
```

### Просмотр сохраненной конфигурации

```c
ESP_LOGI(TAG, "WiFi SSID: %s", config.wifi.ssid);
ESP_LOGI(TAG, "MQTT Broker: %s", config.mqtt.broker_uri);
ESP_LOGI(TAG, "Telegram enabled: %d", config.telegram.enabled);
```

### Размер NVS

Текущая структура `system_config_t` ~2KB.  
NVS по умолчанию имеет достаточно места.

## ✅ Чеклист завершения

- [x] system_config.h расширен IoT структурами
- [x] config_manager.c добавлены defaults
- [x] settings_main_screen создан
- [x] settings_wifi_screen создан
- [x] settings_iot_menu создан
- [x] settings_mqtt_screen создан
- [x] ALL_SETTINGS_SCREENS.c (Telegram/SD/Mesh) создан
- [x] system_menu обновлен (добавлен пункт Settings)
- [ ] Экраны зарегистрированы в lvgl_ui.c
- [ ] CMakeLists.txt обновлен
- [ ] Скомпилировано и протестировано

**Осталось:** Выполнить шаги 1-2 выше и протестировать!

## 🎊 Результат

**Полнофункциональная система настроек IoT прямо на дисплее ESP32!**

Все 50+ параметров редактируются через удобный UI с энкодером и сохраняются в NVS!

🌱 **Настраивайте гидропонику прямо на устройстве без перепрошивки!** 🌱

