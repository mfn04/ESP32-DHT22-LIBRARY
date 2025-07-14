#include "espdht22.h"

#define DHT22_DELAY_START_LOW_US 1000       // Initiation delay used to set signal LOW (in microseconds)
#define DHT22_DELAY_START_HIGH_US 30        // Initiation delay used to set signal HIGH (in microseconds)
#define DHT22_DELAY_TRANSMISSION_US 10000   // Time to wait for input signal from DHT22 (in microseconds)
#define DHT22_DELAY_IDLE_RECOVERY_US 1000   // Time to wait after received signal (in microseconds)

#define DHT22_BIT_THRESHOLD_US 30           // The bit threshold to determine whether a bit is 0 or 1

#define DHT22_TEMP_INT 0                    // First byte is temperature integer
#define DHT22_TEMP_DEC 1                    // Second byte is temperature decimal
#define DHT22_HUMI_INT 2                    // Third byte is humidity integer
#define DHT22_HUMI_DEC 3                    // Fourth byte is humidity decimal
#define DHT22_CHECKSUM 4                    // Fifth byte is checksum

#define DHT22_HIGH 1
#define DHT22_LOW 0

#define DHT22_POSITIVE_EDGES_EXPECTED 43    // Matches the amount of HIGH signals received
#define DHT22_FRAMES_EXPECTED 5             // Size of data array which represents frames in bytes

// Wrapper for ESP Functions to update DHT22 error
#define DHT22_ESP_ERR_CHECK(sensor,esp_err) { \
    ((dht22_t*)sensor)->current_esp_status = esp_err; \
    if(((dht22_t*)sensor)->current_esp_status != ESP_OK){ \
        ((dht22_t*)sensor)->current_status = DHT22_ESP_ERROR; \
        ESP_LOGI("ERROR","ESP TRIGGERED %d", ((dht22_t*)sensor)->current_esp_status); \
        return ((dht22_t*)sensor)->current_status; \
    }  \
}

typedef struct dht22 {
    dht22_err_t current_status;
    esp_err_t current_esp_status;

    float temperature;
    float humidity;

    uint8_t gpio_pin;

    // Store all 5 bytes in array
    // (Temperature Integer, Temperature Decimal, Humidity Integer, Humidity Decimal, Checksum)
    uint8_t data[DHT22_FRAMES_EXPECTED];

    /*****************************************************************
     * Values to be stored in DRAM, and no optimizations by compiler */

    // Each positive edge will write the timer clock to measure duration from edge to edge
    volatile uint64_t last_positive_edge_time;

    // Check whether its HIGH or LOW to figure a positive or negative edge
    volatile int last_output;

    // Positive edge counter
    volatile uint8_t positive_edges_caught;

    // Amount of time spent on high for each bit. It is 43 due to Logic Analyzer showing that there are 43 positive edges and 86 edges in total during transmission
    volatile uint8_t high_times_micros[DHT22_POSITIVE_EDGES_EXPECTED];
} dht22_t;

dht22_t* dht22_create(uint8_t gpio_pin){
    dht22_t* sensor = (dht22_t*) malloc(sizeof(dht22_t));
    *sensor = (dht22_t) {
        .current_esp_status = DHT22_OK,
        .current_status = DHT22_OK,
        .temperature = 0,
        .humidity = 0,
        .gpio_pin = gpio_pin,
        .data = {0},
        .last_positive_edge_time = 0,
        .last_output = DHT22_HIGH,
        .positive_edges_caught = 0,
        .high_times_micros = {0}
    };
    return sensor;
}

dht22_err_t dht22_destroy(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to destroy function.");
        return DHT22_INVALID_SENSOR;
    }
    DHT22_ESP_ERR_CHECK(sensor,gpio_isr_handler_remove(sensor->gpio_pin));
    free(sensor);
    return DHT22_OK;
}

dht22_err_t dht22_init(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to init function.");
        return DHT22_INVALID_SENSOR;
    }
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << sensor->gpio_pin),    // Convert GPIO pin number to bit mask
        .mode = GPIO_MODE_OUTPUT,                   // Set default to GPIO output
        .intr_type = GPIO_INTR_ANYEDGE,             // Trigger an ISR at both edges
        .pull_up_en = GPIO_PULLUP_ENABLE            // Pullup enable
    };

    // Register the configuration of the GPIO pin
    DHT22_ESP_ERR_CHECK(sensor,gpio_config(&gpio_conf));
    ESP_LOGI("DHT22_DEBUG", "Loaded GPIO config for Sensor (%d)!", sensor->gpio_pin);

    // Install ISR server and handler to the GPIO pin
    esp_err_t esp_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if(esp_err != ESP_OK && esp_err == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGI("DHT22_DEBUG", "Failed to install ISR, maybe it was already installed, skipping! Trigged by: Sensor (%d)", sensor->gpio_pin);
    }
    DHT22_ESP_ERR_CHECK(sensor,gpio_isr_handler_add(sensor->gpio_pin, dht22_edges_isr_handler, sensor));
    ESP_LOGI("DHT22_DEBUG", "Added ISR handler to Sensor (%d)!", sensor->gpio_pin);

    // Set the GPIO to output mode and idle HIGH
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_direction(sensor->gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_level(sensor->gpio_pin, 1));
    ESP_LOGI("DHT22_DEBUG", "Sensor (%d) GPIO pin was set to output and idle HIGH.", sensor->gpio_pin);

    sensor->current_status = DHT22_OK;
    return sensor->current_status;
}

dht22_err_t dht22_last_error(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to last_error function.");
        return DHT22_INVALID_SENSOR;
    }
    return sensor->current_status;
}

