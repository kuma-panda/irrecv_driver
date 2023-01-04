# irrecv_driver

- Linux device driver for receiving irremote code.  
    Only **NEC format** available. 

- Designed for Raspberry Pi 1, 2, 3, 4, Zero. (Pico is not supported)

## Hardware

- Raspberry Pi (tested: ZeroW, 3A)
- IR receiver module (tested: VS838)

## Wiring

| VS838 | Pi |
|:-:|:-:|
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO17 (Pin #11) |

## Compile & build

- Upgrade all packages.

        sudo apt update
        sudo apt upgrade

- Install kernel headers.

        sudo apt-get install raspberrypi-kernel-headers

- Make

        cd irrecv_driver
        make

- Create udev rule for this driver  

        sudo -i
        echo 'KERNEL=="mydevice[0-9]*", GROUP="root", MODE="0666"' >>  /etc/udev/rules.d/81-my.rules
        exit

## Install

    cd irrecv_driver
    sudo insmode irrecv.ko

After that, you can see /dev/irrecv0.

## Uninstall

    sudo rmmod irrecv.ko

## Usage

See sample.cpp  
To build sample:

    g++ -o irr_sample sample.cpp

This driver provides only two ioctl interface.

| Command | Arguments | Description | Return value |
| :-- | :-- | :-- | :-- |
| 0 | pointer to the 32bit buffer | read received code | 0: successs<br>1: not received<br><0: error |
| 1 | not used | clear and restart receiving | 0: success<br><0: error |

