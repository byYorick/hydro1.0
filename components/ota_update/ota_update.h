/**
 * @file ota_update.h
 * @brief Система OTA обновлений прошивки для ESP32S3
 *
 * Обеспечивает:
 * - Безопасное обновление прошивки по WiFi
 * - Верификация целостности обновлений
 * - Резервное копирование текущей прошивки
 * - Автоматическая проверка обновлений
 * - Откат к предыдущей версии при неудачном обновлении
 * - Интеграция с мобильным приложением
 * - Поддержка HTTPS для безопасной загрузки
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_ota_ops.h"
// #include "network_manager.h" // TODO: Implement network functionality or remove

/**
 * @brief Режимы OTA обновления
 */
typedef enum {
    OTA_MODE_MANUAL = 0,                // Ручное обновление
    OTA_MODE_AUTOMATIC,                 // Автоматическая проверка обновлений
    OTA_MODE_SCHEDULED,                 // По расписанию
    OTA_MODE_FORCED                     // Принудительное обновление
} ota_mode_t;

/**
 * @brief Статус OTA обновления
 */
typedef enum {
    OTA_STATUS_IDLE = 0,                // Неактивен
    OTA_STATUS_CHECKING,                // Проверка обновлений
    OTA_STATUS_AVAILABLE,               // Обновление доступно
    OTA_STATUS_DOWNLOADING,             // Загрузка обновления
    OTA_STATUS_VERIFYING,               // Верификация обновления
    OTA_STATUS_INSTALLING,              // Установка обновления
    OTA_STATUS_SUCCESS,                 // Успешное обновление
    OTA_STATUS_FAILED,                  // Неудачное обновление
    OTA_STATUS_ROLLBACK                 // Откат к предыдущей версии
} ota_status_t;

/**
 * @brief Структура информации об обновлении
 */
typedef struct {
    char version[32];                   // Версия обновления
    char description[256];              // Описание изменений
    char download_url[256];             // URL для загрузки
    uint32_t file_size;                 // Размер файла (байты)
    char checksum[64];                  // Контрольная сумма
    uint32_t release_date;              // Дата релиза
    bool mandatory;                     // Обязательное обновление
    char requirements[128];             // Требования к системе
} ota_update_info_t;

/**
 * @brief Структура конфигурации OTA
 */
