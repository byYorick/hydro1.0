# Система обработки ошибок (Error Handler)

## Описание

Централизованная система обработки ошибок с автоматическим выводом уведомлений на экран через LVGL.

## Возможности

- ✅ **Централизованная обработка** всех ошибок системы
- ✅ **Категоризация** ошибок по типам (I2C, датчики, дисплей, и т.д.)
- ✅ **Уровни критичности** (Debug, Info, Warning, Error, Critical)
- ✅ **Всплывающие окна** на экране с цветовой индикацией
- ✅ **Интеграция** с системой уведомлений
- ✅ **Статистика** по ошибкам
- ✅ **Логирование** через ESP_LOG

## Использование

### Инициализация

```c
#include "error_handler.h"

// Объявление русского шрифта (если используется)
LV_FONT_DECLARE(montserrat_ru);

void app_main(void) {
    // Инициализация с включенными всплывающими окнами
    error_handler_init(true);
    
    // Установка русского шрифта для поддержки кириллицы
    error_handler_set_font(&montserrat_ru);
}
```

**Важно:** Для корректного отображения русских сообщений об ошибках необходимо:
1. Использовать шрифт с поддержкой кириллицы (Unicode диапазон 0x400-0x4FF)
2. Установить шрифт через `error_handler_set_font()` после инициализации
3. Шрифт должен быть объявлен через `LV_FONT_DECLARE()` и скомпилирован в проект

### Базовое использование

```c
#include "error_handler.h"

// Отчет об ошибке
error_handler_report(
    ERROR_CATEGORY_SENSOR,      // Категория
    ERROR_LEVEL_ERROR,          // Уровень
    ESP_ERR_TIMEOUT,           // Код ошибки
    "SHT3X",                   // Компонент
    "Не удалось прочитать данные с датчика"
);
```

### Использование макросов (рекомендуется)

```c
// Предупреждение
ERROR_WARN(ERROR_CATEGORY_I2C, "i2c_bus", 
          "Медленный отклик устройства 0x%02X", device_addr);

// Ошибка с кодом
ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, "SHT3X", 
            "Ошибка чтения температуры");

// Критическая ошибка
ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, "MAIN", 
              "Недостаточно памяти для инициализации");

// Автоматическая проверка и обработка ошибки I2C
esp_err_t err = i2c_master_transmit(...);
ERROR_CHECK_I2C(err, "i2c_bus", "Ошибка передачи на 0x%02X", addr);

// Автоматическая проверка и обработка ошибки датчика
esp_err_t err = sht3x_read_temperature(...);
ERROR_CHECK_SENSOR(err, "SHT3X", "Не удалось прочитать температуру");
```

## Категории ошибок

- `ERROR_CATEGORY_I2C` - Ошибки шины I2C
- `ERROR_CATEGORY_SENSOR` - Ошибки датчиков
- `ERROR_CATEGORY_DISPLAY` - Ошибки дисплея
- `ERROR_CATEGORY_STORAGE` - Ошибки NVS
- `ERROR_CATEGORY_SYSTEM` - Системные ошибки
- `ERROR_CATEGORY_PUMP` - Ошибки насосов
- `ERROR_CATEGORY_RELAY` - Ошибки реле
- `ERROR_CATEGORY_CONTROLLER` - Ошибки контроллеров
- `ERROR_CATEGORY_NETWORK` - Сетевые ошибки
- `ERROR_CATEGORY_OTHER` - Прочие

## Уровни критичности

- `ERROR_LEVEL_DEBUG` - Отладочная информация (не показывается на экране)
- `ERROR_LEVEL_INFO` - Информация (не показывается на экране)
- `ERROR_LEVEL_WARNING` - Предупреждение (уведомление, оранжевый цвет)
- `ERROR_LEVEL_ERROR` - Ошибка (всплывающее окно, красный цвет)
- `ERROR_LEVEL_CRITICAL` - Критическая ошибка (всплывающее окно на 10 сек, темно-красный)

## Цветовая индикация

- 🔵 **Синий** - Debug/Info
- 🟢 **Зеленый** - Информация
- 🟠 **Оранжевый** - Предупреждение
- 🔴 **Красный** - Ошибка
- 🔴 **Темно-красный** - Критическая ошибка

## Статистика

```c
uint32_t total, critical, errors, warnings;
error_handler_get_stats(&total, &critical, &errors, &warnings);

ESP_LOGI(TAG, "Всего ошибок: %lu", total);
ESP_LOGI(TAG, "Критических: %lu", critical);
ESP_LOGI(TAG, "Ошибок: %lu", errors);
ESP_LOGI(TAG, "Предупреждений: %lu", warnings);

// Очистка статистики
error_handler_clear_stats();
```

## Пример интеграции в компонент

