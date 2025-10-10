# План реализации PID системы для Hydro 1.0 (обновленная версия)

## НОВЫЕ ТРЕБОВАНИЯ

### Интеграция с реальными датчиками
- **PID коррекция должна основываться на реальных значениях датчиков pH и EC**
- Чтение актуальных данных из `sensor_data_t` перед каждой коррекцией
- Проверка валидности данных датчиков (`valid[SENSOR_INDEX_PH]` и `valid[SENSOR_INDEX_EC]`)
- Если датчик неисправен → не выполнять коррекцию, логировать ошибку

### Пороги срабатывания PID
- **Настраиваемые пороги для активации PID коррекции**
- Параметры хранятся в NVS через config_manager
- Настройка через UI экраны
- Независимые пороги для pH UP, pH DOWN, EC UP, EC DOWN

---

## 1. Создание компонента pump_manager

**Файлы:** `components/pump_manager/pump_manager.c/h`, `components/pump_manager/CMakeLists.txt`

Единый компонент, содержащий:

- PID контроллер (структура, compute функция с P+I+D компонентами, anti-windup)
- Менеджер насосов (управление 6 насосами, safety checks, статистика)
- API для работы с PID (инициализация, настройка Kp/Ki/Kd, выполнение коррекции)

**Ключевые структуры:**

```c
typedef struct {
    float kp, ki, kd;           // PID коэффициенты
    float setpoint;              // Целевое значение
    float integral;              // Интегральная составляющая
    float prev_error;            // Предыдущая ошибка для D
    float output_min, output_max; // Лимиты выхода
    uint64_t last_time_us;       // Для расчета dt
    bool enabled;
} pid_controller_t;

typedef struct {
    uint32_t total_runs;         // Общее количество запусков
    float total_volume_ml;       // Общий объем дозированный
    uint64_t total_time_ms;      // Общее время работы
    uint64_t last_run_time;      // Время последнего запуска
} pump_stats_t;
```

**API функции:**

- `pump_manager_init()` - инициализация всех 6 насосов и PID, создание задачи и мьютексов
- `pump_manager_set_pid_tunings(pump_index, kp, ki, kd)` - настройка PID
- `pump_manager_compute_and_execute(pump_index, current, target)` - расчет и выполнение (с 3 попытками)
- `pump_manager_get_stats(pump_index)` - получение статистики
- `pump_manager_reset_pid(pump_index)` - сброс интегральной составляющей
- `pump_manager_reset_daily_counter(pump_index)` - ручной сброс суточного счетчика
- `pump_manager_get_daily_volume(pump_index)` - получить текущий суточный объем

**Thread-safety:**

- 6 отдельных мьютексов (по одному на каждый насос) для параллельной работы
- Защита critical sections при доступе к PID state и статистике

**FreeRTOS задача:**

- `pump_manager_task()` - приоритет 8, стек 3072
- Мониторинг cooldown таймеров
- Проверка достижения max_daily_volume
- Сохранение суточных счетчиков в NVS каждые 10 минут
- Автосброс суточного счетчика в полночь (00:00) + запись старых значений на SD через data_logger
- Flush PID логов на SD каждые 5 минут

**Обработка ошибок:**

- 3 попытки запуска насоса с интервалом 1 сек
- При неудаче: критическое уведомление + отключение PID для насоса + запись в лог + ESP_FAIL

**ВАЖНО: Интеграция с реальными датчиками:**

- PID коррекция должна использовать **реальные значения** датчиков pH и EC
- Чтение значений из глобальной `sensor_data_t` структуры перед каждой коррекцией
- Проверка валидности данных датчиков перед выполнением PID
- Интеграция с `system_tasks.c` для получения актуальных показаний

## 2. Расширение system_config.h

**Добавить РАСШИРЕННУЮ структуру PID с полным набором параметров:**