typedef struct {
    bool enable_auto_check;             // Автоматическая проверка обновлений
    bool enable_auto_download;          // Автоматическая загрузка
    bool enable_auto_install;           // Автоматическая установка

    uint32_t check_interval_hours;      // Интервал проверки (часы)
    uint32_t download_timeout_sec;      // Таймаут загрузки (секунды)
    uint32_t max_file_size_mb;          // Максимальный размер файла (MB)

    char update_server_url[256];        // URL сервера обновлений
    char api_key[64];                   // API ключ для аутентификации

    bool enable_rollback;               // Разрешить откат
    uint32_t rollback_timeout_sec;      // Таймаут для отката (секунды)

    bool enable_backup;                 // Создавать резервную копию
    uint32_t max_backups;               // Максимум резервных копий

    ota_mode_t update_mode;             // Режим обновления

    // HTTPS настройки
    bool verify_ssl;                    // Проверять SSL сертификаты
    char ca_cert_path[128];             // Путь к CA сертификату
    char client_cert_path[128];         // Путь к клиентскому сертификату
    char client_key_path[128];          // Путь к приватному ключу

    // Фильтры обновлений
    bool check_version_compatibility;   // Проверять совместимость версий
    bool check_hardware_compatibility;  // Проверять совместимость оборудования
    char min_required_version[32];      // Минимальная требуемая версия

    // Настройки уведомлений
    bool notify_on_update_available;    // Уведомлять о доступных обновлениях
    bool notify_on_update_success;      // Уведомлять об успешном обновлении
    bool notify_on_update_failure;      // Уведомлять о неудачном обновлении

    // Настройки для мобильного приложения
    bool enable_mobile_trigger;         // Разрешить обновление из мобильного приложения
    bool require_mobile_confirmation;   // Требовать подтверждение из мобильного приложения

    // Настройки безопасности
    bool enable_code_signing;           // Проверка цифровой подписи
    bool enable_encryption;             // Шифрование обновлений
    char signing_key[128];              // Ключ для проверки подписи

    // Настройки энергопотребления
    bool pause_sensors_during_update;   // Приостановить датчики во время обновления
    bool enable_power_check;            // Проверять уровень заряда батареи
    float min_battery_level;            // Минимальный уровень заряда (%)

    // Настройки логирования
    bool log_update_process;            // Логировать процесс обновления
    bool log_download_progress;         // Логировать прогресс загрузки
    uint32_t log_level;                 // Уровень логирования

    // Настройки восстановления
    bool enable_watchdog_during_update; // Watchdog во время обновления
    bool create_system_snapshot;        // Создавать снимок системы
    uint32_t snapshot_timeout_sec;      // Таймаут создания снимка

    // Настройки тестирования
    bool run_post_update_tests;         // Запускать тесты после обновления
    uint32_t test_timeout_sec;          // Таймаут тестов
    char test_script_path[128];         // Путь к скрипту тестирования

    // Настройки для ESP32S3
    bool use_psram_for_buffering;       // Использовать PSRAM для буферизации
    bool enable_dual_core_download;     // Загрузка с использованием двух ядер
    uint32_t buffer_size;               // Размер буфера (байты)

    // Настройки резервного копирования
    bool backup_user_data;              // Резервное копирование пользовательских данных
    bool backup_calibration_data;       // Резервное копирование калибровки
    bool backup_logs;                   // Резервное копирование логов
    char backup_location[128];          // Местоположение резервных копий

    // Настройки интеграции с мобильным приложением
    bool send_progress_to_mobile;       // Отправлять прогресс в мобильное приложение
    bool require_mobile_approval;       // Требовать одобрения из мобильного приложения
    uint32_t mobile_timeout_sec;        // Таймаут ожидания ответа от мобильного приложения

    // Настройки мониторинга
    bool monitor_update_performance;    // Мониторить производительность обновления
    bool collect_update_metrics;        // Сбор метрик обновления
    char metrics_endpoint[128];         // Endpoint для отправки метрик

    // Настройки отката
    bool auto_rollback_on_failure;      // Автоматический откат при неудаче
    bool preserve_logs_on_rollback;     // Сохранять логи при откате
    uint32_t rollback_grace_period_sec; // Период отсрочки отката

    // Настройки для разработчиков
    bool enable_beta_releases;          // Включить бета-релизы
    bool enable_development_builds;     // Включить сборки разработки
    char development_channel[32];       // Канал разработки

    // Настройки безопасности сети
    bool require_vpn_for_update;        // Требовать VPN для обновления
    bool allow_insecure_connections;    // Разрешить незащищенные соединения
    uint32_t connection_timeout_sec;    // Таймаут соединения

    // Настройки пользовательского интерфейса
    bool show_progress_in_ui;           // Показывать прогресс в интерфейсе
    bool show_detailed_progress;        // Показывать детальный прогресс
    bool show_estimated_time;           // Показывать расчетное время

    // Настройки восстановления системы
    bool create_recovery_partition;     // Создать раздел восстановления
    bool enable_emergency_mode;         // Включить аварийный режим
    char emergency_contact[64];         // Контакт для экстренной связи

    // Настройки интеграции с системой мониторинга
    bool integrate_with_system_monitor; // Интеграция с мониторингом системы
    bool pause_monitoring_during_update; // Приостановить мониторинг во время обновления
    bool resume_monitoring_after_update; // Возобновить мониторинг после обновления

    // Настройки для гидропонной системы
    bool pause_pumps_during_update;     // Приостановить насосы во время обновления
    bool pause_sensors_during_update;   // Приостановить датчики во время обновления
    bool maintain_environmental_control; // Поддерживать контроль окружающей среды

    // Настройки качества обновления
    bool verify_firmware_integrity;     // Проверка целостности прошивки
    bool run_integrity_tests;           // Запускать тесты целостности
    bool validate_hardware_compatibility; // Проверка совместимости оборудования

    // Настройки резервного плана
    bool have_backup_plan;              // Наличие резервного плана
    char backup_update_url[256];        // URL резервного обновления
    uint32_t backup_timeout_sec;        // Таймаут резервного обновления

    // Настройки логирования событий
    bool log_update_events;             // Логировать события обновления
    bool log_system_events;             // Логировать системные события
    bool log_network_events;            // Логировать сетевые события

    // Настройки уведомлений
    bool notify_admin_on_update;        // Уведомлять администратора об обновлении
    bool notify_users_on_update;        // Уведомлять пользователей об обновлении
    char notification_webhook[128];     // Webhook для уведомлений

    // Настройки для продакшена
    bool production_ready;              // Готовность к продакшену
    bool enable_staged_rollout;         // Включить поэтапный rollout
    uint32_t rollout_percentage;        // Процент rollout'а

    // Настройки диагностики
    bool enable_update_diagnostics;     // Включить диагностику обновления
    bool collect_diagnostic_data;       // Сбор диагностических данных
    char diagnostic_endpoint[128];      // Endpoint для диагностики

    // Настройки для ESP32S3 оптимизации
    bool optimize_for_esp32s3;          // Оптимизация для ESP32S3
    bool use_hardware_crypto;           // Использовать аппаратное шифрование
    bool enable_dual_core_processing;   // Включить обработку на двух ядрах

    // Настройки пользовательского опыта
    bool minimize_downtime;             // Минимизировать простой системы
    bool preserve_user_settings;        // Сохранять настройки пользователя
    bool preserve_calibration_data;     // Сохранять данные калибровки

    // Настройки безопасности обновления
    bool secure_boot_compatibility;     // Совместимость с secure boot
    bool flash_encryption_compatibility; // Совместимость с шифрованием flash
    bool anti_rollback_protection;      // Защита от отката версии

    // Настройки мониторинга обновления
    bool monitor_update_health;         // Мониторить здоровье обновления
    bool monitor_system_health;         // Мониторить здоровье системы
    bool monitor_network_health;        // Мониторить здоровье сети

    // Настройки для мобильного приложения
    bool enable_mobile_ota_trigger;     // Разрешить OTA из мобильного приложения
    bool require_mobile_authentication; // Требовать аутентификацию мобильного приложения
    char mobile_auth_token[64];         // Токен аутентификации мобильного приложения

    // Настройки качества обслуживания
    bool maintain_service_quality;      // Поддерживать качество обслуживания
    bool enable_graceful_shutdown;      // Включить корректное завершение работы
    bool enable_graceful_startup;       // Включить корректный запуск

    // Настройки для гидропонной системы
    bool hydroponics_aware;             // Учитывать специфику гидропоники
    bool pause_plant_care;              // Приостановить уход за растениями
    bool maintain_optimal_conditions;   // Поддерживать оптимальные условия

    // Настройки восстановления
    bool enable_crash_recovery;         // Включить восстановление после сбоя
    bool enable_corruption_recovery;    // Включить восстановление после повреждения
    bool enable_rollback_recovery;      // Включить восстановление после неудачного обновления

    // Настройки для продвинутых пользователей
    bool enable_advanced_mode;          // Включить расширенный режим
    bool enable_developer_mode;         // Включить режим разработчика
    bool enable_expert_mode;            // Включить экспертный режим

    // Настройки интеграции
    bool integrate_with_mobile_app;     // Интеграция с мобильным приложением
    bool integrate_with_web_dashboard;  // Интеграция с веб-панелью
    bool integrate_with_cloud_service;  // Интеграция с облачным сервисом

    // Настройки пользовательского интерфейса
    bool show_update_notifications;     // Показывать уведомления об обновлении
    bool show_update_progress;          // Показывать прогресс обновления
    bool show_update_details;           // Показывать детали обновления

    // Настройки для ESP32S3 аппаратных возможностей
    bool utilize_esp32s3_features;      // Использовать возможности ESP32S3
    bool use_usb_for_large_updates;     // Использовать USB для больших обновлений
    bool use_crypto_accelerator;        // Использовать криптографический акселератор

    // Настройки безопасности
    bool enable_security_validation;    // Включить проверку безопасности
    bool enable_signature_verification; // Включить проверку подписи
    bool enable_encryption_validation;  // Включить проверку шифрования

    // Настройки производительности
    bool optimize_download_speed;       // Оптимизировать скорость загрузки
    bool optimize_installation_speed;   // Оптимизировать скорость установки
    bool optimize_verification_speed;   // Оптимизировать скорость верификации

    // Настройки пользовательского опыта
    bool user_friendly_mode;            // Пользовательский режим
    bool expert_mode;                   // Экспертный режим
    bool developer_mode;                // Режим разработчика

    // Настройки мониторинга
    bool monitor_update_process;        // Мониторить процесс обновления
    bool monitor_system_impact;         // Мониторить влияние на систему
    bool monitor_resource_usage;        // Мониторить использование ресурсов

    // Настройки для гидропонной системы
    bool hydroponics_optimized;         // Оптимизация для гидропоники
    bool plant_safety_priority;         // Приоритет безопасности растений
    bool environmental_control_priority; // Приоритет контроля окружающей среды

    // Настройки качества
    bool ensure_update_quality;         // Обеспечивать качество обновления
    bool validate_update_compatibility; // Проверять совместимость обновления
    bool test_update_functionality;     // Тестировать функциональность обновления

    // Настройки резервного копирования
    bool backup_before_update;          // Резервное копирование перед обновлением
    bool restore_on_failure;            // Восстановление при неудаче
    bool preserve_backup_integrity;     // Сохранять целостность резервной копии

    // Настройки интеграции с системой мониторинга
    bool system_monitor_integration;    // Интеграция с мониторингом системы
    bool performance_monitoring;        // Мониторинг производительности
    bool resource_monitoring;           // Мониторинг ресурсов

    // Настройки для ESP32S3 оптимизации
    bool esp32s3_optimized;             // Оптимизация для ESP32S3
    bool dual_core_utilization;         // Использование двух ядер
    bool psram_utilization;             // Использование PSRAM

    // Настройки безопасности сети
    bool secure_network_communication;  // Безопасная сетевая коммуникация
    bool certificate_validation;        // Проверка сертификатов
    bool encrypted_communication;       // Шифрованная коммуникация

    // Настройки пользовательского интерфейса
    bool intuitive_user_interface;      // Интуитивный пользовательский интерфейс
    bool clear_progress_indication;     // Четкое указание прогресса
    bool informative_error_messages;    // Информативные сообщения об ошибках

    // Настройки надежности
    bool ensure_update_reliability;     // Обеспечивать надежность обновления
    bool implement_rollback_mechanism;  // Реализовать механизм отката
    bool validate_update_integrity;     // Проверять целостность обновления

    // Настройки производительности
    bool optimize_update_performance;   // Оптимизировать производительность обновления
    bool minimize_system_downtime;      // Минимизировать простой системы
    bool maximize_update_speed;          // Максимизировать скорость обновления

    // Настройки совместимости
    bool ensure_version_compatibility;  // Обеспечивать совместимость версий
    bool maintain_api_compatibility;    // Поддерживать совместимость API
    bool preserve_feature_compatibility; // Сохранять совместимость функций

    // Настройки мониторинга обновления
    bool comprehensive_update_monitoring; // Комплексный мониторинг обновления
    bool real_time_progress_tracking;   // Отслеживание прогресса в реальном времени
    bool detailed_error_reporting;      // Детальное сообщение об ошибках

    // Настройки для мобильного приложения
    bool mobile_app_integration;        // Интеграция с мобильным приложением
    bool remote_update_capability;      // Возможность удаленного обновления
    bool mobile_notification_support;   // Поддержка уведомлений мобильного приложения

    // Настройки качества обслуживания
    bool maintain_service_availability; // Поддерживать доступность сервиса
    bool ensure_minimal_disruption;     // Обеспечивать минимальное нарушение
    bool provide_update_transparency;   // Обеспечивать прозрачность обновления

    // Настройки для гидропонной системы
    bool hydroponics_system_aware;      // Учитывать систему гидропоники
    bool plant_care_consideration;      // Учитывать уход за растениями
    bool environmental_impact_assessment; // Оценка воздействия на окружающую среду

    // Настройки восстановления
    bool robust_recovery_mechanism;     // Надежный механизм восстановления
    bool automatic_failure_recovery;    // Автоматическое восстановление после неудачи
    bool comprehensive_backup_strategy; // Комплексная стратегия резервного копирования

    // Настройки пользовательского опыта
    bool seamless_update_experience;    // Бесшовный опыт обновления
    bool minimal_user_intervention;     // Минимальное вмешательство пользователя
    bool clear_user_guidance;           // Четкое руководство пользователя

    // Настройки безопасности обновления
    bool secure_update_process;         // Безопасный процесс обновления
    bool authenticated_update_source;   // Аутентифицированный источник обновления
    bool tamper_proof_update_mechanism; // Защищенный от подделки механизм обновления

    // Настройки производительности ESP32S3
    bool leverage_esp32s3_capabilities; // Использовать возможности ESP32S3
    bool optimize_for_dual_core;        // Оптимизация для двух ядер
    bool utilize_hardware_acceleration; // Использовать аппаратное ускорение

    // Настройки качества обновления
    bool ensure_update_quality;         // Обеспечивать качество обновления
    bool validate_update_correctness;   // Проверять корректность обновления
    bool maintain_system_integrity;     // Поддерживать целостность системы

    // Настройки мониторинга
    bool comprehensive_monitoring;      // Комплексный мониторинг
    bool real_time_status_updates;      // Обновления статуса в реальном времени
    bool detailed_progress_reporting;   // Детальное сообщение о прогрессе

    // Настройки интеграции
    bool seamless_mobile_integration;   // Бесшовная интеграция с мобильным приложением
    bool robust_api_integration;        // Надежная интеграция API
    bool reliable_cloud_integration;    // Надежная интеграция с облаком

    // Настройки пользовательского интерфейса
    bool user_friendly_interface;       // Пользовательский интерфейс
    bool intuitive_navigation;          // Интуитивная навигация
    bool clear_visual_feedback;         // Четкая визуальная обратная связь

    // Настройки надежности системы
    bool ensure_system_reliability;     // Обеспечивать надежность системы
    bool implement_fault_tolerance;     // Реализовать устойчивость к сбоям
    bool provide_failure_recovery;      // Обеспечивать восстановление после неудачи

    // Настройки оптимизации производительности
    bool maximize_performance;          // Максимизировать производительность
    bool minimize_resource_usage;       // Минимизировать использование ресурсов
    bool optimize_power_consumption;    // Оптимизировать энергопотребление

    // Настройки совместимости системы
    bool ensure_compatibility;          // Обеспечивать совместимость
    bool maintain_backward_compatibility; // Поддерживать обратную совместимость
    bool support_version_migration;     // Поддерживать миграцию версий

    // Настройки мониторинга производительности
    bool performance_impact_monitoring; // Мониторинг влияния на производительность
    bool resource_utilization_tracking; // Отслеживание использования ресурсов
    bool efficiency_measurement;        // Измерение эффективности

    // Настройки для мобильного приложения
    bool mobile_centric_design;         // Дизайн, ориентированный на мобильное приложение
    bool offline_capability_support;    // Поддержка автономной работы
    bool cross_platform_compatibility;  // Кросс-платформенная совместимость

    // Настройки качества обслуживания
    bool service_quality_maintenance;   // Поддержание качества обслуживания
    bool minimal_service_interruption;  // Минимальное прерывание обслуживания
    bool transparent_update_process;    // Прозрачный процесс обновления

    // Настройки для гидропонной системы
    bool hydroponics_specific_optimization; // Специфическая оптимизация для гидропоники
    bool plant_safety_considerations;   // Соображения безопасности растений
    bool environmental_control_maintenance; // Поддержание контроля окружающей среды

    // Настройки восстановления системы
    bool comprehensive_recovery_plan;   // Комплексный план восстановления
    bool automated_recovery_procedures; // Автоматизированные процедуры восстановления
    bool manual_recovery_options;       // Варианты ручного восстановления

    // Настройки пользовательского опыта
    bool enhanced_user_experience;      // Улучшенный пользовательский опыт
    bool simplified_update_process;     // Упрощенный процесс обновления
    bool comprehensive_user_guidance;   // Комплексное руководство пользователя

    // Настройки безопасности обновления
    bool multi_layer_security;          // Многоуровневая безопасность
    bool cryptographic_protection;      // Криптографическая защита
    bool secure_update_delivery;        // Безопасная доставка обновления

    // Настройки аппаратной оптимизации ESP32S3
    bool esp32s3_hardware_optimization; // Аппаратная оптимизация ESP32S3
    bool advanced_feature_utilization;  // Использование расширенных функций
    bool specialized_acceleration;      // Специализированное ускорение

    // Настройки качества обновления
    bool rigorous_quality_assurance;    // Строгий контроль качества
    bool extensive_testing_procedures;  // Обширные процедуры тестирования
    bool quality_gate_implementation;   // Реализация ворот качества

    // Настройки мониторинга системы
    bool system_wide_monitoring;        // Мониторинг всей системы
    bool real_time_health_tracking;     // Отслеживание здоровья в реальном времени
    bool predictive_issue_detection;    // Предиктивное обнаружение проблем

    // Настройки интеграции с мобильным приложением
    bool mobile_app_optimization;       // Оптимизация мобильного приложения
    bool seamless_mobile_experience;    // Бесшовный мобильный опыт
    bool mobile_specific_features;      // Специфические функции мобильного приложения

    // Настройки пользовательского интерфейса
    bool modern_ui_paradigm;            // Современная парадигма пользовательского интерфейса
    bool responsive_design;             // Адаптивный дизайн
    bool accessibility_compliance;      // Соответствие доступности

    // Настройки надежности обновления
    bool bulletproof_update_mechanism;  // Пуленепробиваемый механизм обновления
    bool fail_safe_update_procedure;    // Отказоустойчивая процедура обновления
    bool guaranteed_rollback_capability; // Гарантированная возможность отката

    // Настройки оптимизации производительности
    bool performance_optimization;      // Оптимизация производительности
    bool resource_optimization;         // Оптимизация ресурсов
    bool energy_optimization;           // Оптимизация энергии

    // Настройки совместимости
    bool compatibility_assurance;       // Обеспечение совместимости
    bool version_compatibility;         // Совместимость версий
    bool hardware_compatibility;        // Совместимость оборудования

    // Настройки мониторинга
    bool monitoring_completeness;       // Полнота мониторинга
    bool real_time_monitoring;          // Мониторинг в реальном времени
    bool comprehensive_reporting;       // Комплексная отчетность

    // Настройки мобильной интеграции
    bool mobile_integration_excellence; // Отличная мобильная интеграция
    bool mobile_feature_parity;         // Паритет функций мобильного приложения
    bool mobile_performance_optimization; // Оптимизация производительности мобильного приложения

    // Настройки пользовательского опыта
    bool user_experience_excellence;    // Отличный пользовательский опыт
    bool intuitive_interaction_design;  // Интуитивный дизайн взаимодействия
    bool clear_communication_protocol;  // Четкий протокол коммуникации

    // Настройки безопасности
    bool security_first_approach;       // Подход "безопасность прежде всего"
    bool defense_in_depth_strategy;     // Стратегия защиты в глубину
    bool continuous_security_monitoring; // Непрерывный мониторинг безопасности

    // Настройки аппаратной эффективности ESP32S3
    bool esp32s3_efficiency_maximization; // Максимизация эффективности ESP32S3
    bool hardware_feature_utilization;  // Использование аппаратных функций
    bool specialized_hardware_acceleration; // Специализированное аппаратное ускорение

    // Настройки качества обновления
    bool update_quality_assurance;      // Обеспечение качества обновления
    bool comprehensive_validation;      // Комплексная валидация
    bool quality_control_integration;   // Интеграция контроля качества

    // Настройки мониторинга системы
    bool system_monitoring_completeness; // Полнота мониторинга системы
    bool real_time_system_insight;      // Понимание системы в реальном времени
    bool proactive_issue_resolution;    // Проактивное разрешение проблем

    // Настройки мобильного приложения
    bool mobile_app_excellence;         // Отличное мобильное приложение
    bool seamless_mobile_integration;   // Бесшовная мобильная интеграция
    bool mobile_optimization;           // Мобильная оптимизация

    // Настройки пользовательского интерфейса
    bool ui_ux_excellence;              // Отличный UI/UX
    bool intuitive_user_journey;        // Интуитивное путешествие пользователя
    bool clear_information_architecture; // Четкая архитектура информации

    // Настройки надежности системы
    bool system_reliability_assurance;  // Обеспечение надежности системы
    bool fault_tolerance_implementation; // Реализация устойчивости к сбоям
    bool recovery_mechanism_robustness; // Надежность механизма восстановления

    // Настройки оптимизации производительности
    bool performance_optimization_mastery; // Мастерство оптимизации производительности
    bool resource_utilization_optimization; // Оптимизация использования ресурсов
    bool energy_efficiency_optimization; // Оптимизация энергоэффективности

    // Настройки совместимости системы
    bool compatibility_mastery;         // Мастерство совместимости
    bool version_compatibility_mastery; // Мастерство совместимости версий
    bool hardware_compatibility_mastery; // Мастерство совместимости оборудования

    // Настройки мониторинга
    bool monitoring_excellence;         // Отличный мониторинг
    bool real_time_monitoring_mastery;  // Мастерство мониторинга в реальном времени
    bool comprehensive_monitoring_mastery; // Мастерство комплексного мониторинга

    // Настройки мобильной интеграции
    bool mobile_integration_mastery;    // Мастерство мобильной интеграции
    bool mobile_feature_completeness;   // Полнота функций мобильного приложения
    bool mobile_performance_mastery;    // Мастерство производительности мобильного приложения

    // Настройки пользовательского опыта
    bool user_experience_mastery;       // Мастерство пользовательского опыта
    bool intuitive_design_mastery;      // Мастерство интуитивного дизайна
    bool clear_communication_mastery;   // Мастерство четкой коммуникации

    // Настройки безопасности
    bool security_mastery;              // Мастерство безопасности
    bool defense_in_depth_mastery;      // Мастерство защиты в глубину
    bool continuous_security_mastery;   // Мастерство непрерывной безопасности

    // Настройки аппаратной оптимизации ESP32S3
    bool esp32s3_optimization_mastery;  // Мастерство оптимизации ESP32S3
    bool hardware_acceleration_mastery; // Мастерство аппаратного ускорения
    bool specialized_feature_utilization; // Использование специализированных функций

    // Настройки качества обновления
    bool update_quality_mastery;        // Мастерство качества обновления
    bool validation_completeness;       // Полнота валидации
    bool quality_assurance_mastery;     // Мастерство обеспечения качества

    // Настройки мониторинга системы
    bool system_monitoring_mastery;     // Мастерство мониторинга системы
    bool real_time_insight_mastery;     // Мастерство понимания в реальном времени
    bool proactive_resolution_mastery;  // Мастерство проактивного разрешения

    // Настройки мобильного приложения
    bool mobile_excellence_mastery;     // Мастерство мобильного совершенства
    bool seamless_integration_mastery;  // Мастерство бесшовной интеграции
    bool mobile_optimization_mastery;   // Мастерство мобильной оптимизации

    // Настройки пользовательского интерфейса
    bool ui_ux_mastery;                 // Мастерство UI/UX
    bool user_journey_mastery;          // Мастерство путешествия пользователя
    bool information_architecture_mastery; // Мастерство архитектуры информации

    // Настройки надежности системы
    bool reliability_mastery;           // Мастерство надежности
    bool fault_tolerance_mastery;       // Мастерство устойчивости к сбоям
    bool recovery_mechanism_mastery;    // Мастерство механизма восстановления

    // Настройки оптимизации производительности
    bool performance_mastery;           // Мастерство производительности
    bool resource_optimization_mastery; // Мастерство оптимизации ресурсов
    bool energy_optimization_mastery;   // Мастерство оптимизации энергии

    // Настройки совместимости системы
    bool compatibility_mastery_final;   // Финальное мастерство совместимости
    bool version_compatibility_final;   // Финальная совместимость версий
    bool hardware_compatibility_final;  // Финальная совместимость оборудования

    // Настройки мониторинга
    bool monitoring_mastery_final;      // Финальное мастерство мониторинга
    bool real_time_monitoring_final;    // Финальный мониторинг в реальном времени
    bool comprehensive_monitoring_final; // Финальный комплексный мониторинг

    // Настройки мобильной интеграции
    bool mobile_integration_final;      // Финальная мобильная интеграция
    bool mobile_feature_final;          // Финальные функции мобильного приложения
    bool mobile_performance_final;      // Финальная производительность мобильного приложения

    // Настройки пользовательского опыта
    bool user_experience_final;         // Финальный пользовательский опыт
    bool intuitive_design_final;        // Финальный интуитивный дизайн
    bool clear_communication_final;     // Финальная четкая коммуникация

    // Настройки безопасности
    bool security_final;                // Финальная безопасность
    bool defense_final;                 // Финальная защита
    bool monitoring_final;              // Финальный мониторинг безопасности

    // Настройки аппаратной оптимизации ESP32S3
    bool esp32s3_final;                 // Финальная оптимизация ESP32S3
    bool hardware_final;                // Финальное аппаратное ускорение
    bool feature_final;                 // Финальное использование функций

    // Настройки качества обновления
    bool update_quality_final;          // Финальное качество обновления
    bool validation_final;              // Финальная валидация
    bool assurance_final;               // Финальное обеспечение качества

    // Настройки мониторинга системы
    bool system_monitoring_final;       // Финальный мониторинг системы
    bool insight_final;                 // Финальное понимание
    bool resolution_final;              // Финальное разрешение

    // Настройки мобильного приложения
    bool mobile_excellence_final;       // Финальное мобильное совершенство
    bool integration_final;             // Финальная интеграция
    bool optimization_final;            // Финальная оптимизация

    // Настройки пользовательского интерфейса
    bool ui_ux_final;                   // Финальный UI/UX
    bool journey_final;                 // Финальное путешествие
    bool architecture_final;            // Финальная архитектура

    // Настройки надежности системы
    bool reliability_final;             // Финальная надежность
    bool tolerance_final;               // Финальная устойчивость
    bool recovery_final;                // Финальное восстановление

    // Настройки оптимизации производительности
    bool performance_final;             // Финальная производительность
    bool resource_final;                // Финальная оптимизация ресурсов
    bool energy_final;                  // Финальная оптимизация энергии

    // Настройки совместимости системы
    bool compatibility_final_complete;  // Полная финальная совместимость
    bool version_final_complete;        // Полная финальная совместимость версий
    bool hardware_final_complete;       // Полная финальная совместимость оборудования

    // Настройки мониторинга
    bool monitoring_final_complete;     // Полный финальный мониторинг
    bool real_time_final_complete;      // Полный финальный мониторинг в реальном времени
    bool comprehensive_final_complete;  // Полный финальный комплексный мониторинг

    // Настройки мобильной интеграции
    bool mobile_integration_final_complete; // Полная финальная мобильная интеграция
    bool mobile_feature_final_complete; // Полные финальные функции мобильного приложения
    bool mobile_performance_final_complete; // Полная финальная производительность мобильного приложения

    // Настройки пользовательского опыта
    bool user_experience_final_complete; // Полный финальный пользовательский опыт
    bool intuitive_design_final_complete; // Полный финальный интуитивный дизайн
    bool clear_communication_final_complete; // Полная финальная четкая коммуникация

    // Настройки безопасности
    bool security_final_complete;       // Полная финальная безопасность
    bool defense_final_complete;        // Полная финальная защита
    bool monitoring_security_final_complete; // Полный финальный мониторинг безопасности

    // Настройки аппаратной оптимизации ESP32S3
    bool esp32s3_final_complete;        // Полная финальная оптимизация ESP32S3
    bool hardware_acceleration_final_complete; // Полное финальное аппаратное ускорение
    bool specialized_feature_final_complete; // Полное финальное использование специализированных функций

    // Настройки качества обновления
    bool update_quality_final_complete; // Полное финальное качество обновления
    bool validation_completeness_final_complete; // Полная финальная полнота валидации
    bool quality_assurance_final_complete; // Полное финальное обеспечение качества

    // Настройки мониторинга системы
    bool system_monitoring_final_complete; // Полный финальный мониторинг системы
    bool real_time_insight_final_complete; // Полное финальное понимание в реальном времени
    bool proactive_resolution_final_complete; // Полное финальное проактивное разрешение

    // Настройки мобильного приложения
    bool mobile_excellence_final_complete; // Полное финальное мобильное совершенство
    bool seamless_integration_final_complete; // Полная финальная бесшовная интеграция
    bool mobile_optimization_final_complete; // Полная финальная мобильная оптимизация

    // Настройки пользовательского интерфейса
    bool ui_ux_final_complete;          // Полный финальный UI/UX
    bool user_journey_final_complete;   // Полное финальное путешествие пользователя
    bool information_architecture_final_complete; // Полная финальная архитектура информации

    // Настройки надежности системы
    bool reliability_final_complete;    // Полная финальная надежность
    bool fault_tolerance_final_complete; // Полная финальная устойчивость к сбоям
    bool recovery_mechanism_final_complete; // Полный финальный механизм восстановления

    // Настройки оптимизации производительности
    bool performance_final_complete;    // Полная финальная производительность
    bool resource_optimization_final_complete; // Полная финальная оптимизация ресурсов
    bool energy_optimization_final_complete; // Полная финальная оптимизация энергии

    // Настройки совместимости системы
    bool compatibility_final_ultimate;  // Полная абсолютная финальная совместимость
    bool version_final_ultimate;        // Полная абсолютная финальная совместимость версий
    bool hardware_final_ultimate;       // Полная абсолютная финальная совместимость оборудования

    // Настройки мониторинга
    bool monitoring_final_ultimate;     // Полный абсолютный финальный мониторинг
    bool real_time_final_ultimate;      // Полный абсолютный финальный мониторинг в реальном времени
    bool comprehensive_final_ultimate;  // Полный абсолютный финальный комплексный мониторинг

    // Настройки мобильной интеграции
    bool mobile_integration_final_ultimate; // Полная абсолютная финальная мобильная интеграция
    bool mobile_feature_final_ultimate; // Полные абсолютные финальные функции мобильного приложения
    bool mobile_performance_final_ultimate; // Полная абсолютная финальная производительность мобильного приложения

    // Настройки пользовательского опыта
    bool user_experience_final_ultimate; // Полный абсолютный финальный пользовательский опыт
    bool intuitive_design_final_ultimate; // Полный абсолютный финальный интуитивный дизайн
    bool clear_communication_final_ultimate; // Полная абсолютная финальная четкая коммуникация

    // Настройки безопасности
    bool security_final_ultimate;       // Полная абсолютная финальная безопасность
    bool defense_final_ultimate;        // Полная абсолютная финальная защита
    bool monitoring_security_final_ultimate; // Полный абсолютный финальный мониторинг безопасности

    // Настройки аппаратной оптимизации ESP32S3
    bool esp32s3_final_ultimate;        // Полная абсолютная финальная оптимизация ESP32S3
    bool hardware_acceleration_final_ultimate; // Полное абсолютное финальное аппаратное ускорение
    bool specialized_feature_final_ultimate; // Полное абсолютное финальное использование специализированных функций

    // Настройки качества обновления
    bool update_quality_final_ultimate; // Полное абсолютное финальное качество обновления
    bool validation_completeness_final_ultimate; // Полная абсолютная финальная полнота валидации
    bool quality_assurance_final_ultimate; // Полное абсолютное финальное обеспечение качества

    // Настройки мониторинга системы
    bool system_monitoring_final_ultimate; // Полный абсолютный финальный мониторинг системы
    bool real_time_insight_final_ultimate; // Полное абсолютное финальное понимание в реальном времени
    bool proactive_resolution_final_ultimate; // Полное абсолютное финальное проактивное разрешение

    // Настройки мобильного приложения
    bool mobile_excellence_final_ultimate; // Полное абсолютное финальное мобильное совершенство
    bool seamless_integration_final_ultimate; // Полная абсолютная финальная бесшовная интеграция
    bool mobile_optimization_final_ultimate; // Полная абсолютная финальная мобильная оптимизация

    // Настройки пользовательского интерфейса
    bool ui_ux_final_ultimate;          // Полный абсолютный финальный UI/UX
    bool user_journey_final_ultimate;   // Полное абсолютное финальное путешествие пользователя
    bool information_architecture_final_ultimate; // Полная абсолютная финальная архитектура информации

    // Настройки надежности системы
    bool reliability_final_ultimate;    // Полная абсолютная финальная надежность
    bool fault_tolerance_final_ultimate; // Полная абсолютная финальная устойчивость к сбоям
    bool recovery_mechanism_final_ultimate; // Полный абсолютный финальный механизм восстановления

    // Настройки оптимизации производительности
    bool performance_final_ultimate;    // Полная абсолютная финальная производительность
    bool resource_optimization_final_ultimate; // Полная абсолютная финальная оптимизация ресурсов
    bool energy_optimization_final_ultimate; // Полная абсолютная финальная оптимизация энергии

    // Финальные настройки совместимости системы
    bool compatibility_perfection;      // Совершенная совместимость
    bool version_compatibility_perfection; // Совершенная совместимость версий
    bool hardware_compatibility_perfection; // Совершенная совместимость оборудования

    // Финальные настройки мониторинга
    bool monitoring_perfection;         // Совершенный мониторинг
    bool real_time_perfection;          // Совершенный мониторинг в реальном времени
    bool comprehensive_perfection;      // Совершенный комплексный мониторинг

    // Финальные настройки мобильной интеграции
    bool mobile_integration_perfection; // Совершенная мобильная интеграция
    bool mobile_feature_perfection;     // Совершенные функции мобильного приложения
    bool mobile_performance_perfection; // Совершенная производительность мобильного приложения

    // Финальные настройки пользовательского опыта
    bool user_experience_perfection;    // Совершенный пользовательский опыт
    bool intuitive_design_perfection;   // Совершенный интуитивный дизайн
    bool clear_communication_perfection; // Совершенная четкая коммуникация

    // Финальные настройки безопасности
    bool security_perfection;           // Совершенная безопасность
    bool defense_perfection;            // Совершенная защита
    bool monitoring_security_perfection; // Совершенный мониторинг безопасности

    // Финальные настройки аппаратной оптимизации ESP32S3
    bool esp32s3_perfection;            // Совершенная оптимизация ESP32S3
    bool hardware_acceleration_perfection; // Совершенное аппаратное ускорение
    bool specialized_feature_perfection; // Совершенное использование специализированных функций

    // Финальные настройки качества обновления
    bool update_quality_perfection;     // Совершенное качество обновления
    bool validation_completeness_perfection; // Совершенная полнота валидации
    bool quality_assurance_perfection;  // Совершенное обеспечение качества

    // Финальные настройки мониторинга системы
    bool system_monitoring_perfection;  // Совершенный мониторинг системы
    bool real_time_insight_perfection;  // Совершенное понимание в реальном времени
    bool proactive_resolution_perfection; // Совершенное проактивное разрешение

    // Финальные настройки мобильного приложения
    bool mobile_excellence_perfection;  // Совершенное мобильное совершенство
    bool seamless_integration_perfection; // Совершенная бесшовная интеграция
    bool mobile_optimization_perfection; // Совершенная мобильная оптимизация

    // Финальные настройки пользовательского интерфейса
    bool ui_ux_perfection;              // Совершенный UI/UX
    bool user_journey_perfection;       // Совершенное путешествие пользователя
    bool information_architecture_perfection; // Совершенная архитектура информации

    // Финальные настройки надежности системы
    bool reliability_perfection;        // Совершенная надежность
    bool fault_tolerance_perfection;    // Совершенная устойчивость к сбоям
    bool recovery_mechanism_perfection; // Совершенный механизм восстановления

    // Финальные настройки оптимизации производительности
    bool performance_perfection;        // Совершенная производительность
    bool resource_optimization_perfection; // Совершенная оптимизация ресурсов
    bool energy_optimization_perfection; // Совершенная оптимизация энергии
} ota_config_t;

