# Практические примеры обработки ошибок

Готовые примеры кода для типичных ситуаций в системе гидропонного мониторинга.

## 📑 Содержание

1. [I2C коммуникация](#i2c-коммуникация)
2. [Датчики](#датчики)
3. [Память и ресурсы](#память-и-ресурсы)
4. [Файловые операции (NVS)](#nvs-хранилище)
5. [Система и задачи](#система-и-задачи)
6. [Контроллеры pH/EC](#контроллеры-phec)
7. [Дисплей и UI](#дисплей-и-ui)

---

## I2C коммуникация

### Пример 1: Базовое чтение с обработкой ошибок

```c
#include "error_handler.h"
#include "i2c_bus.h"

static const char *TAG = "MY_SENSOR";
#define SENSOR_ADDR 0x44

esp_err_t read_sensor_value(uint8_t reg, uint8_t *value) {
    esp_err_t err;
    
    // Попытка чтения
    err = i2c_bus_read_reg(SENSOR_ADDR, reg, value, 1);
    
    if (err == ESP_ERR_TIMEOUT) {
        ERROR_CHECK_I2C(err, TAG,
                       "Таймаут чтения регистра 0x%02X с адреса 0x%02X. "
                       "Проверьте: питание, SDA/SCL подключение",
                       reg, SENSOR_ADDR);
        return err;
    }
    
    if (err == ESP_ERR_INVALID_STATE) {
        ERROR_REPORT(ERROR_CATEGORY_I2C, err, TAG,
                    "I2C шина не инициализирована");
        return err;
    }
    
    if (err != ESP_OK) {
        ERROR_CHECK_I2C(err, TAG,
                       "Неизвестная ошибка I2C при чтении 0x%02X", reg);
        return err;
    }
    
    return ESP_OK;
}
```

### Пример 2: Поиск устройства на шине

```c
bool find_i2c_device(uint8_t addr) {
    uint8_t test_data = 0;
    esp_err_t err = i2c_bus_read(addr, &test_data, 1);
    
    if (err == ESP_OK) {
        ERROR_INFO(TAG, "Устройство найдено на адресе 0x%02X", addr);
        return true;
    } else {
        ERROR_WARN(ERROR_CATEGORY_I2C, TAG,
                  "Устройство не найдено на 0x%02X: %s",
                  addr, esp_err_to_name(err));
        return false;
    }
}

void scan_i2c_bus(void) {
    ERROR_INFO(TAG, "Начинаем сканирование I2C шины...");
    
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (find_i2c_device(addr)) {
            found++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (found == 0) {
        ERROR_WARN(ERROR_CATEGORY_I2C, TAG,
                  "Устройства не найдены. Проверьте подключение");
    } else {
        ERROR_INFO(TAG, "Найдено устройств: %d", found);
    }
}
```

### Пример 3: Обработка NACK (устройство занято)

```c
esp_err_t write_with_retry(uint8_t addr, const uint8_t *data, size_t len) {
    const int MAX_RETRIES = 5;
    const int RETRY_DELAY_MS = 50;
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        esp_err_t err = i2c_bus_write(addr, data, len);
        
        if (err == ESP_OK) {
            if (retry > 0) {
                ERROR_INFO(TAG, "Запись успешна с попытки %d", retry + 1);
            }
            return ESP_OK;
        }
        
        if (retry < MAX_RETRIES - 1) {
            ERROR_WARN(ERROR_CATEGORY_I2C, TAG,
                      "Попытка %d/%d: устройство 0x%02X занято, повтор...",
                      retry + 1, MAX_RETRIES, addr);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }
    
    ERROR_REPORT(ERROR_CATEGORY_I2C, ESP_ERR_TIMEOUT, TAG,
                "Не удалось записать на 0x%02X после %d попыток",
                addr, MAX_RETRIES);
    return ESP_ERR_TIMEOUT;
}
```

---

## Датчики

### Пример 4: SHT3X с полной обработкой

```c
#include "sht3x.h"
#include "error_handler.h"

static const char *TAG = "SHT3X_WRAPPER";

typedef struct {
    float temperature;
    float humidity;
    bool valid;
    uint32_t last_read_time;
    uint32_t error_count;
} sensor_cache_t;

static sensor_cache_t g_cache = {
    .temperature = 25.0,
    .humidity = 50.0,
    .valid = false,
    .last_read_time = 0,
    .error_count = 0
};

esp_err_t read_temperature_humidity(float *temp, float *hum) {
    esp_err_t err;
    float new_temp, new_hum;
    
    // Попытка чтения
    err = sht3x_read_temperature(&new_temp);
    
    if (err == ESP_ERR_TIMEOUT) {
        g_cache.error_count++;
        ERROR_CHECK_SENSOR(err, TAG,
                          "SHT3X не отвечает (ошибка #%lu). "
                          "Проверьте: 1) Питание 2.4-5.5V, "
                          "2) I2C адрес 0x44, 3) Подключение",
                          g_cache.error_count);
        
        // Используем кэшированное значение
        if (g_cache.valid) {
            *temp = g_cache.temperature;
            *hum = g_cache.humidity;
            ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                      "Используется кэшированное значение: %.1f°C, %.1f%%",
                      *temp, *hum);
            return ESP_ERR_TIMEOUT; // Возвращаем ошибку, но даём данные
        }
        return err;
    }
    
    if (err == ESP_ERR_INVALID_CRC) {
        ERROR_CHECK_SENSOR(err, TAG,
                          "CRC ошибка SHT3X. Помехи на линии. "
                          "Решение: добавьте pull-up резисторы 4.7kΩ");
        return err;
    }
    
    if (err != ESP_OK) {
        ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG,
                    "Неизвестная ошибка SHT3X");
        return err;
    }
    
    // Проверка валидности данных
    if (new_temp < -40 || new_temp > 85) {
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                  "Температура вне диапазона: %.1f°C (ожидается -40..85°C)",
                  new_temp);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Чтение влажности
    err = sht3x_read_humidity(&new_hum);
    ERROR_CHECK_SENSOR(err, TAG, "Чтение влажности SHT3X");
    
    if (err == ESP_OK) {
        // Обновляем кэш
        g_cache.temperature = new_temp;
        g_cache.humidity = new_hum;
        g_cache.valid = true;
        g_cache.last_read_time = esp_timer_get_time() / 1000;
        g_cache.error_count = 0; // Сброс счётчика ошибок
        
        *temp = new_temp;
        *hum = new_hum;
    }
    
    return err;
}
```

### Пример 5: CCS811 с проверкой готовности

```c
#include "ccs811.h"
#include "error_handler.h"

static const char *TAG = "CCS811_WRAPPER";
static bool g_warmed_up = false;
static uint32_t g_init_time = 0;

esp_err_t read_air_quality(uint16_t *co2, uint16_t *tvoc) {
    // Проверка времени прогрева (20 минут)
    uint32_t uptime_sec = (esp_timer_get_time() / 1000000) - g_init_time;
    if (!g_warmed_up && uptime_sec < 1200) {
        uint32_t remaining = 1200 - uptime_sec;
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                  "CCS811 требует прогрева. "
                  "Осталось: %lu мин %lu сек",
                  remaining / 60, remaining % 60);
        return ESP_ERR_INVALID_STATE;
    }
    g_warmed_up = true;
    
    // Проверка готовности данных
    if (!ccs811_is_data_ready()) {
        // Это нормально, просто ждём
        return ESP_ERR_NOT_FINISHED;
    }
    
    // Чтение данных
    esp_err_t err = ccs811_read_data(co2, tvoc);
    
    if (err != ESP_OK) {
        ERROR_CHECK_SENSOR(err, TAG,
                          "Ошибка чтения CCS811. "
                          "Проверьте: WAK пин (LOW), адрес 0x5A/0x5B");
        return err;
    }
    
    // Валидация диапазона
    if (*co2 > 8192 || *tvoc > 1187) {
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                  "Значения вне диапазона: CO2=%u ppm, TVOC=%u ppb",
                  *co2, *tvoc);
    }
    
    return ESP_OK;
}

void ccs811_init_wrapper(void) {
    g_init_time = esp_timer_get_time() / 1000000;
    g_warmed_up = false;
    
    esp_err_t err = ccs811_init();
    if (err != ESP_OK) {
        ERROR_CRITICAL(ERROR_CATEGORY_SENSOR, err, TAG,
                      "Не удалось инициализировать CCS811");
    } else {
        ERROR_INFO(TAG, "CCS811 инициализирован. Прогрев 20 минут...");
    }
}
```

---

## Память и ресурсы

### Пример 6: Безопасное выделение памяти

```c
#include "error_handler.h"

void* safe_malloc(size_t size, const char *purpose) {
    void *ptr = malloc(size);
    
    if (ptr == NULL) {
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_heap = esp_get_minimum_free_heap_size();
        
        ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, TAG,
                      "Не удалось выделить %d байт для %s. "
                      "Heap: свободно=%lu, минимум=%lu",
                      size, purpose, free_heap, min_heap);
    } else {
        ERROR_DEBUG(TAG, "Выделено %d байт для %s", size, purpose);
    }
    
    return ptr;
}

void* safe_calloc(size_t count, size_t size, const char *purpose) {
    void *ptr = calloc(count, size);
    
    if (ptr == NULL) {
        ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, TAG,
                      "Не удалось выделить %d×%d=%d байт для %s",
                      count, size, count * size, purpose);
    }
    
    return ptr;
}

// Использование
sensor_data_t *data = safe_malloc(sizeof(sensor_data_t), "sensor data");
if (data == NULL) {
    return ESP_ERR_NO_MEM;
}
```

### Пример 7: Мониторинг стека задачи

```c
void monitor_task_stack(const char *task_name) {
    UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL);
    
    if (stack_left < 256) {
        ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_FAIL, task_name,
                      "Критически мало стека: %d байт! Переполнение неизбежно!",
                      stack_left);
    } else if (stack_left < 512) {
        ERROR_WARN(ERROR_CATEGORY_SYSTEM, task_name,
                  "Мало стека: %d байт. Рекомендуется увеличить",
                  stack_left);
    } else {
        ERROR_DEBUG(task_name, "Стек в норме: %d байт свободно", stack_left);
    }
}

void my_sensor_task(void *params) {
    while (1) {
        // Периодически проверяем стек
        monitor_task_stack("sensor_task");
        
        // Основная работа
        read_sensors();
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## NVS хранилище

### Пример 8: Сохранение конфигурации с обработкой

```c
#include "nvs_flash.h"
#include "nvs.h"
#include "error_handler.h"

static const char *TAG = "CONFIG";

esp_err_t save_ph_calibration(float ph4, float ph7, float ph10) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Открытие NVS
    err = nvs_open("calibration", NVS_READWRITE, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ERROR_REPORT(ERROR_CATEGORY_STORAGE, err, TAG,
                    "NVS не инициализирована. Вызовите nvs_flash_init()");
        return err;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ERROR_INFO(TAG, "Создаём новый namespace 'calibration'");
    }
    if (err != ESP_OK) {
        ERROR_REPORT(ERROR_CATEGORY_STORAGE, err, TAG,
                    "Не удалось открыть NVS");
        return err;
    }
    
    // Сохранение значений
    err = nvs_set_blob(nvs_handle, "ph_cal", 
                       &(float[]){ph4, ph7, ph10}, 
                       3 * sizeof(float));
    if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        ERROR_CRITICAL(ERROR_CATEGORY_STORAGE, err, TAG,
                      "NVS переполнена! Требуется очистка");
        nvs_close(nvs_handle);
        return err;
    }
    ERROR_CHECK_SENSOR(err, TAG, "Сохранение калибровки pH");
    
    // Подтверждение записи
    err = nvs_commit(nvs_handle);
    ERROR_CHECK_SENSOR(err, TAG, "Commit NVS");
    
    nvs_close(nvs_handle);
    
    ERROR_INFO(TAG, "Калибровка pH сохранена: 4.0=%.3f, 7.0=%.3f, 10.0=%.3f",
               ph4, ph7, ph10);
    return ESP_OK;
}

esp_err_t load_ph_calibration(float *ph4, float *ph7, float *ph10) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open("calibration", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ERROR_WARN(ERROR_CATEGORY_STORAGE, TAG,
                  "Калибровка не найдена, используются значения по умолчанию");
        *ph4 = 1.0; *ph7 = 2.5; *ph10 = 3.0;
        return ESP_ERR_NVS_NOT_FOUND;
    }
    
    float cal_data[3];
    size_t size = sizeof(cal_data);
    err = nvs_get_blob(nvs_handle, "ph_cal", cal_data, &size);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ERROR_INFO(TAG, "Калибровка отсутствует, используем заводские значения");
        *ph4 = 1.0; *ph7 = 2.5; *ph10 = 3.0;
    } else if (err == ESP_OK) {
        *ph4 = cal_data[0];
        *ph7 = cal_data[1];
        *ph10 = cal_data[2];
        ERROR_INFO(TAG, "Калибровка загружена успешно");
    } else {
        ERROR_REPORT(ERROR_CATEGORY_STORAGE, err, TAG,
                    "Ошибка чтения калибровки");
    }
    
    nvs_close(nvs_handle);
    return err;
}
```

---

## Система и задачи

### Пример 9: Создание задачи с проверками

```c
TaskHandle_t create_monitored_task(TaskFunction_t task_func,
                                   const char *name,
                                   uint32_t stack_size,
                                   UBaseType_t priority) {
    TaskHandle_t handle = NULL;
    
    // Проверка доступной памяти
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < stack_size + 10240) { // +10KB запас
        ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, "TASK_MGR",
                      "Недостаточно heap для задачи '%s' "
                      "(требуется ~%lu, доступно %lu)",
                      name, stack_size, free_heap);
        return NULL;
    }
    
    // Создание задачи
    BaseType_t ret = xTaskCreate(task_func, name, stack_size, 
                                 NULL, priority, &handle);
    
    if (ret != pdPASS) {
        ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_FAIL, "TASK_MGR",
                      "Не удалось создать задачу '%s' "
                      "(стек=%lu, приоритет=%d)",
                      name, stack_size, priority);
        return NULL;
    }
    
    ERROR_INFO("TASK_MGR", 
              "Задача '%s' создана (стек=%lu, приоритет=%d)",
              name, stack_size, priority);
    
    return handle;
}
```

### Пример 10: Watchdog-safe длительные операции

```c
#include "esp_task_wdt.h"

