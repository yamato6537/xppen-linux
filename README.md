# XP-PEN tablet driver

## About

This is a Linux kernel driver for XP-PEN USB tablets.

## Supported tablets

| PID:VID |Model name              |Supported features                 |
|:-------:|:-----------------------|:----------------------------------|
|28BD:0042|XP-PEN Deco01           |2540LPI, 8192PL, EK                |

|ABBR|Meaning         |
|:---|:---------------|
|LPI |lines per inch  |
|PL  |pressure level  |
|TS  |tilt sensitivity|
|EK  |express keys    |

## Build and Install 

### Dependencies

- gcc
- GNU make
- Linux kernel headers
- xf86-input-wacom
- python3

### Build

    $ git clone https://github.com/lambda2111/xppen-linux.git
    $ cd xppen-linux
    $ make
    $ sudo cp 60-xppen.conf /usr/share/X11/xorg.conf.d/

After that, reboot your PC.

### Install

    $ cd xppen-linux
    $ sudo python3 unbind.py
    $ sudo insmod deco.ko

You need to type these commands every boot time.

### Note

You need to rebuild the driver after a Linux kernel update.

### Rebuilding:

    $ cd xppen-linux
    $ make clean
    $ make

## License

GPL (or later)
