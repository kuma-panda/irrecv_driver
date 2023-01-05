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
        echo 'KERNEL=="irrecv[0-9]*", GROUP="root", MODE="0666"' >>  /etc/udev/rules.d/81-my.rules
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
| 0 | pointer to the 32bit buffer<br>If driver has received valid code, it will be copied to this buffer.<br>0x00000000 means 'not received', and 0xFFFFFFFF means 'repeat code received'. | Read received code.<br>After this command was executed, internal received data will be cleared and begin receiving the following code. | 0: successs<br>1: not received<br><0: error |
| 1 | not used | Force clear internal data and restart receiving. | 0: success<br><0: error |

Important notice  
Once driver received valid IR code, the following IRR module's input signal will be ignoread and any code will be never received until clear data using read command or clear command. 
