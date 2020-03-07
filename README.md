# esp-idf-GPS-Repeater
Transfer GPS NMEA messages over Wifi   

# Background
I live in an apartment.   
GPS signal does not reach my room.   
I put my ESP32 and GPS receiver by the window and forward NMEA messages over WiFi.   

---

# Hardware requirements
ESP32.  
GPS module like NEO-6M.

---

# Software requirements
esp-idf ver4.1 or later.   
Because uart_enable_pattern_det_intr() has been changed to uart_enable_pattern_det_baud_intr().

---

# Wireing to GPS module

|GPS||ESP32|
|:-:|:-:|:-:|
|VCC|--|3.3V|
|GND|--|GND|
|TXD|--|Any Pin|

---

# Configure
You can configure UART GPIO port / WiFi setting using menuconfig.

- Socket transfer using TCP
![0001](https://user-images.githubusercontent.com/6020549/76137836-c9741900-6084-11ea-8732-1719ad117cc2.jpg)

![config_tcp](https://user-images.githubusercontent.com/6020549/76137931-b1e96000-6085-11ea-896f-2ba5a50127ce.jpg)


- Broadcast using UDP
![0002](https://user-images.githubusercontent.com/6020549/76137839-d55fdb00-6084-11ea-94f5-3a81f9b1e29a.jpg)

![config_udp](https://user-images.githubusercontent.com/6020549/76137943-c62d5d00-6085-11ea-8ecd-ce14da8832cb.jpg)

You can use udp.py as receiver.

# GPS Server for u-center
u-center is a very powerful NMEA message analysis tool.   
You can use ESP32 as u-center's GPS Server.   
ESP32 acts as a router.   
The SSID of ESP32 is 'myssid'.   
Connect to this access point.   

Start u-center and connect to ESP32.   
Default port is 5000.   

![u-center-1](https://user-images.githubusercontent.com/6020549/62000222-57aa1900-b10c-11e9-9d7d-aa4d32cdafbe.jpg)
![u-center-2](https://user-images.githubusercontent.com/6020549/62000218-57118280-b10c-11e9-867b-afa20d1caee3.jpg)
![u-center-3](https://user-images.githubusercontent.com/6020549/62000219-57118280-b10c-11e9-84ae-f07103141d4f.JPG)
![u-center-4](https://user-images.githubusercontent.com/6020549/62000220-57118280-b10c-11e9-825f-cf77f2fdcb5b.JPG)
![u-center-5](https://user-images.githubusercontent.com/6020549/62000221-57aa1900-b10c-11e9-833d-1a5a05aa68ae.jpg)

