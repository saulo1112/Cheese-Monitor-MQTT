# Práctica 2 — Broker MQTT para IoT y visualización de datos

**Estudiantes:**
- Miguel Angel Franco Restrepo (22506163)
- Saulo Quiñones Góngora (22506635)
- Adrian Felipe Vargas Rojas (22505561)

**Curso:** Inteligencia Artificial aplicada a Internet de las Cosas

**Institución:** Universidad Autónoma de Occidente

**Periodo:** 2026-1S

---

## 1. Resumen del proyecto

Este proyecto implementa un sistema de monitoreo de temperatura y humedad para una cámara de maduración de quesos, utilizando un ESP32 con sensor DHT22 como dispositivo IoT. A diferencia de la práctica anterior (que usaba HTTP y MySQL), esta práctica utiliza el protocolo MQTT para la transmisión de datos, con una arquitectura completamente desplegada en AWS que incluye un broker Mosquitto, una base de datos de series de tiempo InfluxDB, el agente Telegraf como puente entre MQTT e InfluxDB, y Grafana para visualización y alertas.

El flujo de datos es el siguiente:

```
ESP32 + DHT22 → WiFi → Mosquitto (MQTT) → Telegraf → InfluxDB → Grafana
```

---

## 2. Caso de uso

El sistema monitorea las condiciones ambientales de una cámara de maduración de quesos, donde el control de temperatura y humedad es crítico para garantizar la calidad del producto. El sensor DHT22 mide las condiciones del ambiente cada 5 segundos y transmite los datos al broker MQTT en AWS, que los almacena en InfluxDB y los visualiza en tiempo real a través de Grafana con alertas automáticas por correo electrónico.

---

## 3. Arquitectura

### 3.1 Diagrama general

```
ESP32 (DHT22)
     |
     | WiFi → MQTT (puerto 1883)
     ↓
Internet Gateway
     |
     ↓
VPC iot-mqtt-VPC (192.168.0.0/16)
     |
     ├── Subred pública — 192.168.1.0/24
     │        EC2 Ubuntu t3.micro
     │        IP elástica: 32.196.10.168
     │        ├── Broker Mosquitto (puerto 1883)
     │        ├── Telegraf (agente puente MQTT → InfluxDB)
     │        ├── InfluxDB (puerto 8086)
     │        └── Grafana (puerto 3000)
     │
     └── Subred privada — 192.168.2.0/24
              EC2 Ubuntu (instancia adicional)
              Sin acceso directo a internet
              Acceso mediante NAT Gateway
```

### 3.2 Componentes de infraestructura AWS

| Componente | Detalle |
|---|---|
| VPC | `iot-mqtt-VPC` — CIDR `192.168.0.0/16` |
| Subred pública | `Public-Subnet-A` — `192.168.1.0/24` — AZ `us-east-1a` |
| Subred privada | `Private-Subnet-B` — `192.168.2.0/24` — AZ `us-east-1a` |
| Internet Gateway | `iot-igw` — adjunto a `iot-mqtt-VPC` |
| NAT Gateway | `ngw-A` — permite salida a internet desde la subred privada |
| Route Table pública | `rt-igw-public` — `0.0.0.0/0 → iot-igw` |
| Route Table privada | `rt-privada` — `0.0.0.0/0 → ngw-A` |
| EC2 pública | Ubuntu Server — `t3.micro` — IP elástica `32.196.10.168` |
| EC2 privada | Ubuntu Server — `t3.micro` — IP privada `192.168.2.50` |

### 3.3 Security Groups

#### broker-mqtt-SG (EC2 pública)

| Tipo | Puerto | Origen | Propósito |
|---|---|---|---|
| SSH | 22 | 0.0.0.0/0 | Acceso administrativo |
| TCP personalizado | 1883 | 0.0.0.0/0 | Broker MQTT — recepción de datos del ESP32 |
| TCP personalizado | 8086 | 0.0.0.0/0 | InfluxDB API |
| TCP personalizado | 3000 | 0.0.0.0/0 | Grafana dashboard |

---

## 4. Hardware y firmware

### 4.1 Componentes

| Componente | Detalle |
|---|---|
| Microcontrolador | ESP32 Dev Module |
| Sensor | DHT22 (temperatura y humedad) |
| Conexión | WiFi 2.4 GHz |
| Pin de datos | GPIO 15 |
| LED indicador | GPIO 2 (se activa cuando temperatura ≥ 33°C) |

### 4.2 Librerías Arduino utilizadas

| Librería | Autor | Propósito |
|---|---|---|
| `DHT sensor library` | Adafruit | Lectura del sensor DHT22 |
| `PubSubClient` | Nick O'Leary | Comunicación MQTT |
| `ArduinoJson` | Benoit Blanchon | Serialización de datos en JSON |
| `WiFi` | Espressif | Conectividad WiFi |

### 4.3 Estructura del firmware

```
ESP32_CODE/
├── ESP32_CODE.ino    ← Sketch principal
└── credentials.h    ← Credenciales WiFi y broker MQTT
```

### 4.4 credentials.h

