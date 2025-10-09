# Шпаргалка по обработке ошибок

Краткое руководство для быстрого использования системы обработки ошибок.

## 🚀 Быстрый старт

```c
#include "error_handler.h"

// Простой отчёт об ошибке
ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, "SHT3X", 
            "Не удалось прочитать температуру");

// Критическая ошибка
ERROR_CRITICAL(ERROR_CATEGORY_I2C, err, "i2c_bus",
              "Не удалось инициализировать шину");

// Предупреждение
ERROR_WARN(ERROR_CATEGORY_SENSOR, "CCS811",
          "Датчик требует прогрева");

// Автопроверка
ERROR_CHECK_I2C(err, "SHT3X", "Ошибка записи на 0x%02X", addr);
```

## 📊 Таблица категорий

| Категория | Использовать для |
|-----------|------------------|
| `ERROR_CATEGORY_I2C` | Ошибки шины I2C |
| `ERROR_CATEGORY_SENSOR` | Ошибки датчиков |
| `ERROR_CATEGORY_DISPLAY` | Ошибки дисплея |
| `ERROR_CATEGORY_STORAGE` | Ошибки NVS |
| `ERROR_CATEGORY_SYSTEM` | Системные ошибки |
| `ERROR_CATEGORY_PUMP` | Ошибки насосов |
| `ERROR_CATEGORY_RELAY` | Ошибки реле |

## 🎯 Частые коды ошибок

| Код | Что это | Что делать |
|-----|---------|------------|
| 0 | ✅ Успех | Всё ОК |
| -1 | ❌ Общая ошибка | Проверь параметры |
| 257 | 💾 Нет памяти | `esp_get_free_heap_size()` |
| 258 | ⚠️ Неверный аргумент | Проверь NULL, диапазоны |
| 263 | ⏱️ Таймаут | Устройство не отвечает |
| 265 | 🔧 Ошибка CRC | Помехи на линии |

## 🔍 Паттерны использования

### 1. Проверка с обработкой

```c
esp_err_t err = sensor_init();
if (err != ESP_OK) {
    ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG,
                "Инициализация не удалась");
    return err;
}
```

### 2. Попытки с повторами

```c
for (int i = 0; i < 3; i++) {
    err = sensor_read(&value);
    if (err == ESP_OK) break;
    
    ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
              "Попытка %d/3 не удалась", i + 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

### 3. Множественные проверки

```c
// Все I2C операции
ERROR_CHECK_I2C(i2c_master_write(...), TAG, "Запись на 0x%02X", addr);
ERROR_CHECK_I2C(i2c_master_read(...), TAG, "Чтение с 0x%02X", addr);

// Все сенсоры
ERROR_CHECK_SENSOR(sht3x_read(...), "SHT3X", "Чтение температуры");
ERROR_CHECK_SENSOR(ccs811_read(...), "CCS811", "Чтение CO2");
```

### 4. Критические ошибки

```c
void *buffer = malloc(size);
if (!buffer) {
    ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, TAG,
                  "Критическая нехватка памяти (%d байт)", size);
    esp_restart(); // Перезагрузка
}
```

## 🎨 Уровни и цвета

| Уровень | Цвет | Когда использовать |
|---------|------|--------------------|
| DEBUG | 🔵 Синий | Отладочная информация |
| INFO | 🟢 Зелёный | Информационные сообщения |
| WARNING | 🟠 Оранжевый | Предупреждения, не критично |
| ERROR | 🔴 Красный | Ошибки, требует внимания |
| CRITICAL | 🔴 Тёмно-красный | Критические ошибки, система в опасности |

## 💡 Советы по сообщениям

### ✅ Хорошие сообщения

```c
ERROR_REPORT(ERROR_CATEGORY_I2C, err, "SHT3X",
            "Таймаут чтения температуры с адреса 0x44 (попытка %d)", retry);

ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, "MAIN",
              "Не удалось выделить %d байт. Свободно: %d",
              needed, esp_get_free_heap_size());
```

### ❌ Плохие сообщения

```c
ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, "S", "err");  // Неинформативно
ERROR_REPORT(ERROR_CATEGORY_I2C, err, "", "Error");    // Нет компонента
```

## 🔧 Отладка

### Проверка heap

```c
uint32_t free_heap = esp_get_free_heap_size();
uint32_t min_heap = esp_get_minimum_free_heap_size();