void long_operation_safe(void) {
    const int TOTAL_ITERATIONS = 10000;
    const int WDT_RESET_INTERVAL = 100;
    
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        // Выполнение работы
        process_data(i);
        
        // Периодический сброс watchdog
        if (i % WDT_RESET_INTERVAL == 0) {
            esp_task_wdt_reset(); // Сброс watchdog
            vTaskDelay(1);        // Отдаём управление
            
            if (i % 1000 == 0) {
                ERROR_DEBUG(TAG, "Обработано %d/%d итераций",
                           i, TOTAL_ITERATIONS);
            }
        }
    }
    
    ERROR_INFO(TAG, "Длительная операция завершена успешно");
}
```

---

## Контроллеры pH/EC

### Пример 11: Автоматическая коррекция pH

```c
#include "ph_ec_controller.h"
#include "peristaltic_pump.h"
#include "error_handler.h"

static const char *TAG = "PH_CTRL";

esp_err_t auto_adjust_ph(float current_ph, float target_ph) {
    const float TOLERANCE = 0.2;
    const float MAX_CORRECTION = 1.0;
    
    // Проверка валидности измерения
    if (current_ph < 0 || current_ph > 14) {
        ERROR_REPORT(ERROR_CATEGORY_CONTROLLER, ESP_ERR_INVALID_RESPONSE, TAG,
                    "Недопустимое значение pH: %.2f", current_ph);
        return ESP_ERR_INVALID_ARG;
    }
    
    float diff = target_ph - current_ph;
    
    // pH в норме
    if (fabs(diff) < TOLERANCE) {
        ERROR_DEBUG(TAG, "pH в норме: %.2f (цель: %.2f)", 
                   current_ph, target_ph);
        return ESP_OK;
    }
    
    // Слишком большая разница - подозрительно
    if (fabs(diff) > MAX_CORRECTION) {
        ERROR_CRITICAL(ERROR_CATEGORY_CONTROLLER, ESP_FAIL, TAG,
                      "Критическое отклонение pH! "
                      "Текущее: %.2f, целевое: %.2f, разница: %.2f. "
                      "Требуется вмешательство оператора!",
                      current_ph, target_ph, diff);
        // Отключаем автоматику
        ph_ec_controller_disable();
        return ESP_FAIL;
    }
    
    // Рассчитываем дозу
    uint32_t dose_ml = (uint32_t)(fabs(diff) * 10); // Пример формулы
    
    ERROR_INFO(TAG, "Коррекция pH: %.2f → %.2f (доза: %lu мл)",
               current_ph, target_ph, dose_ml);
    
    // Запуск насоса
    pump_id_t pump = (diff > 0) ? PUMP_PH_UP : PUMP_PH_DOWN;
    esp_err_t err = peristaltic_pump_dispense(pump, dose_ml);
    
    if (err != ESP_OK) {
        ERROR_REPORT(ERROR_CATEGORY_PUMP, err, TAG,
                    "Не удалось запустить насос %d для коррекции pH",
                    pump);
        return err;
    }
    
    return ESP_OK;
}
```

### Пример 12: Защита от переполнения резервуара

```c
esp_err_t safe_water_dosing(pump_id_t pump, uint32_t ml) {
    const uint32_t MAX_DAILY_DOSE = 1000; // мл
    static uint32_t daily_total[4] = {0};
    
    // Проверка дневного лимита
    if (daily_total[pump] + ml > MAX_DAILY_DOSE) {
        ERROR_CRITICAL(ERROR_CATEGORY_PUMP, ESP_FAIL, TAG,
                      "Превышен дневной лимит дозирования! "
                      "Насос %d: использовано %lu мл, попытка +%lu мл, лимит %lu мл. "
                      "ВОЗМОЖНА УТЕЧКА!",
                      pump, daily_total[pump], ml, MAX_DAILY_DOSE);
        return ESP_FAIL;
    }
    
    // Выполнение дозирования
    esp_err_t err = peristaltic_pump_dispense(pump, ml);
    
    if (err == ESP_OK) {
        daily_total[pump] += ml;
        ERROR_INFO(TAG, "Дозировано %lu мл (всего за день: %lu/%lu)",
                   ml, daily_total[pump], MAX_DAILY_DOSE);
    } else {
        ERROR_REPORT(ERROR_CATEGORY_PUMP, err, TAG,
                    "Ошибка дозирования %lu мл через насос %d",
                    ml, pump);
    }
    
    return err;
}
```

---

## Дисплей и UI

### Пример 13: Обработка ошибок LVGL

```c
#include "lvgl.h"
#include "error_handler.h"

