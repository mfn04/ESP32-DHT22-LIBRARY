
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define GPIO_PIN 25
#define HIGH 1
#define LOW 0

void dht_init(){
    gpio_set_direction(GPIO_PIN,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_PIN, 1);
}

void dht_probe(){
        gpio_set_direction(GPIO_PIN,GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_PIN, 0);
        esp_rom_delay_us(1000);
        gpio_set_level(GPIO_PIN, 1);
        esp_rom_delay_us(30);
        gpio_set_direction(GPIO_PIN,GPIO_MODE_INPUT);
        esp_rom_delay_us(5000);
        gpio_set_direction(GPIO_PIN,GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_PIN, 1);
        esp_rom_delay_us(2000);
}

// From Logic Analyzer it was found that there are 43 positive edges and 86 edges in total during transmission
const int POSITIVE_EDGES = 43;
const int FRAMES = 5;
static uint8_t data[5] =  {0};
DRAM_ATTR volatile uint64_t last_positive_edge_time = 0;
DRAM_ATTR volatile int last_output = HIGH;
DRAM_ATTR volatile uint8_t positive_edges_caught = 0;
DRAM_ATTR volatile uint8_t high_times_micros[43] = {0};

void IRAM_ATTR reader(void* param) {
    int current_output = gpio_get_level(GPIO_PIN);
    int now = esp_timer_get_time();

    if(last_output == LOW && current_output == HIGH) {
        last_positive_edge_time = now;
    } else if(last_output == HIGH && current_output == LOW){
        high_times_micros[positive_edges_caught++] = now - last_positive_edge_time;
    }
    last_output = current_output;
}

void dht_format(){
    uint8_t current_bit = 1;
    uint8_t byte_index = 0;
    uint8_t MASK = 0b10000000;
    uint8_t counter = 0;
    for(int i = 2; i < positive_edges_caught-1; i++){
        //if(i < 2 || i == (positive_edges_caught-1)) continue; // Skip first and last positive edge
        if(high_times_micros[i] > 30){
            data[byte_index] |= MASK;
        }
        MASK >>= 1;
        if(MASK == 0){
            MASK = 0b10000000;
            byte_index++;
        }
        current_bit++;
        counter++;
    }
    ESP_LOGI("INFO", "YOU PROCESSED %d BITS!", counter);
}

void app_main() {

    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    
    gpio_config(&gpio_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_PIN, reader, NULL);

    dht_init();

    while(1){
        data[0] = data[1] = data[2] = data[3] = data[4] = 0;

        dht_probe();
        dht_format();

        ESP_LOGI("DEBUG", "Edges caught: %d", positive_edges_caught);
        for(int i = 0; i < positive_edges_caught; i++){
            ESP_LOGI("DEBUG", "high_times: %d", high_times_micros[i]);
            high_times_micros[i] = 0;
        }
        positive_edges_caught = 0;
        uint8_t checksum = data[0] + data[1] + data[2] + data[3];
        if(checksum == data[4]){
            float temp = (float)data[0] + ((float)data[1])/10;
            float humi = (float)data[2] + ((float)data[3])/10;
            ESP_LOGI("INFO", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d",data[0], data[1], data[2], data[3], data[4]);
            ESP_LOGI("INFO", "Temperature: %f, Humidity: %f",temp, humi);
        } else {
            ESP_LOGI("INFO", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d",data[0], data[1], data[2], data[3], data[4]);
            ESP_LOGI("INFO", "Checksum failed!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

}