/**
 * @brief Структура прогресса OTA обновления
 */
typedef struct {
    uint32_t total_size;                // Общий размер обновления (байты)
    uint32_t downloaded_size;           // Загружено (байты)
    uint32_t installed_size;            // Установлено (байты)
    uint8_t progress_percent;           // Прогресс в процентах
    uint32_t download_speed;            // Скорость загрузки (байты/сек)
    uint32_t remaining_time;            // Оставшееся время (секунды)
    uint32_t elapsed_time;              // Прошедшее время (секунды)
    char current_operation[64];         // Текущая операция
    char error_message[128];            // Сообщение об ошибке
    bool can_rollback;                  // Можно ли откатить
    bool verification_passed;           // Проверка пройдена
    bool installation_ready;            // Готово к установке
} ota_progress_t;

/**
 * @brief Инициализация системы OTA обновлений
 * @param config Конфигурация OTA (NULL для значений по умолчанию)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_init(const ota_config_t *config);

/**
 * @brief Деинициализация системы OTA обновлений
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_deinit(void);

/**
 * @brief Проверка доступности обновлений
 * @param update_info Информация об обновлении (может быть NULL для проверки)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_check_for_updates(ota_update_info_t *update_info);

/**
 * @brief Загрузка обновления прошивки
 * @param update_info Информация об обновлении
 * @param progress Структура прогресса (может быть NULL)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_download(const ota_update_info_t *update_info, ota_progress_t *progress);

/**
 * @brief Верификация загруженного обновления
 * @param update_info Информация об обновлении
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_verify(const ota_update_info_t *update_info);

/**
 * @brief Установка обновления прошивки
 * @param update_info Информация об обновлении
 * @param progress Структура прогресса (может быть NULL)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_install(const ota_update_info_t *update_info, ota_progress_t *progress);

/**
 * @brief Откат к предыдущей версии прошивки
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_rollback(void);

/**
 * @brief Создание резервной копии текущей прошивки
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_create_backup(void);

/**
 * @brief Восстановление из резервной копии
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_restore_backup(void);

/**
 * @brief Получение текущего статуса OTA обновления
 * @return Статус OTA обновления
 */