lv_obj_t* create_ui_element_safe(lv_obj_t *parent, const char *element_name) {
    if (parent == NULL) {
        ERROR_REPORT(ERROR_CATEGORY_DISPLAY, ESP_ERR_INVALID_ARG, "UI",
                    "Попытка создать '%s' с NULL родителем", element_name);
        return NULL;
    }
    
    lv_obj_t *obj = lv_obj_create(parent);
    
    if (obj == NULL) {
        ERROR_CRITICAL(ERROR_CATEGORY_DISPLAY, ESP_ERR_NO_MEM, "UI",
                      "Не удалось создать UI элемент '%s'. "
                      "Heap: %lu байт",
                      element_name, esp_get_free_heap_size());
        return NULL;
    }
    
    ERROR_DEBUG("UI", "Создан элемент '%s'", element_name);
    return obj;
}
```

---

## Комплексные сценарии

### Пример 14: Полный цикл чтения датчика с восстановлением

```c
typedef struct {
    bool sensor_ok;
    uint32_t consecutive_errors;
    uint32_t total_reads;
    uint32_t successful_reads;
    float last_valid_value;
} sensor_health_t;

static sensor_health_t g_health = {
    .sensor_ok = true,
    .last_valid_value = 25.0
};

esp_err_t robust_sensor_read(float *value) {
    const uint32_t MAX_CONSECUTIVE_ERRORS = 5;
    esp_err_t err;
    
    g_health.total_reads++;
    
    // Попытка чтения
    err = sht3x_read_temperature(value);
    
    if (err == ESP_OK) {
        // Успех - обновляем статистику
        g_health.consecutive_errors = 0;
        g_health.successful_reads++;
        g_health.last_valid_value = *value;
        g_health.sensor_ok = true;
        
        // Статистика раз в 100 чтений
        if (g_health.total_reads % 100 == 0) {
            float success_rate = (float)g_health.successful_reads / 
                                g_health.total_reads * 100;
            ERROR_INFO(TAG, "Статистика SHT3X: успешно %.1f%% (%lu/%lu)",
                      success_rate, g_health.successful_reads, 
                      g_health.total_reads);
        }
        
        return ESP_OK;
    }
    
    // Ошибка - обрабатываем
    g_health.consecutive_errors++;
    
    if (err == ESP_ERR_TIMEOUT) {
        ERROR_CHECK_SENSOR(err, TAG,
                          "SHT3X таймаут (последовательно: %lu)",
                          g_health.consecutive_errors);
    } else if (err == ESP_ERR_INVALID_CRC) {
        ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                  "CRC ошибка #%lu. Проверьте помехи",
                  g_health.consecutive_errors);
    } else {
        ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG,
                    "Ошибка чтения #%lu", g_health.consecutive_errors);
    }
    
    // Критическая ситуация - датчик не работает
    if (g_health.consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
        if (g_health.sensor_ok) {
            ERROR_CRITICAL(ERROR_CATEGORY_SENSOR, ESP_FAIL, TAG,
                          "SHT3X не отвечает %lu раз подряд! "
                          "Датчик признан нерабочим. "
                          "Требуется проверка оборудования!",
                          g_health.consecutive_errors);
            g_health.sensor_ok = false;
        }
        
        // Используем последнее известное значение
        *value = g_health.last_valid_value;
        return ESP_ERR_INVALID_STATE;
    }
    
    // Используем кэш
    *value = g_health.last_valid_value;
    return err;
}

