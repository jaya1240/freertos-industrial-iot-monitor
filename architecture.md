# Architecture

## Tasks

| Task                  | Priority | Stack (words) | Period      | Responsibility                                   |
|------------------------|----------|----------------|-------------|---------------------------------------------------|
| `sensor_task`           | 3 (high) | 4096           | 2 s         | Read DHT22 + BMP280, push `sensor_reading_t` to queue |
| `mqtt_task`             | 2        | 6144           | event-driven| Maintain Wi-Fi/MQTT connection, publish readings   |
| `system_monitor_task`   | 1 (low)  | 3072           | 5 s         | Track heap/stack watermarks, fault counters, LED   |

Higher-priority `sensor_task` guarantees timely sampling even if the MQTT
task blocks momentarily on network I/O. `system_monitor_task` runs at the
lowest priority since health checks are non-urgent housekeeping.

## Data Flow

1. `sensor_task` samples the sensors every `SENSOR_SAMPLE_PERIOD_MS` and
   builds a `sensor_reading_t` (temperature, humidity, pressure, timestamp).
2. The reading is pushed onto `sensor_queue` (a `QueueHandle_t`, depth 10)
   with `xQueueSend()`. If the queue is full (MQTT task stalled), the oldest
   reading is dropped and a fault counter is incremented — this keeps the
   sensor task from ever blocking indefinitely.
3. `mqtt_task` blocks on `xQueueReceive()`, serializes the reading to JSON,
   and publishes it to `MQTT_TOPIC_TELEMETRY`. It also owns the Wi-Fi/MQTT
   connection state machine and reconnects automatically on drop.
4. Both tasks update a shared `system_status_t` struct (last sensor read
   time, last publish time, fault counters) protected by
   `status_mutex` (a `SemaphoreHandle_t`).
5. `system_monitor_task` periodically takes `status_mutex`, reads the
   status snapshot, logs a summary, checks `uxTaskGetStackHighWaterMark()`
   for each task, and drives the status LED:
   - Solid on: all systems nominal
   - Slow blink: Wi-Fi/MQTT disconnected
   - Fast blink: sensor read failures exceeding threshold

## Synchronization Primitives

- `sensor_queue` — `QueueHandle_t`, decouples sampling rate from publish
  rate and network latency.
- `status_mutex` — `SemaphoreHandle_t` (binary, used as mutex), guards the
  shared `system_status_t` struct against concurrent read/write from all
  three tasks.
- `wifi_event_group` — `EventGroupHandle_t`, signals `mqtt_task` when Wi-Fi
  connects/disconnects (standard ESP-IDF pattern).

## Fault Handling

- Sensor read failures increment `status.sensor_fault_count`; after 5
  consecutive failures the monitor task logs an error and speeds up the
  status LED blink.
- MQTT publish failures/disconnects increment `status.mqtt_fault_count` and
  trigger the built-in ESP-IDF MQTT client reconnect backoff.
- A queue-full condition (consumer too slow) increments
  `status.queue_drop_count` rather than blocking the producer.

## Extending

- Add more sensors by extending `sensor_reading_t` and the read logic in
  `sensor_task.c` — the queue and JSON serialization already generalize.
- Add command/control by subscribing `mqtt_task` to a command topic and
  exposing a second queue for downstream actuation tasks.
- Swap MQTT broker/topics/credentials without touching task logic — all in
  `config.h`.
