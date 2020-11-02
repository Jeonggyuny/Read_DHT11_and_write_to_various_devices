# Read_DHT11_data_and_write_to_various_devices

LEDs, Buzzer, and CLCD driver are implemented to operate according to temperature and humidity data received through DHT11. In application, DHT11 periodically reads temperature and humidity data and passes it to LEDs, Buzzer and CLCD.

DHT11 driver was written by referring to driver source code in Linux kernel source code.
(https://github.com/raspberrypi/linux/blob/rpi-4.9.y/drivers/iio/humidity/dht11.c)

Source code of drivers is for Broadcom BCM2837 processor. These drivers have been implemented using 4.9 LTS kernel.

## Block Diagram

![](/img/block_diagram.jpg)

## Circuit Diagram

![](/img/circuit_diagram.jpg)
