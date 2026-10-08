#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/* One sensor sample, queued from sensor_task -> mqtt_task */
typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    int64_t timestamp_us;
} sensor_reading_t;

/* Shared system status, protected by status_mutex.
 * Written by sensor_task / mqtt_task, read by system_monitor_task. */
typedef struct {
    int64_t last_sensor_read_us;
    int64_t last_publish_us;
    uint32_t sensor_fault_count;
    uint32_t mqtt_fault_count;
    uint32_t queue_drop_count;
    bool wifi_connected;
    bool mqtt_connected;
} system_status_t;

/* Globals shared across tasks (defined in main.c) */
extern QueueHandle_t sensor_queue;
extern SemaphoreHandle_t status_mutex;
extern system_status_t g_status;

/* Helper: safely update a field in g_status under status_mutex.
 * Pass a function that mutates the status; keeps critical sections short. */
void status_update(void (*mutator)(system_status_t *status));

void sensor_task(void *pvParameters);
