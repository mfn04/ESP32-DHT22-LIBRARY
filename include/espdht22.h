#ifndef _ESP_DHT22_H_
#define _ESP_DHT22_H_

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define DHT22_DELAY_START_LOW_US 1000       // Initiation delay used to set signal LOW (in microseconds)
#define DHT22_DELAY_START_HIGH_US 30        // Initiation delay used to set signal HIGH (in microseconds)
#define DHT22_DELAY_TRANSMISSION_US 10000   // Time to wait for input signal from DHT22 (in microseconds)
#define DHT22_DELAY_IDLE_RECOVERY_US 1000   // Time to wait after received signal (in microseconds)

#define DHT22_BIT_THRESHOLD_US 30           // The bit threshold to determine whether a bit is 0 or 1

#define DHT22_HIGH 1
#define DHT22_LOW 0

#define DHT22_POSITIVE_EDGES_EXPECTED 43    // Matches the amount of HIGH signals received
#define DHT22_FRAMES_EXPECTED 5             // Size of data array which represents frames in bytes

// Wrapper for ESP Functions to update DHT22 error
#define DHT22_ESP_ERR_CHECK(esp_err) { dht22_current_esp_status = esp_err; if(dht22_current_esp_status != ESP_OK){ dht22_current_status = DHT22_ESP_ERROR; ESP_LOGI("ERROR","ESP TRIGGERED %d", dht22_current_esp_status); return dht22_current_status; }  }

typedef enum {
    DHT22_CHECKSUM_ERROR,   // Failure to validate the data received from DHT22
    DHT22_ESP_ERROR,        // Failed to initialize the DHT22 library due to internal ESP32 errors check dht22_last_esp_error
    DHT22_NO_PROBE,         // The function probe was never called
    DHT22_OK                // DHT22 is functioning correctly
} dht22_err_t;

extern dht22_err_t dht22_current_status;
extern esp_err_t dht22_current_esp_status;
extern float dht22_temperature;
extern float dht22_humidity;

extern uint8_t dht22_gpio_pin;
 
// Store all 5 bytes in array
// (Temperature Integer, Temperature Decimal, Humidity Integer, Humidity Decimal, Checksum)
extern uint8_t dht22_data[DHT22_FRAMES_EXPECTED];

/*****************************************************************
 * Values to be stored in DRAM, and no optimizations by compiler */

// Each positive edge will write the timer clock to measure duration from edge to edge
extern volatile uint64_t dht22_last_positive_edge_time;

// Check whether its HIGH or LOW to figure a positive or negative edge
extern volatile int dht22_last_output;

// Positive edge counter
extern volatile uint8_t dht22_positive_edges_caught;

// Amount of time spent on high for each bit. It is 43 due to Logic Analyzer showing that there are 43 positive edges and 86 edges in total during transmission
extern volatile uint8_t dht22_high_times_micros[DHT22_POSITIVE_EDGES_EXPECTED];

/**
 * Last Error is a function that will display the current status of the DHT22.
 * @param void
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_last_error(void);

/**
 * Last ESP Error is a function that will display the current status of the ESP32.
 * @param void
 * @return current esp error status as a esp_err_t type
 */
esp_err_t dht22_last_esp_error(void);

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
 * @param gpio_num is the number of the digital pin
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_init(uint8_t gpio_num);

/**
 * Probe is a function that will send the initial handshake to the DHT22 device, with a 1mS LOW followed with a 30uS HIGH and immediately
 * switch mode to input. It will wait for 5mS to ensure that the transmission is complete before changing the GPIO mode back to output and
 * idle HIGH.
 * @param void
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_probe(void);

/**
 * Format is a function that will read from the high_times_micros array and write the expected bits based on the timings onto the data array.
 * It is known that DHT22 starts with a 50uS LOW before a bit and sends 26-29uS HIGH for '0' bit and around 70uS for '1' bit.
 * @param void
 * @return current error status as a dht22_err_t type
 */
dht22_err_t dht22_format(void);

/**
 * Get temperature is a getter function that will retrieve the current temperature stored.
 * @param void
 * @return temperature value as a float
 */
float dht22_get_temperature(void);

/**
 * Get humidity is a getter function that will retrieve the current humidity stored.
 * @param void
 * @return humidity value as a float
 */
float dht22_get_humidity(void);

#endif