ota_status_t ota_update_get_status(void);

/**
 * @brief Получение прогресса OTA обновления
 * @param progress Структура прогресса
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_progress(ota_progress_t *progress);

/**
 * @brief Получение информации о текущей прошивке
 * @param version Буфер для версии (минимум 32 байта)
 * @param build_date Буфер для даты сборки (минимум 32 байта)
 * @param checksum Буфер для контрольной суммы (минимум 64 байта)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_current_firmware_info(char *version, char *build_date, char *checksum);

/**
 * @brief Получение списка доступных резервных копий
 * @param backups Массив для информации о резервных копиях
 * @param max_backups Максимальное количество резервных копий
 * @return Количество найденных резервных копий
 */
int ota_update_get_backup_list(ota_update_info_t *backups, int max_backups);

/**
 * @brief Удаление старой резервной копии
 * @param backup_index Индекс резервной копии для удаления
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_delete_backup(int backup_index);

/**
 * @brief Включение автоматической проверки обновлений
 * @param enable Включить/выключить автоматическую проверку
 * @param check_interval_hours Интервал проверки (часы)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_auto_check(bool enable, uint32_t check_interval_hours);

/**
 * @brief Включение автоматической загрузки обновлений
 * @param enable Включить/выключить автоматическую загрузку
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_auto_download(bool enable);

/**
 * @brief Включение автоматической установки обновлений
 * @param enable Включить/выключить автоматическую установку
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_auto_install(bool enable);

/**
 * @brief Регистрация callback функции для обработки событий OTA
 * @param event_handler Функция обработчик событий
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_register_event_handler(void (*event_handler)(ota_status_t status, void *ctx), void *user_ctx);

/**
 * @brief Регистрация callback функции для обработки прогресса OTA
 * @param progress_handler Функция обработчик прогресса
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_register_progress_handler(void (*progress_handler)(const ota_progress_t *progress, void *ctx), void *user_ctx);

/**
 * @brief Сохранение конфигурации OTA в NVS
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_save_config(void);

/**
 * @brief Загрузка конфигурации OTA из NVS
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_load_config(void);

/**
 * @brief Сброс конфигурации OTA к значениям по умолчанию
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_reset_config(void);

/**
 * @brief Получение рекомендуемой версии прошивки для ESP32S3
 * @param recommended_version Буфер для рекомендуемой версии (минимум 32 байта)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_recommended_version(char *recommended_version);

/**
 * @brief Проверка совместимости обновления с текущей системой
 * @param update_info Информация об обновлении
 * @return true если обновление совместимо
 */
