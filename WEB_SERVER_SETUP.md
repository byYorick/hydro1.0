# 🌐 ЗАПУСК WEB-СЕРВЕРА ДЛЯ ПРОСМОТРА ПАРАМЕТРОВ

**Дата:** 16 октября 2025  
**Статус:** ⚠️ Требуется инициализация

---

## ✅ ЧТО УЖЕ ЕСТЬ

### 1. HTTP сервер с REST API ✅

**Компонент:** `mobile_app_interface`

**Endpoints (порт 8080):**
```
GET  /api/sensor_data    - Данные всех датчиков (JSON)
GET  /api/system_status  - Статус системы
GET  /api/settings       - Текущие настройки
POST /api/settings       - Обновить настройки
GET  /api/history        - История данных
POST /api/control        - Управляющие команды
POST /api/auth           - Аутентификация
GET  /api/device_info    - Информация об устройстве
```

### 2. WiFi Manager ✅

**Компонент:** `network_manager`

**Режимы:**
- **STA** - подключение к существующей WiFi сети
- **AP** - собственная точка доступа
- **HYBRID** - оба режима одновременно

---

## 🚀 БЫСТРЫЙ СТАРТ

### Вариант 1: Точка доступа (AP Mode)

**Самый простой!** ESP32 создаст свою WiFi сеть.

#### Шаг 1: Добавить код в `app_main.c`

Добавь после инициализации `ph_ec_controller`:

```c
// ========== ЭТАП 8: Инициализация Web-сервера ==========
ESP_LOGI(TAG, "[8/8] Starting Web Server...");

// Инициализация network_manager в режиме точки доступа
ret = network_manager_init(NETWORK_MODE_AP);
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to initialize network manager: %s", esp_err_to_name(ret));
} else {
    ESP_LOGI(TAG, "  [OK] Network Manager initialized");
    
    // Конфигурация точки доступа
    ap_config_t ap_config = {
        .ssid = "HydroSystem",
        .password = "hydro12345",
        .channel = 1,
        .max_connection = 4,
        .ssid_hidden = false
    };
    
    ret = network_manager_start_ap(&ap_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] WiFi AP started: %s", ap_config.ssid);
        ESP_LOGI(TAG, "  [OK] Password: %s", ap_config.password);
        ESP_LOGI(TAG, "  [OK] IP: 192.168.4.1");
        
        // Запуск HTTP сервера
        ret = network_manager_start_http_server(8080);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  [OK] HTTP Server started on port 8080");
            ESP_LOGI(TAG, "  [OK] Web interface: http://192.168.4.1:8080");
            
            // Инициализация mobile_app_interface для REST API
            ret = mobile_app_interface_init(NETWORK_MODE_AP);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  [OK] REST API initialized");
                ESP_LOGI(TAG, "      → GET  http://192.168.4.1:8080/api/sensor_data");
                ESP_LOGI(TAG, "      → GET  http://192.168.4.1:8080/api/system_status");
                ESP_LOGI(TAG, "      → GET  http://192.168.4.1:8080/api/device_info");
            }
        }
    }
}
```

#### Шаг 2: Обновить `main/CMakeLists.txt`

Добавь в PRIV_REQUIRES:
```cmake
network_manager
mobile_app_interface
```

#### Шаг 3: Прошить и подключиться

1. Собери и прошей проект
2. ESP32 создаст WiFi: **HydroSystem**
3. Подключись с телефона/ноутбука (пароль: **hydro12345**)
4. Открой браузер: **http://192.168.4.1:8080/api/sensor_data**

---

### Вариант 2: Подключение к существующей WiFi

ESP32 подключится к твоей домашней WiFi.

