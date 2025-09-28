# Исправление проблемы таймаута задачи Watchdog в проекте гидропоники на ESP32-S3

## Описание проблемы

Система испытывала таймауты задачи watchdog со следующей ошибкой:
```
E (35733) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (35733) task_wdt:  - IDLE1 (CPU 1)
E (35733) task_wdt: Tasks currently running:
E (35733) task_wdt: CPU 0: IDLE0
E (35733) task_wdt: CPU 1: lvgl_timer
```

Это указывало на то, что задача таймера LVGL выполнялась слишком долго, вызывая срабатывание задачи watchdog.

## Первоначальная причина

Задача таймера LVGL блокировалась на длительные периоды из-за:
1. Длительных таймаутов при попытке захвата мьютекса LVGL
2. Неэффективной обработки обратных вызовов таймера LVGL
3. Возможной конкуренции с другими задачами, пытающимися получить доступ к LVGL

## Реализованное решение

### 1. Оптимизирована задача таймера LVGL

Изменена задача таймера LVGL в [components/lvgl_main/lvgl_main.c](file:///c%3A/esp/hydro/hydro1.0/components/lvgl_main/lvgl_main.c) для:
- Использования очень коротких таймаутов при захвате мьютекса LVGL (10 мс)
- Реализации адаптивной задержки на основе рекомендаций LVGL
- Ограничения задержек для предотвращения таймаутов watchdog
- Поддержания резервного времени сна при неудачном захвате блокировки

```c
static void lvgl_timer_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL timer task started");
    
    uint32_t last_sleep_time = 0;
    
    while (1) {
        // Try to acquire the lock with a very short timeout to prevent blocking
        if (lvgl_lock(10)) {  // 10ms timeout
            // Run the LVGL timer handler
            uint32_t sleep_ms = lv_timer_handler();
            lvgl_unlock();
            
            // Use the sleep time suggested by LVGL, but with reasonable limits
            if (sleep_ms > 50) {
                sleep_ms = 50;  // Cap to 50ms to prevent watchdog issues
            } else if (sleep_ms < 5) {
                sleep_ms = 5;   // Minimum 5ms delay
            }
            
            last_sleep_time = sleep_ms;
        } else {
            // If we can't acquire the lock, use the last known good sleep time
            // but with a minimum to prevent busy waiting
            if (last_sleep_time < 5) {
                last_sleep_time = 10;
            }
        }
        
        // Delay based on LVGL's recommendation or last known good value
        vTaskDelay(pdMS_TO_TICKS(last_sleep_time));
    }
}
```

### 2. Оптимизирована задача обновления дисплея

Изменена задача обновления дисплея для:
- Использования разумных таймаутов при захвате мьютекса LVGL (500 мс)
- Добавления соответствующих задержек для снижения конкуренции
- Улучшения обработки ошибок

```c
static void display_update_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
        // Ожидание данных датчиков
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Received sensor data from queue: pH=%.2f, EC=%.2f, Temp=%.1f", 
                     sensor_data.ph, sensor_data.ec, sensor_data.temp);
            
            // Try to acquire the lock with a reasonable timeout
            if (!lvgl_lock(500)) {  // 500ms timeout
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            // Проверка, что LVGL система инициализирована
            if (lv_is_initialized()) {
                // Обновление отображения датчиков
                update_sensor_display(&sensor_data);
                ESP_LOGI(TAG, "Sensor display updated successfully");
            } else {
                ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
            }
            
            // Освобождение мьютекса
            lvgl_unlock();
        } else {
            ESP_LOGD(TAG, "No sensor data received within timeout");
        }
        
        // Small delay to prevent excessive CPU usage and reduce contention
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

## Конфигурация

Таймаут задачи watchdog настроен в [sdkconfig.defaults](file:///c%3A/esp/hydro/hydro1.0/sdkconfig.defaults) как:
```
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
```

Это обеспечивает таймаут в 10 секунд, чего должно быть достаточно для нормальной работы с оптимизированными задачами.

## Тестирование исправления

Для проверки работоспособности исправления:

1. Прошейте обновленную прошивку на ESP32-S3
2. Отслеживайте вывод в последовательный порт на наличие ошибок таймаута задачи watchdog
3. Эти ошибки больше не должны появляться
4. Убедитесь, что дисплей обновляется правильно и интерфейс остается отзывчивым
5. Подтвердите, что данные датчиков по-прежнему обрабатываются и отображаются

## Влияние на производительность

Исправление должно улучшить стабильность системы за счет:
- Предотвращения таймаутов задачи watchdog
- Снижения конкуренции между задачами LVGL
- Поддержания отзывчивого обновления интерфейса
- Сохранения всей существующей функциональности

## Альтернативные решения

Если проблемы сохраняются, рассмотрите:

1. **Увеличение таймаута watchdog**: Настройте `CONFIG_ESP_TASK_WDT_TIMEOUT_S` в [sdkconfig.defaults](file:///c%3A/esp/hydro/hydro1.0/sdkconfig.defaults)
2. **Настройка приоритетов задач**: Измените приоритеты задач для обеспечения правильного планирования
3. **Снижение нагрузки LVGL**: Оптимизируйте сложность интерфейса или частоту обновления
4. **Использование выделенных ядер ЦП**: Закрепите критические задачи за конкретными ядрами ЦП

## Заключение

Это исправление устраняет проблему таймаута задачи watchdog путем оптимизации задач таймера и обновления дисплея LVGL. Решение сохраняет всю существующую функциональность, обеспечивая лучшую координацию между задачами и предотвращая сбои системы из-за таймаутов watchdog.