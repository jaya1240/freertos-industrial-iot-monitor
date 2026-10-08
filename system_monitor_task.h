#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Task handles the monitor watches for stack high-water marks.
 * Set by main.c after all tasks are created. */
void system_monitor_register_tasks(TaskHandle_t sensor_handle,
                                    TaskHandle_t mqtt_handle,
                                    TaskHandle_t monitor_handle);

void system_monitor_task(void *pvParameters);