```cpp
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

const char* ssid         = "RED_WIFI";
const char* password     = "PASSWORD_WIFI";
const char* mqtt_user    = "usuario_mqtt";
const char* mqtt_password = "password_mqtt";

#endif
```

### 4.5 Formato del mensaje MQTT

El ESP32 publica cada 5 segundos en el topic `esp32/sensor` un payload JSON:

```json
{
  "temperature": 29.1,
  "humidity": 74.2
}
```

### 4.6 Parámetros de conexión MQTT

| Parámetro | Valor |
|---|---|
| Broker IP | `32.196.10.168` (IP elástica de la EC2) |
| Puerto | `1883` |
| Topic | `esp32/sensor` |
| Autenticación | Usuario y contraseña habilitados |
| Client ID | `esp32Client` |

---

## 5. Broker Mosquitto

### 5.1 Instalación

```bash
sudo apt update && sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable mosquitto
```

### 5.2 Configuración de autenticación

Se creó un archivo de contraseñas con el usuario `saulo`:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd saulo
sudo chown mosquitto:mosquitto /etc/mosquitto/passwd
sudo chmod 600 /etc/mosquitto/passwd
```

### 5.3 Archivo mosquitto.conf

```
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
include_dir /etc/mosquitto/conf.d
allow_anonymous false
listener 1883
password_file /etc/mosquitto/passwd
```

### 5.4 Verificación del broker

Se probó el funcionamiento con dos terminales en la EC2:

```bash
# Terminal 1 — suscriptor
mosquitto_sub -h localhost -t "esp32/sensor" -u user -P password

# Terminal 2 — publicador
mosquitto_pub -h localhost -t "esp32/sensor" -m '{"temperature":29,"humidity":74}' -u user -P password
```

También se verificó con MQTT Explorer conectado a `ec2-32-196-10-168.compute-1.amazonaws.com` en el puerto 1883, confirmando recepción de mensajes del ESP32 en tiempo real.

---

## 6. Base de datos InfluxDB

### 6.1 Instalación

```bash
wget -q https://repos.influxdata.com/influxdata-archive_compat.key
echo '393e8779c89ac8d958f81f942f9ad7fb82a25e133faddaf92e15b16e6ac9ce4c influxdata-archive_compat.key' | sha256sum -c && \
  cat influxdata-archive_compat.key | gpg --dearmor | \
  sudo tee /etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg > /dev/null
echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg] https://repos.influxdata.com/debian stable main' | \
  sudo tee /etc/apt/sources.list.d/influxdata.list
sudo apt update && sudo apt install -y influxdb
sudo systemctl enable influxdb && sudo systemctl start influxdb
```

### 6.2 Verificación de datos

```bash
influx
> USE iot-sensors
> SHOW MEASUREMENTS
> SELECT * FROM mqtt_consumer ORDER BY time DESC LIMIT 10
```

La base de datos almacena los campos `temperature`, `humidity`, `host` y `topic` con timestamp en nanosegundos.

### 6.3 Exportación a CSV

```bash
influx -database 'iot-sensors' \
  -execute 'SELECT * FROM mqtt_consumer ORDER BY time ASC' \
  -format csv > ~/datos_$(date +%Y-%m-%d).csv
```

---

## 7. Telegraf — puente MQTT → InfluxDB

Telegraf actúa como agente que se suscribe al broker MQTT y escribe los datos directamente en InfluxDB sin necesidad de código personalizado.

### 7.1 Instalación

```bash
sudo apt install -y telegraf
sudo systemctl enable telegraf
```

### 7.2 Configuración /etc/telegraf/telegraf.conf

```toml
[agent]
  interval = "10s"
  flush_interval = "10s"

[[inputs.mqtt_consumer]]
  servers = ["tcp://192.168.1.102:1883"]
  topics = ["esp32/sensor"]
  username = "saulo"
  password = "iotact22"
  data_format = "json"
  data_type = "float"

[[outputs.influxdb]]
  urls = ["http://localhost:8086"]
  database = "iot-sensors"
```

---

## 8. Grafana — visualización y alertas

### 8.1 Instalación

```bash
sudo mkdir -p /etc/apt/keyrings/
wget -q -O - https://apt.grafana.com/gpg.key | gpg --dearmor | \
  sudo tee /etc/apt/keyrings/grafana.gpg > /dev/null
echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main" | \
  sudo tee /etc/apt/sources.list.d/grafana.list
sudo apt update && sudo apt install -y grafana
sudo systemctl enable grafana-server && sudo systemctl start grafana-server
```

### 8.2 Datasource

Se configuró InfluxDB como fuente de datos:
- URL: `http://localhost:8086`
- Database: `iot-sensors`

### 8.3 Dashboard

El dashboard incluye los siguientes paneles:

| Panel | Tipo | Query |
|---|---|---|
| Temperatura actual | Gauge | `SELECT last("temperature") FROM "mqtt_consumer"` |
| Humedad actual | Gauge | `SELECT last("humidity") FROM "mqtt_consumer"` |
| Promedio temperatura (1h) | Stat | `SELECT mean("temperature") FROM "mqtt_consumer" WHERE time > now() - 1h` |
| Promedio humedad (1h) | Stat | `SELECT mean("humidity") FROM "mqtt_consumer" WHERE time > now() - 1h` |
| Total registros | Stat | `SELECT count("temperature") FROM "mqtt_consumer"` |
| Temperatura en el tiempo | Time series | `SELECT mean("temperature") FROM "mqtt_consumer" WHERE $timeFilter GROUP BY time($__interval)` |
| Humedad en el tiempo | Time series | `SELECT mean("humidity") FROM "mqtt_consumer" WHERE $timeFilter GROUP BY time($__interval)` |
| Registros recientes | Table | `SELECT "temperature", "humidity" FROM "mqtt_consumer" WHERE $timeFilter ORDER BY time DESC LIMIT 50` |

### 8.4 Alertas

Se configuraron dos reglas de alerta en **Alerting → Alert rules**:

**Alerta de temperatura:**
- Condición: `last(temperature) IS ABOVE 33`
- Pending period: `1m`
- Summary: *Temperatura alta en la cámara de maduración*

**Alerta de humedad:**
- Condición: `last(humidity) IS BELOW 70`
- Pending period: `1m`
- Summary: *Humedad baja en la cámara de maduración — riesgo de resecamiento del queso*

### 8.5 Notificaciones por email

Se configuró SMTP en `/etc/grafana/grafana.ini`:

```ini
[server]
root_url = http://32.196.10.168:3000

[smtp]
enabled = true
host = smtp.gmail.com:587
user = correo@gmail.com
password = app_password_16_caracteres
from_address = correo@gmail.com
from_name = Monitor Cámara de Maduración
skip_verify = true
```

Las notificaciones se envían automáticamente al correo configurado cuando se dispara alguna alerta, con el siguiente formato:
- Asunto: `[FIRING:1] Alerta de temperatura/humedad Cámara de maduración`
- Cuerpo: incluye el valor que disparó la alerta, timestamp y enlace directo al dashboard

---

## 9. Comandos útiles

```bash
# Ver estado de todos los servicios
sudo systemctl status mosquitto influxdb telegraf grafana-server

# Reiniciar todos los servicios
sudo systemctl restart mosquitto influxdb telegraf grafana-server

# Ver logs en tiempo real de Telegraf
sudo journalctl -fu telegraf

# Ver logs de Mosquitto
sudo tail -f /var/log/mosquitto/mosquitto.log

# Consultar datos en InfluxDB
influx -database 'iot-sensors' -execute 'SELECT * FROM mqtt_consumer ORDER BY time DESC LIMIT 10'

# Exportar CSV
influx -database 'iot-sensors' \
  -execute 'SELECT * FROM mqtt_consumer ORDER BY time ASC' \
  -format csv > ~/datos_$(date +%Y-%m-%d).csv

# Probar broker desde la EC2
mosquitto_sub -h localhost -t "esp32/sensor" -u saulo -P password
```

---

## 10. Acceso al sistema

| Servicio | URL | Credenciales |
|---|---|---|
| Grafana | `http://32.196.10.168:3000` | admin / password |
| InfluxDB API | `http://32.196.10.168:8086` | sin auth |
| Broker MQTT | `32.196.10.168:1883` | user / password |

---

## 11. Lecciones aprendidas

- El flag `-c` en `mosquitto_passwd` crea el archivo desde cero, si se ejecuta dos veces borra el usuario anterior. Para agregar usuarios adicionales se omite `-c`.
- El error `status=13` en Mosquitto indica problema de permisos, se resuelve con `chown mosquitto:mosquitto` sobre el archivo passwd.
- Telegraf necesita un bloque `[agent]` mínimo en su configuración para arrancar correctamente.
- El `root_url` de Grafana debe configurarse sin el punto y coma (`;`) inicial, de lo contrario la línea queda comentada y los links de alertas apuntan a localhost.
- En AWS Academy las IP públicas de EC2 pueden cambiar entre sesiones de laboratorio, la IP elástica resuelve este problema para los links del ESP32 y Grafana.
- El NAT Gateway es necesario para que las instancias en subredes privadas puedan acceder a internet para instalar paquetes.
- Gmail requiere una App Password (contraseña de aplicación) de 16 caracteres para enviar emails desde aplicaciones externas, la contraseña normal es rechazada.
- El `Ctrl+W` en el navegador cierra la pestaña — dentro de nano el atajo de búsqueda es `Ctrl+\` y el deshacer es `Alt+U`.

---

## 12. Estructura del repositorio

```
Practica2-MQTT/
├── ESP32_CODE/
│   ├── ESP32_CODE.ino       ← Firmware principal del ESP32
│   └── credentials.h        ← Credenciales WiFi y MQTT (no subir a git)
├── grafana/
│   └── dashboard.json        ← Dashboard importable en Grafana
└── README.md                 ← Este archivo
```

---

## 13. Uso académico

Este proyecto fue desarrollado con fines educativos en el contexto de la Especialización en Inteligencia Artificial — curso de Inteligencia Artificial aplicada a Internet de las Cosas, Universidad Autónoma de Occidente, periodo 2026-1S.
