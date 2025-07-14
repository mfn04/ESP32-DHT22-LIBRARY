#include "espdht22.h"

dht22_err_t dht22_current_status = DHT22_OK;
esp_err_t dht22_current_esp_status = ESP_OK;

float dht22_temperature = 0;
float dht22_humidity = 0;

uint8_t dht22_gpio_pin = -1;

uint8_t dht22_data[DHT22_FRAMES_EXPECTED] =  {0};
DRAM_ATTR volatile uint64_t dht22_last_positive_edge_time = 0;
DRAM_ATTR volatile int dht22_last_output = DHT22_HIGH;
DRAM_ATTR volatile uint8_t dht22_positive_edges_caught = 0;
DRAM_ATTR volatile uint8_t dht22_high_times_micros[DHT22_POSITIVE_EDGES_EXPECTED] = {0};


dht22_err_t dht22_last_error(){
    return dht22_current_status;
}

esp_err_t dht22_last_esp_error(){
    return dht22_current_esp_status;
}

float dht22_get_temperature(void){
    return dht22_temperature;
}


float dht22_get_humidity(void){
    return dht22_humidity;
}

dht22_err_t dht22_init(uint8_t gpio_num){
    dht22_gpio_pin = gpio_num;
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << dht22_gpio_pin),      // Convert GPIO pin number to bit mask
        .mode = GPIO_MODE_OUTPUT,                   // Set default to GPIO output
        .intr_type = GPIO_INTR_ANYEDGE,             // Trigger an ISR at both edges
        .pull_up_en = GPIO_PULLUP_ENABLE            // Pullup enable
    };

    // Register the configuration of the GPIO pin
    DHT22_ESP_ERR_CHECK(gpio_config(&gpio_conf));

    // Install ISR server and handler to the GPIO pin
    DHT22_ESP_ERR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    DHT22_ESP_ERR_CHECK(gpio_isr_handler_add(dht22_gpio_pin, dht22_edges_isr_handler, NULL));

    // Set the GPIO to output mode and idle HIGH
    DHT22_ESP_ERR_CHECK(gpio_set_direction(dht22_gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(gpio_set_level(dht22_gpio_pin, 1));

    dht22_current_status = DHT22_OK;
    return dht22_current_status;
}

dht22_err_t dht22_probe(){
    // Reset the data array
    dht22_data[0] = dht22_data[1] = dht22_data[2] = dht22_data[3] = dht22_data[4] = 0;

    // Change mode to output and set LOW level for 1mS
    DHT22_ESP_ERR_CHECK(gpio_set_direction(dht22_gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(gpio_set_level(dht22_gpio_pin, 0));
    esp_rom_delay_us(DHT22_DELAY_START_LOW_US);

    // Set HIGH for 30uS
    DHT22_ESP_ERR_CHECK(gpio_set_level(dht22_gpio_pin, 1));
    esp_rom_delay_us(DHT22_DELAY_START_HIGH_US);
    
    // Change mode to input and wait 5mS (transmission estimate)
    DHT22_ESP_ERR_CHECK(gpio_set_direction(dht22_gpio_pin,GPIO_MODE_INPUT));
    esp_rom_delay_us(DHT22_DELAY_TRANSMISSION_US);

    // Change mode back to output and resume idle HIGH with a 1mS delay for safety
    DHT22_ESP_ERR_CHECK(gpio_set_direction(dht22_gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(gpio_set_level(dht22_gpio_pin, 1));
    esp_rom_delay_us(DHT22_DELAY_IDLE_RECOVERY_US);
    
    dht22_current_status = DHT22_OK;
    return dht22_current_status;
}

void IRAM_ATTR dht22_edges_isr_handler(void* param) {
    int current_output = gpio_get_level(dht22_gpio_pin);
    int now = esp_timer_get_time();

    // Positive edge start
    if(dht22_last_output == DHT22_LOW && current_output == DHT22_HIGH) {
        // Record start time
        dht22_last_positive_edge_time = now;
    } 
    // Positive edge end
    else if(dht22_last_output == DHT22_HIGH && current_output == DHT22_LOW){
        // Record time elapsed since start
        dht22_high_times_micros[dht22_positive_edges_caught++] = now - dht22_last_positive_edge_time;
    }
    dht22_last_output = current_output;
}

dht22_err_t dht22_format(){
    uint8_t current_bit = 1;        // Current bit (max is 40)
    uint8_t byte_index = 0;         // Current byte (data array)
    uint8_t MASK = 0b10000000;      // Mask that will be shifted to right every iteration
    uint8_t counter = 0;            // Amount of bits processed for debugging

    // Skip first two edges (handshake) and last edge which is idle
    for(int i = 2; i < dht22_positive_edges_caught-1; i++){
        if(dht22_high_times_micros[i] > DHT22_BIT_THRESHOLD_US){
            dht22_data[byte_index] |= MASK;
        }
        MASK >>= 1;
        // Reset mask when it is 0
        if(MASK == 0){
            MASK = 0b10000000;
            byte_index++;
        }
        current_bit++;
        counter++;
    }
    ESP_LOGD("DHT22_DEBUG", "YOU PROCESSED %d BITS!", counter);
    dht22_positive_edges_caught = 0;

    uint8_t checksum = dht22_data[0] + dht22_data[1] + dht22_data[2] + dht22_data[3]; // Must be last 8 bits of the sum of all data octets
    
    // Checksum calculated matches the last byte
    if(checksum == dht22_data[4]){
        float temp = (float)dht22_data[0] + ((float)dht22_data[1])/10;
        float humi = (float)dht22_data[2] + ((float)dht22_data[3])/10;
        dht22_temperature = temp;
        dht22_humidity = humi;
        dht22_current_status = DHT22_OK;
        ESP_LOGD("DHT22_DEBUG", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d",dht22_data[0], dht22_data[1], dht22_data[2], dht22_data[3], dht22_data[4]);
        return dht22_current_status;
    }
    // Checksum failed
    else {
        dht22_temperature = 0;
        dht22_humidity = 0;
        dht22_current_status = DHT22_CHECKSUM_ERROR;
        ESP_LOGD("DHT22_DEBUG", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d", dht22_data[0], dht22_data[1], dht22_data[2], dht22_data[3], dht22_data[4]);
        return dht22_current_status;
    }
}