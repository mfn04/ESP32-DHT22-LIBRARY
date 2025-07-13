## DHT22 Library
DHT22 is a sensor device that measures the temperature and humidty. DHT22 Library aims to create a light version of the Arduino IDE DHT22 libraries. The purpose of this repository was to resolve an issue faced by the author during the Smart Irrigation System project where the DHT22 consistently was active even when no data was being fetched, which may be due to how the library was constantly probing the DHT22. This in turn created a need to increase power efficiency and put a stop to wasting the Li-Ion 18650 battery.

## Components
- ESP32
- DHT22
- Jumper cables
- Logic Analyzer & Multimeter

## Features
- Probe DHT22 only when needed

## Timing
- GPIO is set to output
- Data bus must be high during idle for at least 1-2 seconds.
- Data bus is dragged low for 1mS.
- Data bus is pulsed high for 30uS.
- Wait for DHT22 response of 80uS low before data bits are sent

![signal]

## Signal Interpretation
The DHT22 sensor always sends out a ~50uS low signal as an indication of the start of a bit transmission. When it is high for 26-30us it is a '0' bit, if it is high for around ~70uS it is a '1' bit.


[signal]: https://raw.githubusercontent.com/mfn04/ESP32-DHT22-LIBRARY/refs/heads/main/imgs/dht-signal.png