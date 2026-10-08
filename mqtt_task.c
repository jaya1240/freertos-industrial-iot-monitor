#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "mqtt_task.h"
#include "sensor_task.h"
#include "config.h"

static const char *TAG = "mqtt_task";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static esp_mqtt_client_handle_t mqtt_client = NULL;
static int wifi_retry_count = 0;

/* ---- status_update() mutator callbacks ---- */
static void mark_wifi_connected(system_status_t *s)   { s->wifi_connected = true; }
static void mark_wifi_disconnected(system_status_t *s) { s->wifi_connected = false; }
static void mark_mqtt_connected(system_status_t *s)    { s->mqtt_connected = true; }
static void mark_mqtt_disconnected(system_status_t *s) { s->mqtt_connected = false; }
static void mark_mqtt_fault(system_status_t *s)        { s->mqtt_fault_count++; }
static void mark_publish_ok(system_status_t *s)        { s->last_publish_us = esp_timer_get_time(); }

/* ---------------------------------------------------------------------
 * Wi-Fi event handling
 * --------------------------------------------------------------------- */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        status_update(mark_wifi_disconnected);
        if (wifi_retry_count < WIFI_CONNECT_MAX_RETRY) {
            esp_wifi_connect();
            wifi_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying (%d/%d)",
                     wifi_retry_count, WIFI_CONNECT_MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_retry_count = 0;
        status_update(mark_wifi_connected);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID '%s'...", WIFI_SSID);
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
}

/* ---------------------------------------------------------------------
 * MQTT event handling
 * --------------------------------------------------------------------- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            status_update(mark_mqtt_connected);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            status_update(mark_mqtt_disconnected);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error event");
            status_update(mark_mqtt_fault);
            break;
        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* Serialize a sensor_reading_t to a small JSON payload.
 * Caller must free() the returned string. */
static char *reading_to_json(const sensor_reading_t *r)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature_c", r->temperature_c);
    cJSON_AddNumberToObject(root, "humidity_pct", r->humidity_pct);
    cJSON_AddNumberToObject(root, "pressure_hpa", r->pressure_hpa);
    cJSON_AddNumberToObject(root, "timestamp_us", (double)r->timestamp_us);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

void mqtt_task(void *pvParameters)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();
    mqtt_app_start();

    sensor_reading_t reading;

    for (;;) {
        /* Block until sensor_task produces a new reading. */
        if (xQueueReceive(sensor_queue, &reading, portMAX_DELAY) == pdTRUE) {
            char *payload = reading_to_json(&reading);
            if (payload == NULL) {
                ESP_LOGE(TAG, "Failed to serialize reading to JSON");
                continue;
            }

            int msg_id = esp_mqtt_client_publish(
                mqtt_client, MQTT_TOPIC_TELEMETRY, payload, 0, MQTT_QOS, 0);

            if (msg_id >= 0) {
                status_update(mark_publish_ok);
                ESP_LOGI(TAG, "Published msg_id=%d: %s", msg_id, payload);
            } else {
                status_update(mark_mqtt_fault);
                ESP_LOGE(TAG, "Publish failed");
            }

            free(payload);
        }
    }
}