```c
typedef struct {
    // Основные PID коэффициенты
    float kp;                    // Пропорциональный коэффициент
    float ki;                    // Интегральный коэффициент
    float kd;                    // Дифференциальный коэффициент
    
    // Лимиты выхода
    float output_min;            // Минимальный выход (мл)
    float output_max;            // Максимальный выход (мл)
    
    // Расширенные параметры
    float deadband;              // Мертвая зона (не корректировать если ошибка < deadband)
    float integral_max;          // Максимальное значение интеграла (anti-windup)
    float sample_time_ms;        // Время выборки для PID (мс)
    
    // Ограничения безопасности
    float max_dose_per_cycle;    // Максимальная доза за один цикл (мл)
    uint32_t cooldown_time_ms;   // Время между коррекциями (мс)
    uint32_t max_daily_volume;   // Максимальный суточный объем (мл)
    
    // Режимы работы
    bool enabled;                // Включен ли PID
    bool auto_reset_integral;    // Автосброс интеграла при смене знака ошибки
    bool use_derivative_filter;  // Использовать фильтр для D-компоненты
    
    // НОВОЕ: Пороги срабатывания PID
    float activation_threshold;   // Порог активации PID (разница от целевого значения)
    float deactivation_threshold; // Порог деактивации (когда считать что достигли цели)
} pid_config_t;

// В system_config_t добавить:
pid_config_t pump_pid[PUMP_INDEX_COUNT];  // PID для 6 насосов
```

**Значения по умолчанию (в config_manager.c):**

- **pH UP/DOWN:** 
  - Kp=2.0, Ki=0.5, Kd=0.1
  - output_min=1.0, output_max=50.0
  - deadband=0.05, integral_max=100.0
  - sample_time=5000, max_dose=10.0
  - cooldown=60000, max_daily=500
  - **activation_threshold=0.3** (начинать коррекцию при отклонении >0.3 pH)
  - **deactivation_threshold=0.05** (считать целью достигнутой при отклонении <0.05 pH)

- **EC A/B/C:** 
  - Kp=1.5, Ki=0.3, Kd=0.05
  - output_min=1.0, output_max=100.0
  - deadband=0.1, integral_max=200.0
  - sample_time=10000, max_dose=20.0
  - cooldown=120000, max_daily=1000
  - **activation_threshold=0.2** (начинать коррекцию при отклонении >0.2 EC)
  - **deactivation_threshold=0.05** (считать целью достигнутой при отклонении <0.05 EC)

- **Water:** 
  - Kp=1.0, Ki=0.2, Kd=0.0
  - output_min=5.0, output_max=200.0
  - deadband=0.05, integral_max=150.0
  - sample_time=10000, max_dose=50.0
  - cooldown=120000, max_daily=2000
  - **activation_threshold=0.2**
  - **deactivation_threshold=0.05**

## 3. Обновление config_manager

**Файл:** `components/config_manager/config_manager.c`

Расширить функции:

- `config_load()` - загрузка PID настроек из NVS (включая пороги срабатывания)
- `config_save()` - сохранение PID настроек в NVS
- `config_manager_get_defaults()` - установка дефолтных PID значений (включая все расширенные параметры и пороги)

## 3.1 Расширение data_logger для статистики и PID логов

**Файл:** `components/data_logger/data_logger.h`

Добавить новые функции:

```c
// Логирование статистики насосов (на SD в pump_stats.csv)
esp_err_t data_logger_log_pump_stats(pump_index_t pump, float volume_ml, uint32_t duration_ms);

// Логирование PID коррекции (буферизация + JSON формат)
esp_err_t data_logger_log_pid_correction(pump_index_t pump, float setpoint, float current, 
                                          float p_term, float i_term, float d_term, 
                                          float output_ml, const char* status);

// Принудительный flush буфера PID логов на SD
esp_err_t data_logger_flush_pid_logs(void);
```

**Реализация в data_logger.c:**

- Буфер PID логов: массив на 10 записей в RAM
- Формат JSON: `{"time":"2025-10-10T12:34:56", "pump":"pH_UP", "setpoint":6.8, "current":7.2, "pid":{"p":0.8,"i":0.2,"d":0.05}, "output":5.2, "status":"OK"}`
- Автоматический flush при заполнении буфера (10 записей)
- Файлы: `/sdcard/pump_stats.csv` и `/sdcard/pid_logs_[YYYY-MM-DD].json`

## 4. Переписать ph_ec_controller для работы с реальными датчиками