// Функция для сброса после ремонта
void reset_sensor_health(void) {
    g_health.consecutive_errors = 0;
    g_health.sensor_ok = true;
    ERROR_INFO(TAG, "Состояние датчика сброшено");
}
```

### Пример 15: Система аварийной остановки

```c
typedef enum {
    EMERGENCY_NONE,
    EMERGENCY_PH_CRITICAL,
    EMERGENCY_EC_HIGH,
    EMERGENCY_PUMP_FAILURE,
    EMERGENCY_SENSOR_LOST
} emergency_type_t;

void trigger_emergency_stop(emergency_type_t type, const char *reason) {
    ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_FAIL, "EMERGENCY",
                  "АВАРИЙНАЯ ОСТАНОВКА: %s", reason);
    
    // Останавливаем все насосы
    for (int i = 0; i < 4; i++) {
        esp_err_t err = peristaltic_pump_stop(i);
        if (err != ESP_OK) {
            ERROR_CRITICAL(ERROR_CATEGORY_PUMP, err, "EMERGENCY",
                          "Не удалось остановить насос %d!", i);
        }
    }
    
    // Отключаем реле
    for (int i = 0; i < 8; i++) {
        trema_relay_set(i, false);
    }
    
    // Отключаем автоматические контроллеры
    ph_ec_controller_disable();
    
    // Показываем на экране
    ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_FAIL, "EMERGENCY",
                  "Система остановлена. Код: %d. Причина: %s",
                  type, reason);
    
    // Уведомление через все каналы
    notification_create(NOTIF_TYPE_CRITICAL, NOTIF_PRIORITY_URGENT,
                       NOTIF_SOURCE_SYSTEM,
                       "АВАРИЙНАЯ ОСТАНОВКА! Проверьте систему!");
}

