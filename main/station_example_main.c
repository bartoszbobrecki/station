#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"
#include "driver/gpio.h"

#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"
#define BLINK_GPIO GPIO_NUM_2  // Adjust this according to your setup
#define CONFIG_BLINK_PERIOD 500  // Blink period in milliseconds

static const char *TAG = "wifi_led_example";
static bool s_led_state = false;
static bool is_connected = false;  // Tracks Wi-Fi connection status

// Wi-Fi event handler to update connection status
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_connected = false;
        ESP_LOGI(TAG, "Disconnected from Wi-Fi, attempting to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        is_connected = true;
        ESP_LOGI(TAG, "Connected to Wi-Fi with IP address");
    }
}

// Function to toggle LED state
static void blink_led(void) {
    gpio_set_level(BLINK_GPIO, s_led_state);
}

// Configure the LED GPIO
static void configure_led(void) {
    ESP_LOGI(TAG, "Configured LED for blinking.");
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

// Wi-Fi initialization function
void wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

// Main application function
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    configure_led();
    wifi_init();

    while (1) {
        if (!is_connected) {
            ESP_LOGI(TAG, "Flashing LED as Wi-Fi is not connected.");
            s_led_state = !s_led_state;  // Toggle LED state
            blink_led();
            vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        } else {
            // Turn off LED and stop flashing if connected
            gpio_set_level(BLINK_GPIO, 0);
            ESP_LOGI(TAG, "Connected to Wi-Fi, LED stopped flashing.");
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Check every second if disconnected
        }
    }
}
