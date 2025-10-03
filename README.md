# Accurate Body Vitals Measuring
#### Hi everyone. This project made during a course for Microcontrollers.
#### It uses ESP32 as core.(You can use any ESP8266 Mod but it is important that using a module with higher RAM is better for using two I2C modules/sensors.
#### I wrote the code for OLED display module too.

## Used Modules and Sensors:
### ESP32(or ESP8266 but it is slower) as core
### MAX30205 Human Body temperature sensor
### MAX30100 Heart beat and SpO2 measuring sensor
### 0.96 OLED Display module.




## API
```
server.on("/api/temp",    HTTP_GET,      handleTemp);
server.on("/api/vitals",  HTTP_GET,      handleVitals);
server.on("/values",      HTTP_GET,      handleValues);
server.on("/api/info",    HTTP_GET,      handleInfo);
```
you can get heart beat and SpO2 values on /values.
you can get all values on /api/vitals.
you can get information about connecting to ESP or connected ESP on /api/info.
you can get just temperature in /api/temp.
