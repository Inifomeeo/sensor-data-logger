#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "dht.h"

#define DHT_GPIO 27
#define DHT_TYPE DHT_TYPE_DHT11

static const char *TAG = "SENSOR_DATA_LOGGER";

// Sensor data structure
typedef struct {
    float temperature;
    float humidity;
} sensor_data_t;

void read_sensor(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    sensor_data_t data;
    float temperature, humidity;
    
    // Configure GPIO
    gpio_reset_pin(DHT_GPIO);
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

    while (1) {
        if (dht_read_float_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature) == ESP_OK) {
            data.temperature = temperature;
            data.humidity = humidity;
            
            if (xQueueSendToBack(queue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Queue full - dropping reading");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read sensor");
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void log_data(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    sensor_data_t data;
    
    while (1) {
        if (xQueueReceive(queue, &data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", 
                   data.temperature, data.humidity);
        }
    }
}

void app_main(void)
{
    TaskHandle_t read_handle, log_handle;
    read_handle = log_handle = NULL;

    // Queue capable of containing 5 sensor readings
    QueueHandle_t sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
    if (sensor_queue == 0) {
        ESP_LOGW(TAG, "Failed to create queue");
    }
    
    xTaskCreate(read_sensor, "Read_Sensor", 2048, sensor_queue, 5, &read_handle);
    configASSERT(read_handle);

    xTaskCreate(log_data, "Log_Data", 2048, sensor_queue, 5, &log_handle);
    configASSERT(log_handle);
    
    ESP_LOGI(TAG, "Sensor data logger started. Free heap: %d", esp_get_free_heap_size());
}