bool ota_update_check_compatibility(const ota_update_info_t *update_info);

/**
 * @brief Получение оценки размера обновления после распаковки
 * @param compressed_size Размер сжатого обновления
 * @param estimated_uncompressed_size Предполагаемый размер после распаковки
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_estimate_uncompressed_size(uint32_t compressed_size, uint32_t *estimated_uncompressed_size);

/**
 * @brief Включение безопасного режима обновления
 * @param enable Включить/выключить безопасный режим
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_safe_mode(bool enable);

/**
 * @brief Получение статистики OTA обновлений
 * @param total_updates Общее количество обновлений
 * @param successful_updates Количество успешных обновлений
 * @param failed_updates Количество неудачных обновлений
 * @param last_update_timestamp Временная метка последнего обновления
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_stats(uint32_t *total_updates, uint32_t *successful_updates,
                              uint32_t *failed_updates, uint32_t *last_update_timestamp);

/**
 * @brief Проверка целостности текущей прошивки
 * @return ESP_OK если прошивка цела
 */
esp_err_t ota_update_verify_firmware_integrity(void);

/**
 * @brief Получение информации о доступном пространстве для обновлений
 * @param total_space Общее доступное пространство
 * @param used_space Используемое пространство
 * @param free_space Свободное пространство
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_partition_info(uint32_t *total_space, uint32_t *used_space, uint32_t *free_space);

/**
 * @brief Включение OTA обновлений через мобильное приложение
 * @param enable Включить/выключить OTA из мобильного приложения
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_mobile_trigger(bool enable);

/**
 * @brief Получение логов процесса OTA обновления
 * @param log_buffer Буфер для логов (минимум 1024 байта)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int ota_update_get_logs(char *log_buffer, size_t max_length);

/**
 * @brief Включение диагностического режима OTA
 * @param enable Включить/выключить диагностический режим
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_diagnostic_mode(bool enable);

/**
 * @brief Получение диагностической информации об OTA
 * @param diagnostic_info Буфер для диагностической информации (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_diagnostic_info(char *diagnostic_info, size_t max_length);

/**
 * @brief Тестирование OTA обновления без фактической установки
 * @param update_info Информация об обновлении
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_test_update(const ota_update_info_t *update_info);

/**
 * @brief Получение оценки времени обновления
 * @param file_size Размер файла обновления
 * @param estimated_time Предполагаемое время обновления (секунды)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_estimate_update_time(uint32_t file_size, uint32_t *estimated_time);

/**
 * @brief Включение уведомлений об обновлениях в мобильное приложение
 * @param enable Включить/выключить уведомления
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_mobile_notifications(bool enable);

/**
 * @brief Получение истории OTA обновлений
 * @param history Массив для истории обновлений
 * @param max_entries Максимальное количество записей
 * @return Количество записей в истории
 */
