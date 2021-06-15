# esp-idf-GPS-Repeater
Transfer GPS NMEA messages over Wifi/Bluetooth   

# Background
I live in an apartment.   
GPS signal does not reach my room.   
I put my ESP32 and GPS receiver by the window and forward NMEA messages over WiFi.   

---

# Hardware requirements
- ESP32   
- GPS module like NEO-6M   

---

# Software requirements
- esp-idf ver4.1 or later   
 Because uart_enable_pattern_det_intr() has been changed to uart_enable_pattern_det_baud_intr().

---

# Wireing to GPS module

|GPS||ESP32|
|:-:|:-:|:-:|
|VCC|--|3.3V|
|GND|--|GND|
|TXD|--|Any Pin|

---

# Install
```
git clone https://github.com/nopnop2002/esp-idf-GPS-Repeater
cd esp-idf-GPS-Repeater
idf.py set-target esp32
idf.py menuconfig
idf.py install
```

---

# Configure
You can configure UART GPIO port and transfer protocol using menuconfig.

## Transfer using TCP Socket   
![0001](https://user-images.githubusercontent.com/6020549/121999796-c53abd80-cde8-11eb-9715-010d359806d6.jpg)

![config-socket](https://user-images.githubusercontent.com/6020549/121999980-15198480-cde9-11eb-8b99-c8cffb51eb4c.jpg)

You can use tcp.py as receiver.   
`python ./tcp.py`

## Transfer using UDP Broadcast   
![0002](https://user-images.githubusercontent.com/6020549/121999806-c966db00-cde8-11eb-9e83-1bdf7018b47c.jpg)

![config-udp](https://user-images.githubusercontent.com/6020549/122000013-1e0a5600-cde9-11eb-8393-9f0b528c11cc.jpg)

You can use udp.py as receiver.   
`python ./udp.py`

## Transfer using Bluetooth SPP   

![0003](https://user-images.githubusercontent.com/6020549/121999813-cbc93500-cde8-11eb-9de1-927f0d70b3af.jpg)

![config-spp](https://user-images.githubusercontent.com/6020549/122000026-2498cd80-cde9-11eb-95b4-ff4458cc3502.jpg)

You can use Iphone/Android as receiver.

# GPS Server for u-center
[u-center](https://www.u-blox.com/en/product/u-center) is a very powerful NMEA message analysis tool.   
You can use ESP32 as u-center's GPS Server.   
ESP32 acts as a Socket Server.   
Build the firmware using TCP socket.   

Start u-center and connect to ESP32.   
Default port is 5000.   

![u-center-1](https://user-images.githubusercontent.com/6020549/62000222-57aa1900-b10c-11e9-9d7d-aa4d32cdafbe.jpg)
![u-center-2](https://user-images.githubusercontent.com/6020549/62000218-57118280-b10c-11e9-867b-afa20d1caee3.jpg)
![u-center-3](https://user-images.githubusercontent.com/6020549/62000219-57118280-b10c-11e9-84ae-f07103141d4f.JPG)
![u-center-4](https://user-images.githubusercontent.com/6020549/62000220-57118280-b10c-11e9-825f-cf77f2fdcb5b.JPG)
![u-center-5](https://user-images.githubusercontent.com/6020549/62000221-57aa1900-b10c-11e9-833d-1a5a05aa68ae.jpg)

# GPS Server for Bluetooth GPS
Bluetooth GPS is Android Application.   
You can download from [here](https://play.google.com/store/apps/details?id=googoo.android.btgps).   
ESP32 acts as a SPP Acceptor.   
Build the firmware using Bluetooth SPP.   

1.Pair with ESP_SPP_ACCEPTOR.   
2.Open application.   
3.CONNECT.   

![android-1](https://user-images.githubusercontent.com/6020549/122001254-ffa55a00-cdea-11eb-8962-0399f9ce102d.JPG)
![android-2](https://user-images.githubusercontent.com/6020549/122001257-00d68700-cdeb-11eb-84c9-dc4b1428c389.JPG)

# References
Repository with UI is [here](https://github.com/nopnop2002/esp-idf-GPS-View).   