// Использование
if (ph_value < 3.0 || ph_value > 9.0) {
    char reason[128];
    snprintf(reason, sizeof(reason),
             "Критическое значение pH: %.2f", ph_value);
    trigger_emergency_stop(EMERGENCY_PH_CRITICAL, reason);
}
```

---

## 🎓 Лучшие практики

### 1. Информативные сообщения

```c
// ✅ Хорошо: Детальное описание
ERROR_REPORT(ERROR_CATEGORY_I2C, err, "SHT3X",
            "Таймаут чтения регистра 0x2C с адреса 0x44 "
            "(попытка %d из %d, задержка %d мс)",
            retry, max_retries, delay_ms);

// ❌ Плохо: Неинформативно
ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, "S", "error");
```

### 2. Контекстная информация

```c
// ✅ Хорошо: Включаем текущее состояние
ERROR_REPORT(ERROR_CATEGORY_PUMP, err, "PUMP",
            "Насос %d не запускается. "
            "Давление: %.1f bar, напряжение: %.1f V, "
            "рабочих часов: %lu",
            pump_id, pressure, voltage, operating_hours);

// ❌ Плохо: Без контекста
ERROR_REPORT(ERROR_CATEGORY_PUMP, err, "PUMP", "Failed");
```

### 3. Прогрессивная эскалация

```c
if (retry_count == 1) {
    ERROR_DEBUG(TAG, "Первая попытка не удалась, повтор...");
} else if (retry_count < MAX_RETRIES / 2) {
    ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
              "Попытка %d/%d не удалась", retry_count, MAX_RETRIES);
} else if (retry_count < MAX_RETRIES) {
    ERROR_REPORT(ERROR_CATEGORY_SENSOR, err, TAG,
                "Множественные ошибки (%d/%d)",
                retry_count, MAX_RETRIES);
} else {
    ERROR_CRITICAL(ERROR_CATEGORY_SENSOR, err, TAG,
                  "Все %d попыток исчерпаны. Датчик недоступен!",
                  MAX_RETRIES);
}
```

---

## 🔍 Отладочные помощники

### Вывод состояния системы

```c
void print_system_diagnostics(void) {
    uint32_t total, critical, errors, warnings;
    error_handler_get_stats(&total, &critical, &errors, &warnings);
    
    ERROR_INFO("DIAGNOSTICS", 
              "=== Диагностика системы ===");
    ERROR_INFO("DIAGNOSTICS",
              "Heap: свободно=%lu, минимум=%lu",
              esp_get_free_heap_size(),
              esp_get_minimum_free_heap_size());
    ERROR_INFO("DIAGNOSTICS",
              "Ошибки: всего=%lu, крит=%lu, ошибок=%lu, предупр=%lu",
              total, critical, errors, warnings);
    ERROR_INFO("DIAGNOSTICS",
              "Uptime: %llu секунд",
              esp_timer_get_time() / 1000000);
    ERROR_INFO("DIAGNOSTICS",
              "=========================");
}
```

### Проверка здоровья компонентов

```c
typedef struct {
    const char *name;
    bool (*health_check)(void);
} component_t;

