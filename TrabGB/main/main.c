#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define WIFI_SSID "É o celular do Pedro"
#define WIFI_PASS "997114511"

#define LED_GPIO    GPIO_NUM_18
#define BUTTON_GPIO GPIO_NUM_22
#define RESET_GPIO  GPIO_NUM_19

static const char *TAG = "CONTADOR";

static int contador = 0;
static SemaphoreHandle_t mutex_contador;

static const char *pagina_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta http-equiv='refresh' content='2'>"
    "<title>Contador ESP32</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;text-align:center;background:#009c3b;color:#fff;padding:50px;margin:0;}"
    "h1{font-size:3em;color:#ffdf00;text-shadow:2px 2px 4px #000;}"
    ".caixa{background:#002776;border-radius:20px;padding:40px;display:inline-block;margin:20px;border:4px solid #ffdf00;}"
    ".contador{font-size:8em;font-weight:bold;color:#ffdf00;}"
    ".integrantes{margin-top:40px;background:#002776;border-radius:15px;padding:20px 40px;display:inline-block;border:2px solid #ffdf00;}"
    ".integrantes h3{color:#ffdf00;margin-bottom:10px;}"
    ".integrantes p{color:#fff;margin:5px 0;font-size:1.1em;}"
    "footer{margin-top:30px;color:#ffdf00;font-size:0.9em;}"
    "</style></head><body>"
    "<h1>&#127463;&#127479; Contador ESP32</h1>"
    "<div class='caixa'>"
    "<div class='contador'>%d</div>"
    "<p style='color:#fff;margin-top:10px;'>cliques no botao</p>"
    "</div>"
    "<br>"
    "<div class='integrantes'>"
    "<h3>Integrantes</h3>"
    "<p>Gustavo Bueno</p>"
    "<p>Matheus Frohlich</p>"
    "<p>Pedro Bassani</p>"
    "</div>"
    "<footer>Sistemas Embarcados 2026/01 - UNISINOS</footer>"
    "</body></html>";

static esp_err_t handle_pagina(httpd_req_t *req)
{
    char resposta[2048];

    xSemaphoreTake(mutex_contador, portMAX_DELAY);
    int valor = contador;
    xSemaphoreGive(mutex_contador);

    snprintf(resposta, sizeof(resposta), pagina_html, valor);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resposta, strlen(resposta));
    return ESP_OK;
}

void task_botao(void *pvParameters)
{
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    int estado_anterior = 1;

    while (1) {
        int estado_atual = gpio_get_level(BUTTON_GPIO);

        if (estado_anterior == 1 && estado_atual == 0) {
            xSemaphoreTake(mutex_contador, portMAX_DELAY);
            contador++;
            ESP_LOGI(TAG, "botao pressionado, contador: %d", contador);
            xSemaphoreGive(mutex_contador);
        }

        estado_anterior = estado_atual;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void task_reset(void *pvParameters)
{
    gpio_set_direction(RESET_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RESET_GPIO, GPIO_PULLUP_ONLY);

    int estado_anterior = 1;

    while (1) {
        int estado_atual = gpio_get_level(RESET_GPIO);

        if (estado_anterior == 1 && estado_atual == 0) {
            xSemaphoreTake(mutex_contador, portMAX_DELAY);
            contador = 0;
            ESP_LOGI(TAG, "contador resetado!");
            xSemaphoreGive(mutex_contador);
        }

        estado_anterior = estado_atual;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void task_led(void *pvParameters)
{
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    int estado_led = 0;
    int contador_anterior = 0;

    while (1) {
        xSemaphoreTake(mutex_contador, portMAX_DELAY);
        int valor = contador;
        xSemaphoreGive(mutex_contador);

        if (valor == 0) {
            estado_led = 0;
            gpio_set_level(LED_GPIO, 0);
            contador_anterior = 0;
        } else if (valor != contador_anterior) {
            estado_led = !estado_led;
            gpio_set_level(LED_GPIO, estado_led);
            contador_anterior = valor;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void task_servidor(void *pvParameters)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t servidor = NULL;

    if (httpd_start(&servidor, &config) == ESP_OK) {
        httpd_uri_t uri = {
            .uri     = "/",
            .method  = HTTP_GET,
            .handler = handle_pagina,
        };
        httpd_register_uri_handler(servidor, &uri);
        ESP_LOGI(TAG, "servidor http iniciado");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void wifi_evento(void *arg, esp_event_base_t base,
                        int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evento = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "conectado! acesse: http://" IPSTR, IP2STR(&evento->ip_info.ip));
    }
}

void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_evento, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_evento, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void app_main(void)
{
    nvs_flash_init();

    mutex_contador = xSemaphoreCreateMutex();

    wifi_init();

    xTaskCreate(task_botao,   "task_botao", 2048, NULL, 3, NULL);
    xTaskCreate(task_reset,   "task_reset", 2048, NULL, 3, NULL);
    xTaskCreate(task_led,     "task_led",   2048, NULL, 2, NULL);
    xTaskCreate(task_servidor,"task_http",  4096, NULL, 1, NULL);
}