**Файл:** `components/ph_ec_controller/ph_ec_controller.c`

**Удалить:**

- Старую пропорциональную логику в `ph_ec_controller_correct_ph()` (строки 202-246)
- Старую логику в `ph_ec_controller_correct_ec()` (строки 249-311)

**Добавить:**

- Включение `pump_manager.h`
- Получение реальных значений датчиков
- Проверку валидности данных датчиков
- Проверку порогов срабатывания (activation_threshold)
- Вызовы `pump_manager_compute_and_execute()` вместо прямого управления насосами

**Новая логика коррекции pH с реальными датчиками:**

```c
esp_err_t ph_ec_controller_correct_ph(float current_ph) {
    if (!g_ph_auto_mode) return ESP_OK;
    
    // НОВОЕ: Получение реальных данных датчика
    sensor_data_t sensor_data;
    if (get_current_sensor_data(&sensor_data) != ESP_OK) {
        ESP_LOGW(TAG, "Не удалось получить данные датчиков");
        return ESP_FAIL;
    }
    
    // НОВОЕ: Проверка валидности данных pH датчика
    if (!sensor_data.valid[SENSOR_INDEX_PH]) {
        ESP_LOGW(TAG, "Данные pH датчика невалидны, пропускаем коррекцию");
        return ESP_FAIL;
    }
    
    // Использование реальных значений
    float real_ph = sensor_data.ph;
    float target_ph = g_ph_params.target_ph;
    float error = fabsf(real_ph - target_ph);
    
    // Выбор насоса
    pump_index_t pump_idx = (real_ph > target_ph) 
        ? PUMP_INDEX_PH_DOWN : PUMP_INDEX_PH_UP;
    
    // НОВОЕ: Проверка порога срабатывания
    system_config_t config;
    config_load(&config);
    if (error < config.pump_pid[pump_idx].activation_threshold) {
        ESP_LOGD(TAG, "pH в пределах порога срабатывания (%.2f < %.2f)", 
                 error, config.pump_pid[pump_idx].activation_threshold);
        return ESP_OK;
    }
    
    // PID расчет и выполнение
    ESP_LOGI(TAG, "Коррекция pH через PID: реальн=%.2f цель=%.2f", real_ph, target_ph);
    return pump_manager_compute_and_execute(pump_idx, real_ph, target_ph);
}
```

**Аналогично для EC:**

```c
esp_err_t ph_ec_controller_correct_ec(float current_ec) {
    // Получение реальных данных
    sensor_data_t sensor_data;
    if (get_current_sensor_data(&sensor_data) != ESP_OK) {
        return ESP_FAIL;
    }
    
    // Проверка валидности EC датчика
    if (!sensor_data.valid[SENSOR_INDEX_EC]) {
        ESP_LOGW(TAG, "Данные EC датчика невалидны");
        return ESP_FAIL;
    }
    
    float real_ec = sensor_data.ec;
    float target_ec = g_ec_params.target_ec;
    float error = fabsf(real_ec - target_ec);
    
    // Проверка порогов срабатывания для каждого насоса EC
    // ... аналогично pH
    
    // PID коррекция с реальными значениями
    return pump_manager_compute_and_execute(...);
}
```

## 5. Создание UI экранов

**ВАЖНО: Все кнопки должны быть интерактивными с обработчиками событий LV_EVENT_CLICKED!**

### 5.1 Структура каталогов

```
components/lvgl_ui/screens/
├── pumps/
│   ├── pumps_status_screen.c/h      - Статус всех 6 насосов
│   ├── pumps_manual_screen.c/h      - Ручное управление (старт/стоп)
│   └── pump_calibration_screen.c/h  - Калибровка насосов
└── pid/
    ├── pid_main_screen.c/h          - Список PID контроллеров
    ├── pid_detail_screen.c/h        - Детали одного PID
    ├── pid_tuning_screen.c/h        - Настройка Kp/Ki/Kd
    ├── pid_advanced_screen.c/h      - Расширенные настройки PID (НОВЫЙ!)
    ├── pid_thresholds_screen.c/h    - Настройка порогов срабатывания (НОВЫЙ!)
    └── pid_graph_screen.c/h         - График real-time
```

