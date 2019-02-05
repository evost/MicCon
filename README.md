# MicCon
![TravisCI](https://travis-ci.org/evost/MicCon.svg?branch=master "TravisCI")

MicConOS is a virtual machine for Arduino Due and Mega, that can execute files compiled for it and work with hardware, such as SD, TFT, PS/2 keyboard, RTC.

MicConAsm is a translator (compiler) for MicConOS.

## Requirements
MicConOS requires the following libraries
- [UTFT](http://rinkydinkelectronics.com/library.php?id=51)
- [PS2Keyboard](https://github.com/PaulStoffregen/PS2Keyboard)
- [timer-api](https://github.com/sadr0b0t/arduino-timer-api)
- [SD](https://github.com/arduino-libraries/SD)
- Standart libraries from Arduino framework (Wire.h, SPI.h, libc, etc.)

## Device Support
- Arduino Mega2560
- Arduino Due
- SD cards (via SD & SPI Library)
- RTC DS3231 (via Wire Library)
- PS/2 Keyboard
- LCD TFT (via UTFT Library)

## Examples
You can compile examples from `/docs/progs` with MicConAsm or `asm`, copy `*.bin` files into SD and execute it with `io.bin` or `50.bin`

## Info
General description of system you can read in `/docs/paradigm.md`.
Assembly language table you can find in `/docs/assembly_language.md`.
More details and hardware description on the project page on [Arduino Project Hub](https://create.arduino.cc/projecthub/evost/miccon-vm-and-translator-4a072a)

## License
MicConOS and MicConAsm is open-sourced software licensed under the [GNU GPL v3](http://www.gnu.org/licenses/gpl.html).