```c
// Конфигурация WiFi клиента
network_wifi_config_t wifi_cfg = {
    .ssid = "YOUR_WIFI_NAME",
    .password = "YOUR_WIFI_PASSWORD",
    .channel = 0,  // auto
    .auto_reconnect = true
};

network_manager_init(NETWORK_MODE_STA);
network_manager_connect_wifi(&wifi_cfg);
network_manager_start_http_server(8080);
mobile_app_interface_init(NETWORK_MODE_STA);

// Узнать IP адрес:
char ip_str[16];
network_manager_get_ip(ip_str);
ESP_LOGI(TAG, "IP address: %s", ip_str);
ESP_LOGI(TAG, "Open: http://%s:8080/api/sensor_data", ip_str);
```

---

## 📱 ПРИМЕРЫ ЗАПРОСОВ

### 1. Получить данные датчиков

```bash
curl http://192.168.4.1:8080/api/sensor_data
```

**Ответ:**
```json
{
  "pH": 7.05,
  "EC": 1.52,
  "temperature": 24.3,
  "humidity": 62.5,
  "lux": 15230,
  "co2": 450,
  "timestamp": 1697456789
}
```

### 2. Статус системы

```bash
curl http://192.168.4.1:8080/api/system_status
```

**Ответ:**
```json
{
  "uptime": 3600,
  "free_heap": 150000,
  "min_heap": 120000,
  "auto_control": true,
  "pumps_active": 0
}
```

### 3. Информация об устройстве

```bash
curl http://192.168.4.1:8080/api/device_info
```

**Ответ:**
```json
{
  "device_name": "Hydro Monitor",
  "version": "3.0.0-advanced",
  "chip": "ESP32-S3",
  "mac": "AA:BB:CC:DD:EE:FF",
  "ip": "192.168.4.1",
  "http_port": 8080
}
```

---

## 🎨 ПРОСТОЙ WEB-ИНТЕРФЕЙС

Создам HTML страницу для просмотра данных в браузере!

**Файл:** `web_interface.html` (сохрани локально и открой)