ERROR_INFO("SYSTEM", "Heap: %d байт, мин: %d байт", free_heap, min_heap);
```

### Проверка стека задачи

```c
UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL);
if (stack_left < 512) {
    ERROR_WARN(ERROR_CATEGORY_SYSTEM, TAG,
              "Мало места в стеке: %d байт", stack_left);
}
```

### Время выполнения

```c
uint32_t start = esp_timer_get_time();
// ... операция ...
uint32_t elapsed = (esp_timer_get_time() - start) / 1000; // мс

if (elapsed > 1000) {
    ERROR_WARN(ERROR_CATEGORY_SYSTEM, TAG,
              "Операция заняла %d мс (ожидалось < 1000)", elapsed);
}
```

## 📱 Интеграция с датчиками

### SHT3X (Температура/Влажность)

```c
esp_err_t err = sht3x_read_temperature(&temp);
if (err == ESP_ERR_TIMEOUT) {
    ERROR_CHECK_SENSOR(err, "SHT3X",
                      "Проверьте: питание 2.4-5.5V, адрес 0x44/0x45, SDA/SCL");
} else if (err == ESP_ERR_INVALID_CRC) {
    ERROR_CHECK_SENSOR(err, "SHT3X",
                      "Помехи на линии. Добавьте pull-up резисторы");
}
```

### CCS811 (CO2)

```c
if (ccs811_is_data_ready()) {
    err = ccs811_read_co2(&co2);
    ERROR_CHECK_SENSOR(err, "CCS811", "Чтение CO2");
} else {
    ERROR_WARN(ERROR_CATEGORY_SENSOR, "CCS811",
              "Датчик не готов. Требуется прогрев 20 минут");
}
```

### Trema pH/EC

```c
err = trema_ph_read(&ph_value);
if (err != ESP_OK) {
    if (ph_value < 0 || ph_value > 14) {
        ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, "TREMA_PH",
                    "Значение вне диапазона: %.2f (ожидается 0-14)", ph_value);
    } else {
        ERROR_CHECK_SENSOR(err, "TREMA_PH", "Ошибка чтения pH");
    }
}
```

## 🛠️ Расширенные возможности

### Статистика ошибок

```c
uint32_t total, critical, errors, warnings;
error_handler_get_stats(&total, &critical, &errors, &warnings);

ESP_LOGI(TAG, "Статистика: всего=%lu, крит=%lu, ошибок=%lu, предупр=%lu",
         total, critical, errors, warnings);

// Очистка статистики
error_handler_clear_stats();
```

### Пользовательский callback

```c
void my_error_callback(const error_info_t *error) {
    if (error->category == ERROR_CATEGORY_SENSOR) {
        // Дополнительная обработка ошибок датчиков
        notify_user_via_buzzer();
    }
}

error_handler_register_callback(my_error_callback);
```

### Управление всплывающими окнами

```c
// Отключить всплывающие окна (только уведомления и лог)
error_handler_set_popup(false);

// Включить обратно
error_handler_set_popup(true);

// Сменить шрифт
extern const lv_font_t my_custom_font;
error_handler_set_font(&my_custom_font);
```

## 🚨 Критические ситуации

### Guru Meditation Error

Если система упала:
1. Скопируйте backtrace из монитора
2. Используйте: `xtensa-esp32s3-elf-addr2line -pfiaC -e build/hydroponics.elf АДРЕС`
3. Проверьте размеры стеков задач
4. Включите `CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH`

### Watchdog Timeout

```c
// В длительной операции
for (int i = 0; i < BIG_NUMBER; i++) {
    // Работа...
    
    if (i % 1000 == 0) {
        vTaskDelay(1); // Отдаём управление
    }
}
```

### Stack Overflow

Увеличьте размер стека:
```c
xTaskCreate(my_task, "my_task", 
            4096,  // Было 2048, увеличили
            NULL, 5, NULL);
```

## 📋 Чеклист перед релизом

- [ ] Все ERROR_REPORT имеют осмысленные сообщения
- [ ] Критические ошибки обрабатываются корректно  
- [ ] Добавлены повторные попытки где нужно
- [ ] Проверены размеры стеков (>1KB свободно)
- [ ] Проверена heap память (>20KB свободно в работе)
- [ ] Тестированы все сценарии ошибок
- [ ] Отключены DEBUG логи в production
- [ ] Watchdog не срабатывает

## 🔗 Полезные ссылки

- 📖 [ERROR_CODES_RU.md](ERROR_CODES_RU.md) - Полный справочник кодов
- 📘 [README.md](README.md) - Полная документация error_handler
- 🌐 [ESP-IDF Error Codes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/error-codes.html)

---
**Подсказка**: Держите эту шпаргалку открытой при разработке! 🚀