```c
#include "error_handler.h"

static const char *TAG = "MY_COMPONENT";

esp_err_t my_component_init(void) {
    esp_err_t err;
    
    // Инициализация шины I2C
    err = i2c_bus_init();
    if (err != ESP_OK) {
        ERROR_CRITICAL(ERROR_CATEGORY_I2C, err, TAG, 
                      "Не удалось инициализировать I2C");
        return err;
    }
    
    // Чтение датчика
    float value;
    err = sensor_read(&value);
    if (err == ESP_ERR_TIMEOUT) {
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG, 
                  "Таймаут чтения датчика (попытка %d)", retry_count);
    } else if (err != ESP_OK) {
        ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG, 
                    "Ошибка чтения датчика");
    }
    
    return ESP_OK;
}
```

## Callback для обработки ошибок

```c
void my_error_callback(const error_info_t *error) {
    // Дополнительная обработка ошибок
    if (error->level == ERROR_LEVEL_CRITICAL) {
        // Например, перезагрузка компонента
        component_restart();
    }
}

// Регистрация callback
error_handler_register_callback(my_error_callback);
```

## Включение/выключение всплывающих окон

```c
// Отключить всплывающие окна
error_handler_set_popup(false);

// Включить обратно
error_handler_set_popup(true);
```

## Интеграция

Система автоматически интегрируется с:
- **notification_system** - все ошибки уровня WARNING и выше создают уведомления
- **LVGL** - ошибки уровня ERROR и выше показываются всплывающими окнами
- **ESP_LOG** - все ошибки логируются в консоль

## Зависимости

- `notification_system`
- `lvgl`
- `freertos`

## Пример вывода

```
E (12345) SHT3X: [SENSOR] Не удалось прочитать температуру (code: 263)
```

На экране появится красное всплывающее окно:
```
ОШИБКА

SHT3X: Не удалось прочитать температуру

Код: 263 (ESP_ERR_TIMEOUT)
```

## Коды ошибок - Краткая справка

### Наиболее частые ошибки:

| Код | Название | Типичная причина | Быстрое решение |
|-----|----------|------------------|-----------------|
| **0** | ESP_OK | Успех | — |
| **-1** | ESP_FAIL | Общая ошибка | Проверьте параметры |
| **257** | ESP_ERR_NO_MEM | Нет памяти | Освободите heap или увеличьте размер |
| **258** | ESP_ERR_INVALID_ARG | Неверный параметр | Проверьте аргументы функции |
| **259** | ESP_ERR_INVALID_STATE | Неверное состояние | Проверьте порядок вызовов |
| **263** | ESP_ERR_TIMEOUT | Таймаут | Проверьте устройство/сеть |
| **265** | ESP_ERR_INVALID_CRC | Ошибка CRC | Проверьте связь, помехи |

### Ошибки I2C:

| Код | Причина | Решение |
|-----|---------|---------|
| 263 | Устройство не отвечает | Проверьте адрес, питание, подключение |
| 265 | Повреждение данных | Используйте pull-up резисторы, короткие провода |

### Ошибки NVS:

| Код | Название | Решение |
|-----|----------|---------|
| 4354 | Не инициализирована | Вызовите nvs_flash_init() |
| 4355 | Ключ не найден | Создайте значение по умолчанию |
| 4358 | Нет места | Очистите данные или увеличьте раздел |

📖 **Полная справка**: См. [ERROR_CODES_RU.md](ERROR_CODES_RU.md) для подробного описания всех кодов ошибок

## Примеры обработки типичных ошибок

### I2C устройство не отвечает:
```c
esp_err_t err = i2c_master_transmit(handle, data, len, 1000);
if (err == ESP_ERR_TIMEOUT) {
    ERROR_CHECK_I2C(err, "SHT3X", 
                   "Датчик не отвечает. Проверьте: "
                   "1) Питание (2.4-5.5V) "
                   "2) I2C адрес (0x44/0x45) "
                   "3) Подключение SDA/SCL");
    // Попытка переинициализации
    sensor_reset();
}
```

### Нехватка памяти:
```c
char *buffer = malloc(size);
if (buffer == NULL) {
    ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, TAG,
                  "Не удалось выделить %d байт. "
                  "Свободно heap: %d байт", 
                  size, esp_get_free_heap_size());
    // Критическая ситуация - освобождаем кэши
    clear_caches();
    return ESP_ERR_NO_MEM;
}
```

### Повторные попытки при таймауте:
```c
#define MAX_RETRIES 3
int retry = 0;
esp_err_t err;

do {
    err = sensor_read(&value);
    if (err == ESP_OK) break;
    
    if (err == ESP_ERR_TIMEOUT) {
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                  "Попытка %d/%d: Таймаут чтения датчика",
                  retry + 1, MAX_RETRIES);
        vTaskDelay(pdMS_TO_TICKS(100)); // Задержка перед повтором
    } else {
        // Другая ошибка - прерываем попытки
        ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG,
                    "Критическая ошибка датчика");
        break;
    }
} while (++retry < MAX_RETRIES);

if (err != ESP_OK) {
    // Используем последнее известное значение
    value = last_valid_value;
}
```

## Дополнительные ресурсы

- 📖 **[Справочник кодов ошибок (ERROR_CODES_RU.md)](ERROR_CODES_RU.md)** - Полная таблица всех кодов с описаниями
- [ESP-IDF Error Codes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/error-codes.html)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/)
- [I2C Specification](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)

