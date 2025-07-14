
#include "espdht22.h"

#define DHT22_PIN 25

void app_main() {

    dht22_t* sensor = dht22_create(DHT22_PIN);

    dht22_init(sensor);

    while(1){

        dht22_err_t prob_err = dht22_probe(sensor);
        if(prob_err != DHT22_OK){
            esp_err_t last_err = dht22_last_esp_error(sensor);
            ESP_LOGI("INFO", "FOUND AN ERROR in probe DHT22 %d, %d", prob_err, last_err);
        }
        dht22_err_t form_err = dht22_format(sensor);
        if(form_err != DHT22_OK){
            esp_err_t last_err = dht22_last_esp_error(sensor);
            ESP_LOGI("INFO", "FOUND AN ERROR in format DHT22 %d, %d", form_err, last_err);
        }

        float temp = dht22_get_temperature(sensor);
        float humi = dht22_get_humidity(sensor);

        ESP_LOGI("INFO", "Temp: %.2f, Humi: %.2f", temp, humi);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    dht22_destroy(sensor);

}