### 5.2 Экран pid_thresholds_screen.c (НОВЫЙ - Настройка порогов срабатывания)

**Назначение:** Настройка порогов активации/деактивации PID для каждого насоса

**Содержимое:**

- Выбор насоса (dropdown: pH UP, pH DOWN, EC A, EC B, EC C, Water)
- Текущие значения датчиков (real-time)
- Целевое значение (target)
- **Поле: Порог активации PID** (activation_threshold)
  - Описание: "PID начнет работу при отклонении больше этого значения"
  - Slider + числовое поле
  - Диапазон: 0.01 - 2.0 (для pH), 0.01 - 1.0 (для EC)
  - Текущее значение из config
  
- **Поле: Порог деактивации** (deactivation_threshold)
  - Описание: "Цель считается достигнутой при отклонении меньше этого значения"
  - Slider + числовое поле
  - Диапазон: 0.01 - 0.5
  - Текущее значение из config

- **Визуализация:**
  - График с зонами:
    - Зеленая зона: |error| < deactivation_threshold (цель достигнута)
    - Желтая зона: deactivation_threshold ≤ |error| < activation_threshold (ожидание)
    - Красная зона: |error| ≥ activation_threshold (PID активен)
  - Текущее положение на графике

- **Кнопки с обработчиками:**
  - "Применить" → `on_apply_thresholds()` → сохранение в config через config_save()
  - "По умолчанию" → восстановление дефолтных порогов
  - "Назад"

**Регистрация:** `"pid_thresholds"`

**Обработчик сохранения:**

```c
static void on_apply_thresholds(lv_event_t *e) {
    pump_index_t pump_idx = get_selected_pump();
    float activation = get_activation_threshold_value();
    float deactivation = get_deactivation_threshold_value();
    
    // Валидация
    if (deactivation >= activation) {
        show_warning("Порог деактивации должен быть меньше порога активации!");
        return;
    }
    
    // Сохранение в config
    system_config_t config;
    config_load(&config);
    config.pump_pid[pump_idx].activation_threshold = activation;
    config.pump_pid[pump_idx].deactivation_threshold = deactivation;
    config_save(&config);
    
    // Применить в pump_manager
    pump_manager_apply_config(&config);
    
    show_popup("Пороги срабатывания сохранены!");
}
```

### 5.6 Экран pid_advanced_screen.c (Расширенные настройки)

**Содержимое:**

- Все расширенные параметры PID с редакторами:
  - output_min / output_max (мл)
  - deadband (зона нечувствительности)
  - integral_max (anti-windup лимит)
  - sample_time_ms (время выборки)
  - max_dose_per_cycle (макс. доза за цикл)
  - cooldown_time_ms (время между коррекциями)
  - max_daily_volume (суточный лимит объема)
  - Чекбокс: auto_reset_integral
  - Чекбокс: use_derivative_filter
  - **НОВОЕ: Ссылка на экран "Настройка порогов" → pid_thresholds_screen**

- Кнопка "Сохранить" → запись всех параметров в config_manager
- Кнопка "По умолчанию" → восстановление всех дефолтных значений
- Кнопка "Пороги срабатывания" → переход к pid_thresholds_screen
- Кнопка "Назад"

**Регистрация:** `"pid_advanced"`

### 5.3 Экран pid_detail_screen.c

**Содержимое:**

- Детали одного PID контроллера (получить pump_index из screen_show context)
- **НОВОЕ: Отображение реальных значений датчика в реальном времени**
- Текущие параметры: Kp, Ki, Kd, setpoint
- **НОВОЕ: Пороги срабатывания (activation/deactivation)**
- Текущее состояние: P-term, I-term, D-term, Output
- **НОВОЕ: Статус PID (ACTIVE/IDLE/WAITING)**
- Статистика: количество запусков, общий объем, суточный объем

**Кнопки с обработчиками:**

- "Настроить" → `on_tune_click()` → screen_show("pid_tuning", pump_idx)
- "Расширенные" → `on_advanced_click()` → screen_show("pid_advanced", pump_idx)
- "Пороги" → `on_thresholds_click()` → screen_show("pid_thresholds", pump_idx)
- "Сброс интеграла" → `on_reset_integral_click()` → pump_manager_reset_pid(pump_idx)
- "Тест" → `on_test_click()` → запуск тестового цикла насоса (5 сек)
- "График" → `on_graph_click()` → screen_show("pid_graph", pump_idx)
- "Назад" (back_button)