```html
<!DOCTYPE html>
<html>
<head>
    <title>Hydro System - Dashboard</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background: #f0f0f0;
        }
        h1 { color: #2c3e50; }
        .card {
            background: white;
            border-radius: 8px;
            padding: 20px;
            margin: 10px 0;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .sensor {
            display: inline-block;
            margin: 10px;
            padding: 15px 25px;
            background: #3498db;
            color: white;
            border-radius: 5px;
        }
        .value { font-size: 24px; font-weight: bold; }
        .label { font-size: 14px; opacity: 0.9; }
        .status { color: #27ae60; font-weight: bold; }
        .error { color: #e74c3c; }
        button {
            padding: 10px 20px;
            margin: 5px;
            background: #3498db;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        button:hover { background: #2980b9; }
    </style>
</head>
<body>
    <h1>🌱 Hydro System Dashboard</h1>
    
    <div class="card">
        <h2>Статус подключения</h2>
        <p id="connection-status" class="status">Проверка...</p>
        <button onclick="checkConnection()">Проверить</button>
        <button onclick="refreshData()">Обновить данные</button>
    </div>
    
    <div class="card">
        <h2>Датчики</h2>
        <div id="sensors">Загрузка...</div>
    </div>
    
    <div class="card">
        <h2>Статус системы</h2>
        <div id="system-status">Загрузка...</div>
    </div>
    
    <div class="card">
        <h2>Информация об устройстве</h2>
        <div id="device-info">Загрузка...</div>
    </div>

    <script>
        // Измени на IP твоего ESP32
        const ESP_IP = '192.168.4.1';  // Для AP режима
        // const ESP_IP = '192.168.1.100';  // Для STA режима (узнай IP из логов)
        
        const API_BASE = `http://${ESP_IP}:8080/api`;
        
        async function checkConnection() {
            const status = document.getElementById('connection-status');
            try {
                const response = await fetch(`${API_BASE}/device_info`);
                if (response.ok) {
                    status.textContent = '✅ Подключено к ESP32!';
                    status.className = 'status';
                    loadAllData();
                } else {
                    status.textContent = '❌ Ошибка подключения';
                    status.className = 'error';
                }
            } catch (error) {
                status.textContent = '❌ ESP32 недоступен. Проверь IP адрес!';
                status.className = 'error';
            }
        }
        
        async function loadSensorData() {
            try {
                const response = await fetch(`${API_BASE}/sensor_data`);
                const data = await response.json();
                
                document.getElementById('sensors').innerHTML = `
                    <div class="sensor">
                        <div class="label">pH</div>
                        <div class="value">${data.pH.toFixed(2)}</div>
                    </div>
                    <div class="sensor">
                        <div class="label">EC (mS/cm)</div>
                        <div class="value">${data.EC.toFixed(2)}</div>
                    </div>
                    <div class="sensor" style="background: #e67e22;">
                        <div class="label">Температура (°C)</div>
                        <div class="value">${data.temperature.toFixed(1)}</div>
                    </div>
                    <div class="sensor" style="background: #9b59b6;">
                        <div class="label">Влажность (%)</div>
                        <div class="value">${data.humidity.toFixed(1)}</div>
                    </div>
                    <div class="sensor" style="background: #f39c12;">
                        <div class="label">Освещённость (Lux)</div>
                        <div class="value">${Math.round(data.lux)}</div>
                    </div>
                    <div class="sensor" style="background: #16a085;">
                        <div class="label">CO2 (ppm)</div>
                        <div class="value">${Math.round(data.co2)}</div>
                    </div>
                `;
            } catch (error) {
                document.getElementById('sensors').innerHTML = '<p class="error">Ошибка загрузки данных</p>';
            }
        }
        
        async function loadSystemStatus() {
            try {
                const response = await fetch(`${API_BASE}/system_status`);
                const data = await response.json();
                
                document.getElementById('system-status').innerHTML = `
                    <p><strong>Время работы:</strong> ${Math.floor(data.uptime / 60)} минут</p>
                    <p><strong>Свободная память:</strong> ${Math.floor(data.free_heap / 1024)} KB</p>
                    <p><strong>Автоконтроль:</strong> ${data.auto_control ? '✅ Включен' : '❌ Выключен'}</p>
                    <p><strong>Активных насосов:</strong> ${data.pumps_active}</p>
                `;
            } catch (error) {
                document.getElementById('system-status').innerHTML = '<p class="error">Ошибка загрузки</p>';
            }
        }
        
        async function loadDeviceInfo() {
            try {
                const response = await fetch(`${API_BASE}/device_info`);
                const data = await response.json();
                
                document.getElementById('device-info').innerHTML = `
                    <p><strong>Устройство:</strong> ${data.device_name}</p>
                    <p><strong>Версия:</strong> ${data.version}</p>
                    <p><strong>Чип:</strong> ${data.chip}</p>
                    <p><strong>MAC:</strong> ${data.mac}</p>
                    <p><strong>IP:</strong> ${data.ip}</p>
                `;
            } catch (error) {
                document.getElementById('device-info').innerHTML = '<p class="error">Ошибка загрузки</p>';
            }
        }
        
        function loadAllData() {
            loadSensorData();
            loadSystemStatus();
            loadDeviceInfo();
        }
        
        function refreshData() {
            loadAllData();
        }
        
        // Автообновление каждые 5 секунд
        setInterval(loadSensorData, 5000);
        setInterval(loadSystemStatus, 10000);
        
        // Загрузить при старте
        checkConnection();
    </script>
</body>
</html>
```

---

## 🔧 КОД ДЛЯ ИНИЦИАЛИЗАЦИИ

### Добавь в `main/app_main.c`:

**После строки 657 (после `ph_ec_controller_init()`):**

```c
// ========== ЭТАП 8: Инициализация Web-интерфейса ==========
ESP_LOGI(TAG, "[8/8] Initializing Web Server...");

