ESP-32 LED Ring App
===================

This is a small application that was intended to help me learn more of
the inner workings of the ESP-32 platform, WS2812B LED controllers, FreeRTOS,
and the CoAP protocol.

The led controller is influenced by [this one on Insentricity](http://www.insentricity.com/a.cl/268/controlling-ws2812-rgb-leds-from-the-esp32).
Unfortunately I couldn't understand the code, so I chose to rewrite it instead.
By default, the application runs a 24 LED ring in strobing rainbow mode.
The LEDs can be controlled using the [CoAP Protocol](http://coap.technology/)
There is an associated iOS application that allows you to control the device.

To Build the App
----------------

* Download and install [ESP-IDF](http://www.espressif.com/sites/default/files/documentation/esp-idf_getting_started_guide_en.pdf)
* Run `make menuconfig` to ensure that your USB device is configured correctly
* Open `main.c` and configure your GPIO port, WiFi SSID, and password
* Run `make flash`

If all goes well, you should see the LED ring cycling through all of the colors of the rainbow.

If you want to further control the lights, you can communicate with the device via the [CoAP Protocol](http://coap.technology/)

If you have libcoap installed on your computer, you can use the `coap-client` command to control the device
* `coap-client coap://your_device/led_ring` gets the current mode
* `coap-client -m put -e '["solid_rainbow"]' coap://your_device/led_ring` for a solid rainbow
* `coap-client -m put -e '["spinning_rainbow"]' coap://your_device/led_ring` for a spinning rainbow
* `coap-client -m put -e '["strobing_rainbow"]' coap://your_device/led_ring` for a strobing rainbow
* `coap-client -m put -e '["solid_dots"]' coap://your_device/led_ring` for a dots
* `coap-client -m put -e '["spinning_dots"]' coap://your_device/led_ring` for a spinning dots
* `coap-client -m put -e '["strobing_dots"]' coap://your_device/led_ring` for a strobing dots (like a strobe light)
* `coap-client -m put -e '["solid_color",64,0,0]' coap://your_device/led_ring` for solid red
* `coap-client -m put -e '["solid_color",0,0,64]' coap://your_device/led_ring` for solid blue
* `coap-client -m put -e '["solid_color",0,0,0]' coap://your_device/led_ring` to turn the LEDs off

The CoAP server is also configured to response to multicast requests, which allows multiple devices to be controlled simultaneously.
This is how the associated iOS app finds devices on the network.