bool check_all_components(void) {
    component_t components[] = {
        {"I2C", i2c_bus_health_check},
        {"SHT3X", sht3x_health_check},
        {"CCS811", ccs811_health_check},
        {"Display", lcd_health_check},
        {NULL, NULL}
    };
    
    bool all_ok = true;
    
    for (int i = 0; components[i].name != NULL; i++) {
        if (!components[i].health_check()) {
            ERROR_WARN(ERROR_CATEGORY_SYSTEM, "HEALTH",
                      "Компонент '%s' не прошёл проверку здоровья",
                      components[i].name);
            all_ok = false;
        }
    }
    
    if (all_ok) {
        ERROR_INFO("HEALTH", "Все компоненты в порядке ✓");
    } else {
        ERROR_WARN(ERROR_CATEGORY_SYSTEM, "HEALTH",
                  "Обнаружены проблемы. Запустите диагностику");
    }
    
    return all_ok;
}
```

---

## 🔗 Навигация

- 📘 [README.md](README.md) - Основная документация
- 📖 [ERROR_CODES_RU.md](ERROR_CODES_RU.md) - Справочник кодов
- 📄 [ERROR_CHEATSHEET_RU.md](ERROR_CHEATSHEET_RU.md) - Шпаргалка
- 📋 [DOCS_INDEX.md](DOCS_INDEX.md) - Индекс документации

**Следующий шаг**: Адаптируйте эти примеры под свои компоненты!

