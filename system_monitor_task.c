#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "system_monitor_task.h"
#include "sensor_task.h"
#include "config.h"

static const char *TAG = "monitor_task";

static TaskHandle_t s_sensor_handle = NULL;
static TaskHandle_t s_mqtt_handle = NULL;
static TaskHandle_t s_monitor_handle = NULL;

void system_monitor_register_tasks(TaskHandle_t sensor_handle,
                                    TaskHandle_t mqtt_handle,
                                    TaskHandle_t monitor_handle)
{
    s_sensor_handle = sensor_handle;
    s_mqtt_handle = mqtt_handle;
    s_monitor_handle = monitor_handle;
}

static void init_status_led(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}

/* Blink pattern conveys health at a glance without needing a serial console:
 *   - solid on:   nominal
 *   - slow blink: network (Wi-Fi/MQTT) down
 *   - fast blink: sensor faults past threshold
 */
static void update_status_led(bool wifi_ok, bool mqtt_ok, bool sensor_ok)
{
    static bool led_state = false;
    static int64_t last_toggle_us = 0;
    int64_t now = esp_timer_get_time();

    if (wifi_ok && mqtt_ok && sensor_ok) {
        gpio_set_level(STATUS_LED_GPIO, 1);
        return;
    }

    int64_t blink_period_us = sensor_ok ? 500000 : 150000; /* slow vs fast */
    if (now - last_toggle_us >= blink_period_us) {
        led_state = !led_state;
        gpio_set_level(STATUS_LED_GPIO, led_state);
        last_toggle_us = now;
    }
}

static void log_stack_watermark(const char *name, TaskHandle_t handle)
{
    if (handle == NULL) {
        return;
    }
    UBaseType_t words_free = uxTaskGetStackHighWaterMark(handle);
    if (words_free < 128) {
        ESP_LOGW(TAG, "%s stack watermark LOW: %u words free", name, (unsigned)words_free);
    } else {
        ESP_LOGD(TAG, "%s stack watermark: %u words free", name, (unsigned)words_free);
    }
}

void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "system_monitor_task started (period=%d ms)", MONITOR_CHECK_PERIOD_MS);
    init_status_led();

    const TickType_t period_ticks = pdMS_TO_TICKS(MONITOR_CHECK_PERIOD_MS);

    for (;;) {
        system_status_t snapshot;
        xSemaphoreTake(status_mutex, portMAX_DELAY);
        snapshot = g_status;
        xSemaphoreGive(status_mutex);

        bool sensor_ok = snapshot.sensor_fault_count < SENSOR_FAULT_THRESHOLD;

        ESP_LOGI(TAG,
                 "heap_free=%u  wifi=%d mqtt=%d  faults(sensor=%u mqtt=%u drop=%u)",
                 (unsigned)esp_get_free_heap_size(),
                 snapshot.wifi_connected, snapshot.mqtt_connected,
                 (unsigned)snapshot.sensor_fault_count,
                 (unsigned)snapshot.mqtt_fault_count,
                 (unsigned)snapshot.queue_drop_count);

        log_stack_watermark("sensor_task", s_sensor_handle);
        log_stack_watermark("mqtt_task", s_mqtt_handle);
        log_stack_watermark("monitor_task", s_monitor_handle);

        update_status_led(snapshot.wifi_connected, snapshot.mqtt_connected, sensor_ok);

        vTaskDelay(period_ticks);
    }
}
