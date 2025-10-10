# План реализации PID системы для Hydro 1.0

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
} pid_config_t;

// В system_config_t добавить:
pid_config_t pump_pid[PUMP_INDEX_COUNT];  // PID для 6 насосов
```

**Значения по умолчанию (в config_manager.c):**

- **pH UP/DOWN:** Kp=2.0, Ki=0.5, Kd=0.1, output_min=1.0, output_max=50.0, deadband=0.05, integral_max=100.0, sample_time=5000, max_dose=10.0, cooldown=60000, max_daily=500
- **EC A/B/C:** Kp=1.5, Ki=0.3, Kd=0.05, output_min=1.0, output_max=100.0, deadband=0.1, integral_max=200.0, sample_time=10000, max_dose=20.0, cooldown=120000, max_daily=1000
- **Water:** Kp=1.0, Ki=0.2, Kd=0.0, output_min=5.0, output_max=200.0, deadband=0.05, integral_max=150.0, sample_time=10000, max_dose=50.0, cooldown=120000, max_daily=2000

## 3. Обновление config_manager

**Файл:** `components/config_manager/config_manager.c`

Расширить функции:

- `config_load()` - загрузка PID настроек из NVS
- `config_save()` - сохранение PID настроек в NVS
- `config_manager_get_defaults()` - установка дефолтных PID значений (включая все расширенные параметры)

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

## 4. Переписать ph_ec_controller

**Файл:** `components/ph_ec_controller/ph_ec_controller.c`

**Удалить:**

- Старую пропорциональную логику в `ph_ec_controller_correct_ph()` (строки 202-246)
- Старую логику в `ph_ec_controller_correct_ec()` (строки 249-311)

**Добавить:**

- Включение `pump_manager.h`
- Вызовы `pump_manager_compute_and_execute()` вместо прямого управления насосами
- Проверку режима (auto/manual) перед коррекцией

**Новая логика коррекции pH:**

```c
esp_err_t ph_ec_controller_correct_ph(float current_ph) {
    if (!g_ph_auto_mode) return ESP_OK;
    
    // Выбор насоса
    pump_index_t pump_idx = (current_ph > g_ph_params.target_ph) 
        ? PUMP_INDEX_PH_DOWN : PUMP_INDEX_PH_UP;
    
    // PID расчет и выполнение
    return pump_manager_compute_and_execute(pump_idx, current_ph, g_ph_params.target_ph);
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
│   └── pump_calibration_screen.c/h  - Калибровка насосов (НОВЫЙ!)
└── pid/
    ├── pid_main_screen.c/h          - Список PID контроллеров
    ├── pid_detail_screen.c/h        - Детали одного PID
    ├── pid_tuning_screen.c/h        - Настройка Kp/Ki/Kd
    └── pid_graph_screen.c/h         - График real-time
```

### 5.2 Экран pumps_status_screen.c

**Содержимое:**

- Таблица 6 насосов (имя, статус, последний запуск, общий объем)
- Иконки статуса (активен/остановлен/ошибка)
- Кнопка перехода к ручному управлению
- Callback для обновления статуса в реальном времени

**Регистрация:** `"pumps_status"`

### 5.3 Экран pid_main_screen.c

**Содержимое:**

- Список 6 PID контроллеров
- Для каждого: имя насоса, Kp/Ki/Kd, enabled/disabled
- Клик на элемент → переход к pid_detail_screen
- Кнопка "Настроить все"

**Регистрация:** `"pid_main"`

### 5.4 Экран pid_detail_screen.c

**Содержимое:**

- Детали одного PID контроллера
- Текущие параметры: Kp, Ki, Kd, setpoint
- Текущее состояние: P-term, I-term, D-term, Output
- Статистика: количество запусков, общий объем
- Кнопки: "Настроить", "Сбросить интеграл", "Тест"

**Регистрация:** `"pid_detail"`

### 5.5 Экран pid_tuning_screen.c (Базовые настройки)

**Содержимое:**

- Энкодер-редакторы для Kp, Ki, Kd
- Слайдеры или числовые поля с мягкой валидацией (предупреждения при экстремальных значениях)
- Кнопка "Сохранить" → запись в config_manager через config_save()
- Кнопка "По умолчанию" → восстановление дефолтных Kp/Ki/Kd
- Кнопка "Расширенные" → переход к pid_advanced_screen

**Регистрация:** `"pid_tuning"`

**Валидация:** Kp: 0-10 (warn >5), Ki: 0-5 (warn >3), Kd: 0-2 (warn >1)

### 5.6 Экран pid_advanced_screen.c (Расширенные настройки) **[НОВЫЙ]**

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

- Кнопка "Сохранить" → запись всех параметров в config_manager
- Кнопка "По умолчанию" → восстановление всех дефолтных значений
- Кнопка "Назад"

**Регистрация:** `"pid_advanced"`

**Валидация с предупреждениями:**

- output_min: 0.1-10 (warn если <1 или >5)
- output_max: 10-500 (warn если >200)
- max_dose_per_cycle: 1-100 (warn если >50)
- cooldown_time_ms: 10000-600000 (warn если <60000)

### 5.6 Экран pid_graph_screen.c

**Содержимое:**

- График в реальном времени (LVGL chart)
- Ось Y: значение сенсора (pH/EC)
- Ось X: время (последние 60 секунд)
- Линии: setpoint (зеленая), current value (синяя), output (красная)
- Обновление каждые 2 секунды

**Регистрация:** `"pid_graph"`

## 6. Интеграция экранов

**Файл:** `components/lvgl_ui/screen_manager/screen_init.c`

Добавить в функцию инициализации:

```c
screen_register("pumps_status", pumps_status_screen_create, ...);
screen_register("pid_main", pid_main_screen_create, ...);
screen_register("pid_detail", pid_detail_screen_create, ...);
screen_register("pid_tuning", pid_tuning_screen_create, ...);
screen_register("pid_graph", pid_graph_screen_create, ...);
```

**Файл:** `components/lvgl_ui/screens/system/system_menu_screen.c`

Добавить пункты меню:

- "Статус насосов" → screen_show("pumps_status", NULL)
- "PID Настройки" → screen_show("pid_main", NULL)

## 7. Обновление CMakeLists

**Файл:** `components/pump_manager/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS "pump_manager.c"
    INCLUDE_DIRS "."
    REQUIRES peristaltic_pump freertos
)
```

**Файл:** `components/lvgl_ui/CMakeLists.txt`

Добавить новые экраны в SRCS:

```cmake
"screens/pumps/pumps_status_screen.c"
"screens/pumps/pumps_manual_screen.c"
"screens/pid/pid_main_screen.c"
"screens/pid/pid_detail_screen.c"
"screens/pid/pid_tuning_screen.c"
"screens/pid/pid_graph_screen.c"
```

### 5.7 Экран pump_calibration_screen.c (Калибровка насосов) **[НОВЫЙ]**

**Назначение:** Калибровка реального расхода каждого насоса для точной дозировки

**Содержимое:**

- Выбор насоса (dropdown или список из 6 насосов)
- Текущее значение flow_rate из конфигурации (мл/сек)
- Поле ввода времени калибровки (по умолчанию: 10000 мс)
- Кнопка "ЗАПУСТИТЬ КАЛИБРОВКУ" → запуск насоса на указанное время
- Таймер обратного отсчета во время работы насоса
- После остановки: поле ввода "Введите реальный объем (мл)"
- Автоматический расчет: `calibrated_flow_rate = real_volume_ml / (time_ms / 1000.0)`
- Отображение: "Новый расход: X.XX мл/сек (было: Y.YY мл/сек)"
- Кнопка "СОХРАНИТЬ" → запись в config.pump_config[idx].flow_rate_ml_per_sec через config_save()
- Кнопка "ОТМЕНА" / "ПОВТОРИТЬ"
- История последних калибровок (дата, старое/новое значение)

**Регистрация:** `"pump_calibration"`

**Обработчики событий:**

```c
static void on_start_calibration_click(lv_event_t *e) {
    pump_index_t pump_idx = get_selected_pump();
    uint32_t time_ms = get_calibration_time();
    
    // Popup подтверждения
    show_popup("Запустить насос на калибровку?\nВремя: " + time_ms + " мс", on_confirm_calibration);
}