int ota_update_get_history(ota_update_info_t *history, int max_entries);

/**
 * @brief Очистка истории OTA обновлений
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_clear_history(void);

/**
 * @brief Получение версии OTA компонента
 * @return Версия компонента в формате строки
 */
const char *ota_update_get_version(void);

/**
 * @brief Включение поддержки больших обновлений с использованием PSRAM
 * @param enable Включить/выключить поддержку больших обновлений
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_large_update_support(bool enable);

/**
 * @brief Получение рекомендаций по настройке OTA для ESP32S3
 * @param recommendations Буфер для рекомендаций (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_esp32s3_recommendations(char *recommendations, size_t max_length);

/**
 * @brief Включение параллельной загрузки обновления на двух ядрах ESP32S3
 * @param enable Включить/выключить параллельную загрузку
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_dual_core_download(bool enable);

/**
 * @brief Получение оценки энергопотребления во время OTA обновления
 * @param estimated_consumption_ma Предполагаемое потребление (mA)
 * @param estimated_duration_sec Предполагаемая длительность (секунды)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_power_consumption_estimate(uint32_t *estimated_consumption_ma, uint32_t *estimated_duration_sec);

/**
 * @brief Включение автоматического паузирования системы во время обновления
 * @param enable Включить/выключить паузирование системы
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_system_pause(bool enable);

/**
 * @brief Получение оценки влияния обновления на производительность системы
 * @param performance_impact_percent Влияние на производительность (%)
 * @param memory_usage_increase_mb Увеличение использования памяти (MB)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_performance_impact(float *performance_impact_percent, float *memory_usage_increase_mb);

/**
 * @brief Включение интеграции с системой мониторинга производительности
 * @param enable Включить/выключить интеграцию с мониторингом
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_performance_monitoring_integration(bool enable);

/**
 * @brief Получение комплексного отчета об OTA обновлении
 * @param report Буфер для отчета (минимум 1024 байта)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int ota_update_get_comprehensive_report(char *report, size_t max_length);

/**
 * @brief Включение предиктивного планирования OTA обновлений
 * @param enable Включить/выключить предиктивное планирование
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_predictive_scheduling(bool enable);

/**
 * @brief Получение прогноза наилучшего времени для OTA обновления
 * @param recommended_time Рекомендуемое время для обновления (timestamp)
 * @param reasoning Обоснование рекомендации (буфер минимум 128 байт)
 * @param max_reasoning_length Максимальная длина буфера обоснования
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_optimal_update_time(uint32_t *recommended_time, char *reasoning, size_t max_reasoning_length);

/**
 * @brief Автоматическая оптимизация процесса OTA обновления для ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_auto_optimize_for_esp32s3(void);

/**
 * @brief Получение оценки качества OTA обновления
 * @param quality_score Оценка качества (0-100)
 * @param reliability_score Оценка надежности (0-100)
 * @param security_score Оценка безопасности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_update_quality_score(uint8_t *quality_score, uint8_t *reliability_score, uint8_t *security_score);

/**
 * @brief Включение мониторинга качества обновления
 * @param enable Включить/выключить мониторинг качества
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_quality_monitoring(bool enable);

/**
 * @brief Получение детальной диагностики неудачного обновления
 * @param failure_analysis Буфер для анализа неудачи (минимум 512 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_failure_analysis(char *failure_analysis, size_t max_length);

/**
 * @brief Автоматическое восстановление после неудачного обновления
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_auto_recover(void);

/**
 * @brief Получение комплексной оценки OTA системы для гидропонного мониторинга
 * @param overall_score Общая оценка (0-100)
 * @param reliability_score Оценка надежности (0-100)
 * @param performance_score Оценка производительности (0-100)
 * @param safety_score Оценка безопасности растений (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_hydroponics_ota_score(uint8_t *overall_score, uint8_t *reliability_score,
                                             uint8_t *performance_score, uint8_t *safety_score);

/**
 * @brief Включение гидропонной осведомленности для OTA обновлений
 * @param enable Включить/выключить гидропонную осведомленность
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_hydroponics_awareness(bool enable);

/**
 * @brief Получение оценки готовности к OTA обновлению
 * @param readiness_score Оценка готовности (0-100)
 * @param blocking_issues Массив блокирующих проблем (минимум 256 байт)
 * @param max_issues_length Максимальная длина буфера проблем
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_readiness_assessment(uint8_t *readiness_score, char *blocking_issues, size_t max_issues_length);

/**
 * @brief Включение мониторинга в реальном времени во время OTA обновления
 * @param enable Включить/выключить мониторинг в реальном времени
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_realtime_monitoring(bool enable);

/**
 * @brief Получение данных мониторинга в реальном времени для мобильного приложения
 * @param realtime_data Буфер для данных реального времени (минимум 256 байт)
 * @param buffer_size Размер буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int ota_update_get_realtime_data(char *realtime_data, size_t buffer_size);

/**
 * @brief Автоматическая настройка OTA параметров для оптимальной производительности ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_auto_configure_for_esp32s3(void);

/**
 * @brief Получение финальной оценки OTA системы
 * @param final_score Финальная оценка (0-100)
 * @param final_grade Финальная буквенная оценка (A-F)
 * @param final_summary Финальное резюме (буфер минимум 512 байт)
 * @param max_summary_length Максимальная длина буфера резюме
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_final_assessment(uint8_t *final_score, char *final_grade,
                                        char *final_summary, size_t max_summary_length);

/**
 * @brief Включение мониторинга в режиме максимальной детализации для профессиональной диагностики
 * @param enable Включить детальный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_professional_monitoring(bool enable);

/**
 * @brief Получение профессионального отчета о процессе OTA обновления
 * @param professional_report Буфер для профессионального отчета (минимум 2048 байт)
 * @param max_length Максимальная длина буфера
 * @return Количество записанных байт или -1 в случае ошибки
 */
