#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "sensor_task.h"
#include "config.h"

static const char *TAG = "sensor_task";

/* ---------------------------------------------------------------------
 * Sensor read stubs.
 *
 * Replace the bodies of these two functions with real driver calls
 * (e.g. a DHT22 bit-banging library, or an I2C BMP280 driver such as
 * esp-idf-lib's bmp280 component). The task/queue/fault-handling
 * structure around them does not need to change.
 * --------------------------------------------------------------------- */
static bool read_dht22(float *temperature_c, float *humidity_pct)
{
    // TODO: replace with real DHT22 driver read.
    // Returning simulated but plausible values so the rest of the
    // pipeline (queue -> MQTT -> monitor) can be exercised end-to-end
    // before hardware drivers are wired in.
    *temperature_c = 24.5f + ((esp_random() % 100) / 100.0f);
    *humidity_pct  = 45.0f + ((esp_random() % 200) / 100.0f);
    return true;
}

static bool read_bmp280(float *pressure_hpa)
{
    // TODO: replace with real BMP280 I2C driver read.
    *pressure_hpa = 1012.0f + ((esp_random() % 300) / 100.0f);
    return true;
}

/* status_update() mutator callbacks -- kept as small named functions
 * since plain C (unlike Objective-C/Clang blocks) has no closure
 * literals. Each one runs with status_mutex already held. */
static void mark_queue_drop(system_status_t *s)
{
    s->queue_drop_count++;
}

static void mark_sensor_read_ok(system_status_t *s)
{
    s->last_sensor_read_us = esp_timer_get_time();
}

static void mark_sensor_fault(system_status_t *s)
{
    s->sensor_fault_count++;
}

void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "sensor_task started (period=%d ms)", SENSOR_SAMPLE_PERIOD_MS);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period_ticks = pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS);
    uint32_t consecutive_failures = 0;

    for (;;) {
        sensor_reading_t reading = {0};
        bool ok = true;

        ok &= read_dht22(&reading.temperature_c, &reading.humidity_pct);
        ok &= read_bmp280(&reading.pressure_hpa);
        reading.timestamp_us = esp_timer_get_time();

        if (ok) {
            consecutive_failures = 0;

            /* Non-blocking send: if the MQTT task has stalled and the
             * queue is full, drop the oldest sample rather than block
             * this high-priority task indefinitely. */
            if (xQueueSend(sensor_queue, &reading, 0) != pdTRUE) {
                sensor_reading_t discard;
                xQueueReceive(sensor_queue, &discard, 0);
                xQueueSend(sensor_queue, &reading, 0);
                status_update(mark_queue_drop);
            }

            status_update(mark_sensor_read_ok);

            ESP_LOGD(TAG, "T=%.2fC H=%.2f%% P=%.2fhPa",
                     reading.temperature_c, reading.humidity_pct, reading.pressure_hpa);
        } else {
            consecutive_failures++;
            status_update(mark_sensor_fault);

            if (consecutive_failures >= SENSOR_FAULT_THRESHOLD) {
                ESP_LOGE(TAG, "%u consecutive sensor read failures", consecutive_failures);
            }
        }

        vTaskDelayUntil(&last_wake_time, period_ticks);
    }
}
