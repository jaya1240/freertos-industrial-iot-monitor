# FreeRTOS-Based Industrial IoT Monitoring System

A multi-tasking industrial monitoring firmware for the **ESP32**, built on
**FreeRTOS** (via ESP-IDF), that reads sensor data, publishes it over
**MQTT**, and watches system health — all running as independent,
priority-scheduled tasks communicating through queues and mutexes.

## Features

- **Multi-task architecture** — sensor acquisition, MQTT publishing, and
  system health monitoring run as separate FreeRTOS tasks with distinct
  priorities and stack sizes.
- **Sensor abstraction layer** — reads temperature, humidity, and pressure
  (DHT22 + BMP280 style interface); swap in real driver calls where marked.
- **MQTT telemetry** — publishes JSON-formatted sensor readings to a
  configurable broker/topic, with automatic Wi-Fi + MQTT reconnect handling.
- **Inter-task communication** — a `QueueHandle_t` moves sensor readings from
  the sensor task to the MQTT task; a mutex-protected shared struct exposes
  live system status to the monitor task.
- **System health monitoring** — tracks free heap, task watermarks, and
  sensor/MQTT fault counters; drives a status LED and logs warnings.
- **Configurable** — all pins, intervals, Wi-Fi/MQTT credentials, and task
  priorities live in one `config.h`.

## Architecture

```
                +----------------+
                |   Sensors      |
                | (DHT22, BMP280)|
                +-------+--------+
                        |
                 sensor_task (Prio 3)
                        |
                 xQueueSend(reading)
                        v
                +----------------+        +----------------------+
                | sensor_queue   |------->| mqtt_task (Prio 2)   |
                +----------------+        +----------+-----------+
                                                      |
                                            publish JSON via MQTT
                                                      v
                                              MQTT Broker (cloud/local)

                +------------------------------+
                | system_monitor_task (Prio 1) |
                |  - heap watermark             |
                |  - task stack watermarks      |
                |  - fault counters (mutex)     |
                |  - status LED                 |
                +------------------------------+
```

See [`docs/architecture.md`](docs/architecture.md) for task priorities, stack
sizes, and the full data-flow description.

## Hardware

| Component     | Interface     | Notes                                 |
|----------------|---------------|-----------------------------------------|
| ESP32 DevKit   | —             | Any ESP32 dev board                    |
| DHT22          | GPIO (1-wire) | Temperature + humidity                 |
| BMP280         | I2C           | Barometric pressure                    |
| Status LED     | GPIO          | Blinks/patterns indicate system state  |

Pin assignments are in `main/config.h`.

## Building (ESP-IDF)

```bash
# Requires ESP-IDF v5.x installed and sourced (export.sh / export.bat)
idf.py set-target esp32
idf.py menuconfig      # optional: adjust sdkconfig
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Before building, edit `main/config.h` with your:
- Wi-Fi SSID / password
- MQTT broker URI, port, topic
- Sensor GPIO/I2C pins

## Project Structure

```
freertos-industrial-iot-monitor/
├── CMakeLists.txt               # top-level ESP-IDF project file
├── sdkconfig.defaults           # sane default build config
├── main/
│   ├── CMakeLists.txt
│   ├── config.h                 # all pins, credentials, task params
│   ├── main.c                   # app_main: creates queue/mutex/tasks
│   ├── sensor_task.c/.h         # reads sensors, pushes to queue
│   ├── mqtt_task.c/.h           # Wi-Fi + MQTT, publishes readings
│   └── system_monitor_task.c/.h # health monitoring, status LED
├── docs/
│   └── architecture.md
├── .gitignore
└── LICENSE
```

## Publishing to GitHub

```bash
cd freertos-industrial-iot-monitor
git init
git add .
git commit -m "Initial commit: FreeRTOS industrial IoT monitoring system"
git branch -M main
git remote add origin https://github.com/<your-username>/<repo-name>.git
git push -u origin main
```

## License

MIT — see [`LICENSE`](LICENSE).