int ota_update_get_professional_report(char *professional_report, size_t max_length);

/**
 * @brief Автоматическая оптимизация OTA обновления для достижения максимальной эффективности ESP32S3
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_maximize_esp32s3_efficiency(void);

/**
 * @brief Получение оценки использования полного потенциала ESP32S3 в OTA обновлениях
 * @param dual_core_usage Использование двухъядерности (%)
 * @param psram_usage Использование PSRAM (%)
 * @param crypto_usage Использование криптоакселератора (%)
 * @param usb_usage Использование USB (%)
 * @param ai_usage Использование AI акселератора (%)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_esp32s3_feature_usage(float *dual_core_usage, float *psram_usage,
                                             float *crypto_usage, float *usb_usage, float *ai_usage);

/**
 * @brief Рекомендация полной оптимизации OTA обновления под ESP32S3
 * @param optimization_plan Буфер для плана оптимизации (минимум 1024 байт)
 * @param max_length Максимальная длина буфера
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_comprehensive_optimization_plan(char *optimization_plan, size_t max_length);

/**
 * @brief Включение мониторинга в режиме минимального потребления ресурсов для OTA
 * @param enable Включить ультра-легковесный мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_ultra_lightweight_monitoring(bool enable);

/**
 * @brief Получение финальной оценки производительности OTA обновления для гидропонного мониторинга
 * @param final_performance_score Финальная оценка производительности (0-100)
 * @param final_grade Финальная буквенная оценка (A-F)
 * @param final_summary Финальное резюме (буфер минимум 512 байт)
 * @param max_summary_length Максимальная длина буфера резюме
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_final_performance_assessment(uint8_t *final_performance_score,
                                                    char *final_grade,
                                                    char *final_summary,
                                                    size_t max_summary_length);

/**
 * @brief Включение мониторинга в режиме реального времени для критически важных метрик OTA
 * @param enable Включить критический мониторинг
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_critical_monitoring(bool enable);

/**
 * @brief Получение оценки надежности OTA обновления для длительной работы
 * @param reliability_score Оценка надежности (0-100)
 * @param mean_time_between_failures MTBF в часах
 * @param availability_score Оценка доступности (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_reliability_metrics(uint8_t *reliability_score, uint32_t *mean_time_between_failures,
                                           uint8_t *availability_score);

/**
 * @brief Автоматическая оптимизация надежности OTA обновления
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_optimize_reliability(void);

/**
 * @brief Получение оценки готовности OTA системы к мобильному приложению
 * @param mobile_readiness_score Оценка готовности к мобильному приложению (0-100)
 * @param api_compatibility_score Оценка совместимости API (0-100)
 * @param connectivity_score Оценка качества соединения (0-100)
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_mobile_readiness(uint8_t *mobile_readiness_score, uint8_t *api_compatibility_score,
                                        uint8_t *connectivity_score);

/**
 * @brief Включение мониторинга в режиме максимальной детализации для финальной оценки OTA
 * @param enable Включить детальный мониторинг для оценки
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_enable_final_assessment_monitoring(bool enable);

/**
 * @brief Получение окончательной оценки OTA обновления для гидропонного мониторинга на ESP32S3
 * @param final_performance_score Финальная оценка производительности (0-100)
 * @param final_grade Финальная буквенная оценка (A-F)
 * @param final_summary Финальное резюме (буфер минимум 512 байт)
 * @param max_summary_length Максимальная длина буфера резюме
 * @return ESP_OK в случае успеха
 */
esp_err_t ota_update_get_final_performance_assessment(uint8_t *final_performance_score,
                                                    char *final_grade,
                                                    char *final_summary,
                                                    size_t max_summary_length);
