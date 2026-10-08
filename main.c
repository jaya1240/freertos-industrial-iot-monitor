#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "config.h"
#include "sensor_task.h"
#include "mqtt_task.h"
#include "system_monitor_task.h"

static const char *TAG = "main";

/* Definitions of the extern globals declared in sensor_task.h */
QueueHandle_t sensor_queue = NULL;
SemaphoreHandle_t status_mutex = NULL;
system_status_t g_status = {0};

void status_update(void (*mutator)(system_status_t *status))
{
    xSemaphoreTake(status_mutex, portMAX_DELAY);
    mutator(&g_status);
    xSemaphoreGive(status_mutex);
}

void app_main(void)
{
    ESP_LOGI(TAG, "FreeRTOS Industrial IoT Monitoring System starting");

    sensor_queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(sensor_reading_t));
    status_mutex = xSemaphoreCreateMutex();

    if (sensor_queue == NULL || status_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create queue/mutex, halting");
        return;
    }

    TaskHandle_t sensor_handle = NULL;
    TaskHandle_t mqtt_handle = NULL;
    TaskHandle_t monitor_handle = NULL;

    xTaskCreate(sensor_task, "sensor_task",
                SENSOR_TASK_STACK_SIZE, NULL,
                SENSOR_TASK_PRIORITY, &sensor_handle);

    xTaskCreate(mqtt_task, "mqtt_task",
                MQTT_TASK_STACK_SIZE, NULL,
                MQTT_TASK_PRIORITY, &mqtt_handle);

    xTaskCreate(system_monitor_task, "monitor_task",
                MONITOR_TASK_STACK_SIZE, NULL,
                MONITOR_TASK_PRIORITY, &monitor_handle);

    system_monitor_register_tasks(sensor_handle, mqtt_handle, monitor_handle);

    ESP_LOGI(TAG, "All tasks created: sensor(prio=%d) mqtt(prio=%d) monitor(prio=%d)",
             SENSOR_TASK_PRIORITY, MQTT_TASK_PRIORITY, MONITOR_TASK_PRIORITY);
}
