# 📚 Примеры использования IoT системы

## Пример 1: Базовая интеграция в app_main.c

```c
#include "iot_integration.h"
#include "iot_config.h"

void app_main(void) {
    // Базовая инициализация ESP-IDF
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Инициализация IoT системы
    ESP_ERROR_CHECK(iot_system_init());
    ESP_ERROR_CHECK(iot_system_start());
    
    // Основной цикл
    while (1) {
        // Чтение датчиков (ваш код)
        float ph = read_ph_sensor();
        float ec = read_ec_sensor();
        float temp = read_temperature();
        
        // Публикация во все каналы (MQTT, SD, Telegram)
        iot_publish_sensor_data(ph, ec, temp, 65.0, 500, 450);
        
        // Проверка алармов
        if (ph < 5.5 || ph > 7.5) {
            iot_publish_alarm("ph_critical", "pH вне допустимого диапазона!", "critical");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## Пример 2: MQTT команды (callback)

```c
#include "mqtt_client.h"

void mqtt_command_handler(const mqtt_command_t *cmd, void *ctx) {
    ESP_LOGI("APP", "MQTT команда: %d", cmd->type);
    
    switch (cmd->type) {
        case MQTT_CMD_SET_PH_TARGET:
            // Парсим payload
            cJSON *root = cJSON_Parse(cmd->payload);
            if (root) {
                cJSON *value = cJSON_GetObjectItem(root, "value");
                if (value) {
                    float new_target = value->valuedouble;
                    ph_ec_controller_set_ph_target(new_target);
                    ESP_LOGI("APP", "Новый pH target: %.2f", new_target);
                }
                cJSON_Delete(root);
            }
            break;
            
        case MQTT_CMD_START_PUMP:
            // Запуск насоса
            cJSON *root2 = cJSON_Parse(cmd->payload);
            if (root2) {
                cJSON *pump_id = cJSON_GetObjectItem(root2, "pump_id");
                if (pump_id) {
                    peristaltic_pump_dose(pump_id->valueint, 10.0, 5.0);
                }
                cJSON_Delete(root2);
            }
            break;
            
        default:
            ESP_LOGW("APP", "Неизвестная команда");
            break;
    }
}

void app_main(void) {
    // ... инициализация ...
    
    // Регистрация обработчика команд
    mqtt_subscribe_commands(mqtt_command_handler, NULL);
}
```

## Пример 3: Telegram команды

```c
#include "telegram_bot.h"

void telegram_command_handler(const char *command, void *ctx) {
    ESP_LOGI("APP", "Telegram команда: %s", command);
    
    if (strcmp(command, "/status") == 0) {
        // Отправляем статус
        telegram_send_status(current_ph, current_ec, current_temp, "OK");
        
    } else if (strncmp(command, "/set_ph ", 8) == 0) {
        // Парсим значение
        float new_ph = atof(command + 8);
        ph_ec_controller_set_ph_target(new_ph);
        
        telegram_send_formatted("✅ pH target установлен: %.2f", new_ph);
        
    } else if (strcmp(command, "/calibrate") == 0) {
        // Запускаем калибровку
        telegram_send_message("🔧 Калибровка запущена...");
        // ... ваш код калибровки ...
        telegram_send_message("✅ Калибровка завершена");
    }
}

void app_main(void) {
    // ... инициализация ...
    
    telegram_register_command_callback(telegram_command_handler, NULL);
}
```

## Пример 4: SD-карта конфигурация

```c
#include "sd_storage.h"
#include "cJSON.h"

// Сохранение настроек на SD
void save_settings_to_sd(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "ph_target", 6.8);
    cJSON_AddNumberToObject(root, "ec_target", 1.5);
    cJSON_AddBoolToObject(root, "auto_control", true);
    
    char *json_str = cJSON_PrintUnformatted(root);
    sd_save_config("settings", json_str);
    free(json_str);
    cJSON_Delete(root);
    
    ESP_LOGI("APP", "Настройки сохранены на SD");
}

// Загрузка настроек с SD
void load_settings_from_sd(void) {
    char json_buffer[512];
    
    if (sd_load_config("settings", json_buffer, sizeof(json_buffer)) == ESP_OK) {
        cJSON *root = cJSON_Parse(json_buffer);
        if (root) {
            cJSON *ph_target = cJSON_GetObjectItem(root, "ph_target");
            cJSON *ec_target = cJSON_GetObjectItem(root, "ec_target");
            
            if (ph_target) {
                ph_ec_controller_set_ph_target(ph_target->valuedouble);
            }
            
            cJSON_Delete(root);
            ESP_LOGI("APP", "Настройки загружены с SD");
        }
    }
}
```

## Пример 5: AI коррекция с обратной связью

```c
#include "ai_controller.h"