// Инициализация network_manager
ret = network_manager_init(NETWORK_MODE_AP);
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "  [WARN] Failed to initialize network manager: %s", esp_err_to_name(ret));
} else {
    ESP_LOGI(TAG, "  [OK] Network Manager initialized");
    
    // Настройка точки доступа
    ap_config_t ap_cfg = {
        .ssid = "HydroSystem",
        .password = "hydro12345",
        .channel = 1,
        .max_connection = 4,
        .ssid_hidden = false
    };
    
    ret = network_manager_start_ap(&ap_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] WiFi AP started");
        ESP_LOGI(TAG, "      SSID: %s", ap_cfg.ssid);
        ESP_LOGI(TAG, "      Password: %s", ap_cfg.password);
        ESP_LOGI(TAG, "      IP: 192.168.4.1");
        
        // Запуск HTTP сервера на порту 8080
        ret = network_manager_start_http_server(8080);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  [OK] HTTP Server started on port 8080");
            
            // Инициализация REST API
            ret = mobile_app_interface_init(NETWORK_MODE_AP);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  [OK] REST API ready");
                ESP_LOGI(TAG, "=================================================");
                ESP_LOGI(TAG, "   Web Interface: http://192.168.4.1:8080");
                ESP_LOGI(TAG, "   API Endpoint: http://192.168.4.1:8080/api/sensor_data");
                ESP_LOGI(TAG, "=================================================");
            }
        }
    }
}
```

### Обновить includes в `main/app_main.c`:

```c
// Добавь после других includes:
#include "network_manager.h"
#include "mobile_app_interface.h"
```

### Обновить `main/CMakeLists.txt`:

```cmake
PRIV_REQUIRES
    ...
    network_manager
    mobile_app_interface
    ...
```

---

## 📱 КАК ИСПОЛЬЗОВАТЬ

### После прошивки:

1. **На телефоне/ноутбуке:**
   - Открой WiFi настройки
   - Найди сеть **HydroSystem**
   - Подключись (пароль: **hydro12345**)

2. **Откр ой браузер:**
   - **Главная:** http://192.168.4.1:8080
   - **Датчики:** http://192.168.4.1:8080/api/sensor_data
   - **Статус:** http://192.168.4.1:8080/api/system_status

3. **Или используй HTML интерфейс:**
   - Сохрани HTML код выше в файл `hydro_dashboard.html`
   - Открой в браузере
   - Нажми "Проверить"
   - Данные будут обновляться каждые 5 секунд

---

## 🛠️ ДОПОЛНИТЕЛЬНЫЕ ВОЗМОЖНОСТИ

### 1. Управление насосами через API

```bash
curl -X POST http://192.168.4.1:8080/api/control \
  -H "Content-Type: application/json" \
  -d '{"command":"start_pump","param1":"0","param2":"5000"}'
```

### 2. Изменение настроек

```bash
curl -X POST http://192.168.4.1:8080/api/settings \
  -H "Content-Type: application/json" \
  -d '{"auto_control":true,"target_ph":6.5}'
```

### 3. История данных

```bash
curl http://192.168.4.1:8080/api/history
```

---

## ⚠️ ВАЖНО

### Настройки по умолчанию:

- **SSID:** HydroSystem
- **Пароль:** hydro12345
- **IP:** 192.168.4.1
- **Порт:** 8080

### Безопасность:

⚠️ Смени пароль WiFi на более сложный в production!

```c
.password = "YourStrongPassword123!",
```

---

## 📋 ЧЕКЛИСТ

- [ ] Добавить includes в app_main.c
- [ ] Добавить код инициализации
- [ ] Обновить CMakeLists.txt
- [ ] Собрать проект
- [ ] Прошить
- [ ] Подключиться к WiFi HydroSystem
- [ ] Открыть http://192.168.4.1:8080/api/sensor_data
- [ ] Проверить данные

---

**Web-сервер готов к запуску!** 🚀

Хочешь, чтобы я добавил код прямо сейчас?