static void on_confirm_calibration(bool confirmed) {
    if (!confirmed) return;
    
    // Прямой запуск насоса, игнорируя PID и лимиты
    pump_run_ms(pump_pins[pump_idx].ia, pump_pins[pump_idx].ib, time_ms);
    
    // Показать таймер обратного отсчета
    start_countdown_timer(time_ms);
    
    // После завершения → показать поле ввода реального объема
}

static void on_save_calibration_click(lv_event_t *e) {
    float real_volume = get_input_volume();
    float new_flow_rate = real_volume / (calibration_time_ms / 1000.0);
    
    // Сохранение в конфиг
    system_config_t config;
    config_load(&config);
    config.pump_config[pump_idx].flow_rate_ml_per_sec = new_flow_rate;
    config_save(&config);
    
    // Применить в pump_manager (если используется)
    // Или в ph_ec_controller
    ph_ec_controller_apply_config(&config);
    
    // Логирование калибровки
    data_logger_log_pump_calibration(pump_idx, old_flow_rate, new_flow_rate);
    
    show_popup("Калибровка сохранена!");
}
```

**Интеграция:**

- Добавить пункт "Калибровка насосов" в system_menu_screen → screen_show("pump_calibration", NULL)
- Также доступ с pumps_status_screen через кнопку "Калибровать"

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
- Проверка загрузки/сохранения PID конфигурации в NVS
- Тест калибровки насосов (на реальном или mock оборудовании)
- Проверка UI навигации по всем новым экранам
- Проверка сохранения всех настроек через config_manager
- Проверка логирования на SD карту
- Проверка обновления графика в реальном времени
- Тест обработки ошибок (3 попытки запуска насоса)
- Проверка суточных лимитов и автосброса в полночь

## Итог

Полная замена простой коррекции на профессиональную PID систему с:

- Расширенными настройками и калибровкой насосов
- UI экранами для всех аспектов управления
- Persistence всех настроек в NVS
- Логированием на SD карту через data_logger
- Thread-safe реализацией с отдельными мьютексами
- Инкрементальной сборкой для раннего обнаружения ошибок