void apply_ai_correction(void) {
    // Подготовка состояния
    ai_system_state_t state = {
        .current_ph = 7.2,
        .current_ec = 1.3,
        .target_ph = 6.8,
        .target_ec = 1.5,
        .temperature = 24.5,
        .time_since_last_correction = 600, // 10 минут
    };
    
    // Получение рекомендаций
    ai_dosage_prediction_t prediction;
    if (ai_predict_correction(&state, &prediction) == ESP_OK) {
        ESP_LOGI("APP", "AI рекомендует:");
        ESP_LOGI("APP", "  pH UP: %.1f мл", prediction.ph_up_ml);
        ESP_LOGI("APP", "  pH DOWN: %.1f мл", prediction.ph_down_ml);
        ESP_LOGI("APP", "  EC A: %.1f мл", prediction.ec_a_ml);
        ESP_LOGI("APP", "  Уверенность: %.1f%%", prediction.confidence * 100);
        
        // Применяем если уверенность высокая
        if (prediction.confidence > 0.7f) {
            if (prediction.ph_down_ml > 0.1f) {
                peristaltic_pump_dose(PUMP_PH_DOWN, prediction.ph_down_ml, 5.0);
                
                // Отправляем уведомление
                telegram_send_formatted("🔧 AI коррекция: pH DOWN %.1fмл", prediction.ph_down_ml);
            }
        }
    }
}
```

## Пример 6: Mesh slave узел (простой датчик)

```c
// Конфигурация для slave узла в iot_config.h:
#define MESH_ROLE MESH_ROLE_SLAVE
#define MESH_DEVICE_ID 2
#define IOT_MESH_ENABLED true
#define IOT_MQTT_ENABLED false

// Код slave узла:
void app_main(void) {
    // Минимальная инициализация
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Только mesh-сеть
    mesh_network_init(MESH_ROLE_SLAVE, 2);
    mesh_network_start();
    
    // Callback для команд от gateway
    mesh_register_command_callback(mesh_command_handler, NULL);
    
    // Основной цикл - отправка данных на gateway
    while (1) {
        float ph = read_local_ph_sensor();
        float ec = read_local_ec_sensor();
        
        mesh_sensor_data_t data = {
            .device_id = 2,
            .ph = ph,
            .ec = ec,
            .temperature = 25.0,
            .humidity = 60.0,
            .lux = 400,
            .co2 = 450,
            .timestamp = esp_timer_get_time() / 1000,
        };
        
        mesh_send_sensor_data(&data);
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Каждые 10 секунд
    }
}
```

## Пример 7: Task Scheduler - расширенное использование

```c
#include "task_scheduler.h"

// Условие для задачи
bool check_ph_out_of_range(void *arg) {
    float current_ph = *(float *)arg;
    return (current_ph < 6.0 || current_ph > 7.5);
}

// Callback задачи
void send_ph_alarm(void *arg) {
    telegram_send_alarm("ph_out_of_range", "pH вышел за пределы!", 
                        TELEGRAM_SEVERITY_WARNING);
}

void daily_report_callback(void *arg) {
    char report[512];
    // Формируем отчет...
    telegram_send_daily_report(report);
}

