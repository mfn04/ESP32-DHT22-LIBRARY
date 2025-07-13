
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"


void app_main() {
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << 25),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    
    gpio_config(&gpio_conf);

    gpio_set_direction(GPIO_NUM_25,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_25, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    while(1){
        gpio_set_direction(GPIO_NUM_25,GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, 0);
        esp_rom_delay_us(1000);
        gpio_set_level(GPIO_NUM_25, 1);
        esp_rom_delay_us(30);
        gpio_set_direction(GPIO_NUM_25,GPIO_MODE_INPUT);
        vTaskDelay(2);
        gpio_set_direction(GPIO_NUM_25,GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

}