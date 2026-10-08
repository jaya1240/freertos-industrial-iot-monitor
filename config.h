#pragma once

/* =========================================================================
 * Wi-Fi Configuration
 * ========================================================================= */
#define WIFI_SSID               "YOUR_WIFI_SSID"
#define WIFI_PASSWORD           "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_MAX_RETRY  10

/* =========================================================================
 * MQTT Configuration
 * ========================================================================= */
#define MQTT_BROKER_URI         "mqtt://broker.example.com:1883"
#define MQTT_CLIENT_ID          "industrial-iot-node-01"
#define MQTT_TOPIC_TELEMETRY    "industrial/node01/telemetry"
#define MQTT_TOPIC_STATUS       "industrial/node01/status"
#define MQTT_QOS                1

/* =========================================================================
 * Sensor Configuration
 * ========================================================================= */
#define DHT22_GPIO              GPIO_NUM_4
#define BMP280_I2C_PORT         I2C_NUM_0
#define BMP280_SDA_GPIO         GPIO_NUM_21
#define BMP280_SCL_GPIO         GPIO_NUM_22
#define BMP280_I2C_ADDR         0x76
#define I2C_MASTER_FREQ_HZ      100000

#define STATUS_LED_GPIO         GPIO_NUM_2

/* =========================================================================
 * Task Timing
 * ========================================================================= */
#define SENSOR_SAMPLE_PERIOD_MS      2000
#define MONITOR_CHECK_PERIOD_MS      5000
#define SENSOR_FAULT_THRESHOLD       5

/* =========================================================================
 * Task Priorities & Stack Sizes (in words, not bytes)
 * ========================================================================= */
#define SENSOR_TASK_PRIORITY         3
#define SENSOR_TASK_STACK_SIZE       4096

#define MQTT_TASK_PRIORITY           2
#define MQTT_TASK_STACK_SIZE         6144

#define MONITOR_TASK_PRIORITY        1
#define MONITOR_TASK_STACK_SIZE      3072

/* =========================================================================
 * Queue / Sync Object Sizing
 * ========================================================================= */
#define SENSOR_QUEUE_LENGTH          10