void app_main(void) {
    // ... инициализация ...
    
    task_scheduler_init();
    task_scheduler_start();
    
    // Интервальная задача: публикация каждые 5 секунд
    task_scheduler_add_task(1, 5, publish_sensors, NULL);
    
    // Ежедневная задача: отчет в 20:00
    task_scheduler_add_daily_task(2, 20, 0, daily_report_callback, NULL);
    
    // Условная задача: аларм если pH вне диапазона (проверка каждые 30 сек)
    static float ph_value = 6.8;
    task_scheduler_add_conditional_task(3, 30, check_ph_out_of_range, 
                                         send_ph_alarm, &ph_value);
    
    // Однократная задача: калибровка через 10 минут
    task_scheduler_add_once_task(4, 600, calibrate_sensors, NULL);
    
    // Основной цикл
    while (1) {
        task_scheduler_process();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Пример 8: Интеграция с Home Assistant

### Configuration.yaml

```yaml
mqtt:
  broker: 192.168.1.100
  port: 1883
  
sensor:
  - platform: mqtt
    name: "Гидропоника pH"
    state_topic: "hydro/hydro_gateway_001/sensors/ph"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "pH"
    
  - platform: mqtt
    name: "Гидропоника EC"
    state_topic: "hydro/hydro_gateway_001/sensors/ec"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "mS/cm"
    
  - platform: mqtt
    name: "Гидропоника температура"
    state_topic: "hydro/hydro_gateway_001/sensors/temp"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "°C"

automation:
  - alias: "Аларм критического pH"
    trigger:
      platform: mqtt
      topic: "hydro/hydro_gateway_001/alarms"
    condition:
      condition: template
      value_template: "{{ trigger.payload_json.severity == 'critical' }}"
    action:
      service: notify.mobile_app
      data:
        title: "🔴 Критический аларм"
        message: "{{ trigger.payload_json.message }}"
```

## Пример 9: Node-RED Flow

```json
[
  {
    "id": "mqtt_in",
    "type": "mqtt in",
    "topic": "hydro/+/sensors/#",
    "broker": "mqtt_broker"
  },
  {
    "id": "json_parse",
    "type": "json",
    "wires": [["debug", "influxdb_out"]]
  },
  {
    "id": "influxdb_out",
    "type": "influxdb out",
    "measurement": "sensors",
    "database": "hydroponics"
  }
]
```

## Пример 10: Grafana запрос

```sql
SELECT 
  mean("value") as "pH avg",
  max("value") as "pH max",
  min("value") as "pH min"
FROM "sensors"
WHERE 
  topic = 'hydro/hydro_gateway_001/sensors/ph'
  AND time >= now() - 24h
GROUP BY time(1h)
```

## Пример 11: Обработка данных с mesh-сети (gateway)

```c
#include "mesh_network.h"

void mesh_sensor_handler(uint8_t device_id, const mesh_sensor_data_t *data, void *ctx) {
    ESP_LOGI("APP", "Данные от slave узла %d:", device_id);
    ESP_LOGI("APP", "  pH: %.2f", data->ph);
    ESP_LOGI("APP", "  EC: %.2f", data->ec);
    
    // Публикуем в MQTT
    mqtt_publish_ph(data->ph, "ok");
    mqtt_publish_ec(data->ec, "ok");
    
    // Логируем на SD
    sd_sensor_record_t record = {
        .timestamp = time(NULL),
        .ph = data->ph,
        .ec = data->ec,
        .temperature = data->temperature,
        .humidity = data->humidity,
        .lux = data->lux,
        .co2 = data->co2,
    };
    sd_write_sensor_log(&record);
}

void app_main(void) {
    // ... инициализация ...
    
    // Регистрация callback для mesh данных
    mesh_register_sensor_callback(mesh_sensor_handler, NULL);
    
    // Регистрация slave узла (замените на реальный MAC)
    uint8_t slave_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    mesh_register_peer(slave_mac, 2);
}
```

## Пример 12: Отправка команды на slave узел

```c
void send_command_to_slave(uint8_t slave_id, const char *command) {
    mesh_command_t cmd = {
        .target_device = slave_id,
        .command_type = 1, // Пользовательский тип
        .param1 = 0,
        .param2 = 0,
        .timestamp = esp_timer_get_time() / 1000,
    };
    
    if (strcmp(command, "calibrate") == 0) {
        cmd.command_type = 10; // Калибровка
        mesh_send_command(slave_id, &cmd);
        
        telegram_send_formatted("📡 Команда калибровки отправлена узлу %d", slave_id);
    }
}
```

## Пример 13: Ежедневный отчет с статистикой

```c
void generate_daily_report(void *arg) {
    // Собираем статистику за день
    float ph_avg = calculate_average_ph_for_day();
    float ec_avg = calculate_average_ec_for_day();
    int corrections_count = get_corrections_count_today();
    
    char report[1024];
    snprintf(report, sizeof(report),
             "📊 *Дневной отчет за %s*\n\n"
             "📈 Средние значения:\n"
             "  pH: %.2f\n"
             "  EC: %.2f mS/cm\n"
             "  Температура: %.1f°C\n\n"
             "🔧 Коррекций сегодня: %d\n"
             "⚠️ Алармов: %d\n\n"
             "✅ Система работает нормально",
             get_today_date(),
             ph_avg,
             ec_avg,
             24.5,
             corrections_count,
             0);
    
    telegram_send_daily_report(report);
    
    // Также публикуем в MQTT
    mqtt_publish_telemetry(esp_timer_get_time() / 1000000,
                          esp_get_free_heap_size(),
                          get_cpu_usage());
}

void app_main(void) {
    // ... инициализация ...
    
    // Добавляем ежедневную задачу в 20:00
    task_scheduler_add_daily_task(100, 20, 0, generate_daily_report, NULL);
}
```

## Пример 14: Условная задача с AI

```c
bool should_run_ai_correction(void *arg) {
    float ph_error = fabs(current_ph - target_ph);
    float ec_error = fabs(current_ec - target_ec);
    
    // Запускаем AI только если есть значительное отклонение
    return (ph_error > 0.3 || ec_error > 0.2);
}

void run_ai_correction(void *arg) {
    ai_system_state_t state = {
        .current_ph = current_ph,
        .current_ec = current_ec,
        .target_ph = target_ph,
        .target_ec = target_ec,
        .temperature = current_temp,
        .time_since_last_correction = get_time_since_last_correction(),
    };
    
    ai_dosage_prediction_t prediction;
    if (ai_predict_correction(&state, &prediction) == ESP_OK) {
        // Применяем коррекцию
        apply_dosages(&prediction);
        
        // Логируем
        ESP_LOGI("APP", "AI коррекция применена с уверенностью %.1f%%", 
                 prediction.confidence * 100);
    }
}

void app_main(void) {
    // ... инициализация ...
    
    // Условная задача: проверка каждые 60 сек, запуск при отклонении
    task_scheduler_add_conditional_task(200, 60, 
                                        should_run_ai_correction, 
                                        run_ai_correction, NULL);
}
```

## Пример 15: Обработка алармов во всех каналах

```c
void check_alarms_and_notify(void) {
    // Проверка pH
    if (current_ph < 5.5) {
        const char *msg = "pH слишком низкий! Требуется коррекция.";
        
        // MQTT
        mqtt_publish_alarm("ph_low", msg, "critical");
        
        // Telegram
        telegram_send_alarm("pH критически низкий", msg, TELEGRAM_SEVERITY_CRITICAL);
        
        // SD лог
        sd_event_record_t event = {
            .timestamp = time(NULL),
        };
        strncpy(event.type, "ph_low", sizeof(event.type) - 1);
        strncpy(event.message, msg, sizeof(event.message) - 1);
        strncpy(event.severity, "critical", sizeof(event.severity) - 1);
        sd_write_event_log(&event);
        
        // Локальное UI уведомление
        notification_system(NOTIFICATION_ERROR, msg, NOTIF_SOURCE_SYSTEM);
    }
    
    // Проверка EC
    if (current_ec > 2.5) {
        iot_publish_alarm("ec_high", "EC слишком высокий!", "warning");
    }
}
```

## Пример 16: Статистика системы

```c
void print_iot_stats(void) {
    char buffer[1024];
    
    // IoT статистика
    iot_get_system_stats(buffer, sizeof(buffer));
    ESP_LOGI("APP", "%s", buffer);
    
    // AI статистика
    ai_get_stats(buffer, sizeof(buffer));
    ESP_LOGI("APP", "%s", buffer);
    
    // Task scheduler статистика
    task_scheduler_get_info(buffer, sizeof(buffer));
    ESP_LOGI("APP", "%s", buffer);
    
    // SD статистика
    sd_storage_stats_t sd_stats;
    if (sd_get_storage_stats(&sd_stats) == ESP_OK) {
        ESP_LOGI("APP", "SD Card:");
        ESP_LOGI("APP", "  Total: %llu MB", sd_stats.total_bytes / 1024 / 1024);
        ESP_LOGI("APP", "  Used: %llu MB", sd_stats.used_bytes / 1024 / 1024);
        ESP_LOGI("APP", "  Free: %llu MB", sd_stats.free_bytes / 1024 / 1024);
    }
}
```

## Итого: Полная интеграция

```c
void app_main(void) {
    // ========== Базовая инициализация ==========
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // ========== IoT система ==========
    ESP_ERROR_CHECK(iot_system_init());
    ESP_ERROR_CHECK(iot_system_start());
    
    // ========== Callbacks ==========
    mqtt_subscribe_commands(mqtt_command_handler, NULL);
    telegram_register_command_callback(telegram_command_handler, NULL);
    mesh_register_sensor_callback(mesh_sensor_handler, NULL);
    
    // ========== Task Scheduler ==========
    task_scheduler_init();
    task_scheduler_start();
    
    task_scheduler_add_task(1, 5, publish_sensors, NULL);
    task_scheduler_add_daily_task(2, 20, 0, daily_report, NULL);
    task_scheduler_add_conditional_task(3, 60, check_ph_alarm, send_alarm, NULL);
    
    // ========== Основной цикл ==========
    while (1) {
        task_scheduler_process();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**Все компоненты работают вместе! 🎉**

