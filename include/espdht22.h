#ifndef _ESP_DHT22_H_
#define _ESP_DHT22_H_

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

typedef enum {
    DHT22_CHECKSUM_ERROR,   // Failure to validate the data received from DHT22
    DHT22_ESP_ERROR,        // Failed to initialize the DHT22 library due to internal ESP32 errors check dht22_last_esp_error
    DHT22_NO_PROBE,         // The function probe was never called
    DHT22_INVALID_SENSOR,   // Sensor pointer is invalid
    DHT22_OK,               // DHT22 is functioning correctly
    DHT22_FAIL              // Generic failure
} dht22_err_t;

typedef struct dht22 dht22_t;

/**
 * Create is a function that will provide a sensor object that can be probed.
 * @param uint8_t is the GPIO number this sensor will be registered to
 * @return new sensor object
 */
dht22_t* dht22_create(uint8_t gpio_num);

/**
 * Destroy is a function that will clean up the sensor object and free it from memory.
 * @param dht22_t* is the pointer to the sensor to be destroyed
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_destroy(dht22_t*);

/**
 * Last Error is a function that will display the current status of the DHT22.
 * @param dht22_t* is the pointer to the sensor to probe its last error
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_last_error(dht22_t*);

/**
 * Last ESP Error is a function that will display the current status of the ESP32.
 * @param dht22_t* is the pointer to the sensor to probe its last esp error
 * @return current esp error status as a esp_err_t type
 */
esp_err_t dht22_last_esp_error(dht22_t*);

/**
 * Function is an Interrupt Service Routine (ISR) and will be called at the event of every edge. This will help store the duration of each
 * high in microseconds in high_times_micros.
 * @param param is a pointer to pass to the function when the ISR is registered (It is not utilized)
 * @return void
 */
void dht22_edges_isr_handler(void* param);

/**
 * Init is a function that will initialize the GPIO with the pin required with an internal pull up and set the default output of the GPIO
 * to HIGH for idle.
 * @param dht22_t* is the pointer to the sensor to be initialized
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_init(dht22_t*);

/**
 * Probe is a function that will send the initial handshake to the DHT22 device, with a 1mS LOW followed with a 30uS HIGH and immediately
 * switch mode to input. It will wait for 5mS to ensure that the transmission is complete before changing the GPIO mode back to output and
 * idle HIGH.
 * @param void
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_probe(dht22_t*);

/**
 * Format is a function that will read from the high_times_micros array and write the expected bits based on the timings onto the data array.
 * It is known that DHT22 starts with a 50uS LOW before a bit and sends 26-29uS HIGH for '0' bit and around 70uS for '1' bit.
 * @param void
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_format(dht22_t*);

/**
 * Get temperature is a getter function that will retrieve the current temperature stored.
 * @param void
 * @return temperature value as a float
 */
float dht22_get_temperature(dht22_t*);

/**
 * Get humidity is a getter function that will retrieve the current humidity stored.
 * @param void
 * @return humidity value as a float
 */
float dht22_get_humidity(dht22_t*);

#endif