#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"



#define WIFI_SSID "bobternet"
#define WIFI_PASS "admin377"
#define BLINK_GPIO GPIO_NUM_2  
#define CONFIG_BLINK_PERIOD 1000
#define HTTP_SERVER "httpbin.org"
#define HTTP_PORT 80
#define HTTP_REQUEST "GET / HTTP/1.1\r\nHost: " HTTP_SERVER "\r\nConnection: close\r\n\r\n"





static const char *TAG = "system";
static bool s_led_state = false;
static bool is_connected = false;  


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


static void blink_led(void) {
    gpio_set_level(BLINK_GPIO, s_led_state);
}


static void configure_led(void) {
    ESP_LOGI(TAG, "Configured LED for blinking.");
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}


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


void http_get_task(void *pvParameters) {
    while (!is_connected) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);  
    }

    struct addrinfo hints = { 0 };
    struct addrinfo *res;
    int sock;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the IP address of the server
    int err = getaddrinfo(HTTP_SERVER, "80", &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for %s", HTTP_SERVER);
        vTaskDelete(NULL);
        return;
    }


    sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket allocation failed");
        freeaddrinfo(res);
        vTaskDelete(NULL);
        return;
    }

    // Connect to the server
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Socket connection failed");
        close(sock);
        freeaddrinfo(res);
        vTaskDelete(NULL);
        return;
    }
    freeaddrinfo(res);  // Free the address info structure

    // Send the HTTP GET request
    if (write(sock, HTTP_REQUEST, strlen(HTTP_REQUEST)) < 0) {
        ESP_LOGE(TAG, "Failed to send request");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Receive the response
    char recv_buf[512];
    int len;
    ESP_LOGI(TAG, "HTTP response:");
    do {
        len = read(sock, recv_buf, sizeof(recv_buf) - 1);
        if (len > 0) {
            recv_buf[len] = '\0';  // Null-terminate the response
            printf("%s", recv_buf);  // Print the response to the console
        }
    } while (len > 0);

    // Close the socket
    close(sock);
    ESP_LOGI(TAG, "HTTP GET request complete");
    vTaskDelete(NULL);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    configure_led();
    wifi_init();

    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);

    while (1) {


        if (!is_connected) {
            ESP_LOGI(TAG, "Flashing LED as Wi-Fi is not connected.");
            s_led_state = !s_led_state;  // Toggle LED state
            blink_led();
            vTaskDelay(CONFIG_BLINK_PERIOD);
        } else {
            // Turn off LED and stop flashing if connected
            gpio_set_level(BLINK_GPIO, 0);
            ESP_LOGI(TAG, "Connected to Wi-Fi, LED stopped flashing.");
            vTaskDelay(1000);  // Check every second if disconnected
        }
    }
}