esp_err_t dht22_last_esp_error(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to destroy function.");
        return ESP_FAIL;
    }
    return sensor->current_esp_status;
}

float dht22_get_temperature(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to temperature function.");
        return DHT22_INVALID_SENSOR;
    }
    return sensor->temperature;
}


float dht22_get_humidity(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to humidity function.");
        return DHT22_INVALID_SENSOR;
    }
    return sensor->humidity;
}

dht22_err_t dht22_probe(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to probe function.");
        return DHT22_INVALID_SENSOR;
    }

    // Reset the data array
    sensor->data[DHT22_TEMP_INT] = sensor->data[DHT22_TEMP_DEC] 
                            = sensor->data[DHT22_HUMI_INT] = sensor->data[DHT22_HUMI_DEC] = sensor->data[DHT22_CHECKSUM] = 0;
    ESP_LOGI("DHT22_DEBUG", "Reseted data array for Sensor (%d) and attempting handshake.", sensor->gpio_pin);

    // Change mode to output and set LOW level for 1mS
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_direction(sensor->gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_level(sensor->gpio_pin, 0));
    esp_rom_delay_us(DHT22_DELAY_START_LOW_US);

    // Set HIGH for 30uS
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_level(sensor->gpio_pin, 1));
    esp_rom_delay_us(DHT22_DELAY_START_HIGH_US);
    
    // Change mode to input and wait 5mS (transmission estimate)
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_direction(sensor->gpio_pin,GPIO_MODE_INPUT));
    esp_rom_delay_us(DHT22_DELAY_TRANSMISSION_US);

    // Change mode back to output and resume idle HIGH with a 1mS delay for safety
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_direction(sensor->gpio_pin,GPIO_MODE_OUTPUT));
    DHT22_ESP_ERR_CHECK(sensor,gpio_set_level(sensor->gpio_pin, 1));
    esp_rom_delay_us(DHT22_DELAY_IDLE_RECOVERY_US);

    ESP_LOGI("DHT22_DEBUG", "Finished transmission & idle delays for Sensor (%d). Probe complete.", sensor->gpio_pin);
    
    sensor->current_status = DHT22_OK;
    return sensor->current_status;
}

void IRAM_ATTR dht22_edges_isr_handler(void* param) {
    dht22_t* sensor = (dht22_t*)param;
    int current_output = gpio_get_level(sensor->gpio_pin);
    int now = esp_timer_get_time();

    // Positive edge start
    if(sensor->last_output == DHT22_LOW && current_output == DHT22_HIGH) {
        // Record start time
        sensor->last_positive_edge_time = now;
    } 
    // Positive edge end
    else if(sensor->last_output == DHT22_HIGH && current_output == DHT22_LOW){
        // Record time elapsed since start
        sensor->high_times_micros[sensor->positive_edges_caught++] = now - sensor->last_positive_edge_time;
    }
    sensor->last_output = current_output;
}

dht22_err_t dht22_format(dht22_t* sensor){
    if(sensor == NULL){
        ESP_LOGI("DHT22_DEBUG", "An error occured! Passed a NULL pointer to format function.");
        return DHT22_INVALID_SENSOR;
    }
    uint8_t current_bit = 1;        // Current bit (max is 40)
    uint8_t byte_index = 0;         // Current byte (data array)
    uint8_t MASK = 0b10000000;      // Mask that will be shifted to right every iteration
    uint8_t counter = 0;            // Amount of bits processed for debugging

    // Skip first two edges (handshake) and last edge which is idle
    for(int i = 2; i < sensor->positive_edges_caught-1; i++){
        if(sensor->high_times_micros[i] > DHT22_BIT_THRESHOLD_US){
            sensor->data[byte_index] |= MASK;
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
    ESP_LOGI("DHT22_DEBUG", "YOU PROCESSED %d BITS!", counter);
    sensor->positive_edges_caught = 0;

    uint8_t checksum = sensor->data[DHT22_TEMP_INT] + sensor->data[DHT22_TEMP_DEC] 
                            + sensor->data[DHT22_HUMI_INT] + sensor->data[DHT22_HUMI_DEC]; // Must be last 8 bits of the sum of all data octets
    
    // Checksum calculated matches the last byte
    if(checksum == sensor->data[DHT22_CHECKSUM]){
        float decimal_conv = 10;
        float temp = (float)sensor->data[DHT22_TEMP_INT] + ((float)sensor->data[DHT22_TEMP_DEC])/decimal_conv;
        float humi = (float)sensor->data[DHT22_HUMI_INT] + ((float)sensor->data[DHT22_HUMI_DEC])/decimal_conv;
        sensor->temperature = temp;
        sensor->humidity = humi;
        sensor->current_status = DHT22_OK;
        ESP_LOGI("DHT22_DEBUG", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d",
            sensor->data[DHT22_TEMP_INT],sensor->data[DHT22_TEMP_DEC],
                           sensor->data[DHT22_HUMI_INT], sensor->data[DHT22_HUMI_DEC], sensor->data[DHT22_CHECKSUM]);
        return sensor->current_status;
    }
    // Checksum failed
    else {
        sensor->temperature = 0;
        sensor->humidity = 0;
        sensor->current_status = DHT22_CHECKSUM_ERROR;
        ESP_LOGI("DHT22_DEBUG", "Temperature: %d, Temperature Decimal: %d, Humidity: %d, Humidity Decimal: %d, Checksum: %d",
            sensor->data[DHT22_TEMP_INT], sensor->data[DHT22_TEMP_DEC],
                            sensor->data[DHT22_HUMI_INT], sensor->data[DHT22_HUMI_DEC], sensor->data[DHT22_CHECKSUM]);
        return sensor->current_status;
    }
}