**Регистрация:** `"pid_detail"`

## 6. Интеграция экранов

**Файл:** `components/lvgl_ui/screen_manager/screen_init.c`

Добавить в функцию инициализации:

```c
screen_register("pumps_status", pumps_status_screen_create, ...);
screen_register("pid_main", pid_main_screen_create, ...);
screen_register("pid_detail", pid_detail_screen_create, ...);
screen_register("pid_tuning", pid_tuning_screen_create, ...);
screen_register("pid_advanced", pid_advanced_screen_create, ...);  // НОВЫЙ
screen_register("pid_thresholds", pid_thresholds_screen_create, ...);  // НОВЫЙ
screen_register("pid_graph", pid_graph_screen_create, ...);
screen_register("pump_calibration", pump_calibration_screen_create, ...);
```

**Файл:** `components/lvgl_ui/screens/system/system_menu_screen.c`

Добавить пункты меню:

- "Статус насосов" → screen_show("pumps_status", NULL)
- "PID Настройки" → screen_show("pid_main", NULL)
- "Калибровка насосов" → screen_show("pump_calibration", NULL)

## 7. Обновление CMakeLists

**Файл:** `components/lvgl_ui/CMakeLists.txt`

Добавить новые экраны в SRCS:

```cmake
"screens/pumps/pumps_status_screen.c"
"screens/pumps/pumps_manual_screen.c"
"screens/pumps/pump_calibration_screen.c"
"screens/pid/pid_main_screen.c"
"screens/pid/pid_detail_screen.c"
"screens/pid/pid_tuning_screen.c"
"screens/pid/pid_advanced_screen.c"      # НОВЫЙ
"screens/pid/pid_thresholds_screen.c"     # НОВЫЙ
"screens/pid/pid_graph_screen.c"
```

## 8. ВАЖНО: Инкрементальная сборка после каждого TODO

**Окружение ESP-IDF:**

```bash
C:\Windows\system32\cmd.exe /k ""C:\Espressif\idf_cmd_init.bat" esp-idf-29323a3f5a0574597d6dbaa0af20c775"
```

**Процесс:**

1. Завершить TODO (создать файлы, добавить код)
2. Запустить сборку: `idf.py build`
3. Если есть ошибки компиляции/линковки → ИСПРАВИТЬ немедленно
4. Только после успешной сборки → переходить к следующему TODO

**Цель:** Избежать накопления ошибок, обнаруживать проблемы на ранней стадии

## 9. Финальное тестирование

- Полная компиляция проекта без ошибок
- Проверка размера бинарника (не превышает partition size)
- **НОВОЕ: Тест чтения реальных значений датчиков pH и EC**
- **НОВОЕ: Проверка порогов срабатывания PID**
- Проверка загрузки/сохранения PID конфигурации в NVS (включая пороги)
- Тест калибровки насосов (на реальном или mock оборудовании)
- Проверка UI навигации по всем новым экранам
- **НОВОЕ: Проверка экрана настройки порогов через UI**
- Проверка сохранения всех настроек через config_manager
- Проверка логирования на SD карту
- Проверка обновления графика в реальном времени
- Тест обработки ошибок (3 попытки запуска насоса)
- **НОВОЕ: Тест поведения при невалидных данных датчиков**
- Проверка суточных лимитов и автосброса в полночь

## Итог

Полная замена простой коррекции на профессиональную PID систему с:

- **Использованием реальных значений датчиков pH и EC**
- **Настраиваемыми порогами срабатывания через UI**
- **Сохранением порогов в NVS через config_manager**
- Расширенными настройками и калибровкой насосов
- UI экранами для всех аспектов управления
- Persistence всех настроек в NVS
- Логированием на SD карту через data_logger
- Thread-safe реализацией с отдельными мьютексами
- Инкрементальной сборкой для раннего обнаружения ошибок
- Проверкой валидности данных датчиков перед коррекцией

