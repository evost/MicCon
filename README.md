# MicCon

MicConOS is a virtual machine for Arduino Due and Mega, that can execute files compiled for it and work with hardware, such as SD, TFT, PS/2 keyboard, RTC.

MicConAsm is a translator for MicConOS.

## Requirements
MicConOS requires the following libraries
- [UTFT](http://rinkydinkelectronics.com/library.php?id=51)
- [MemoryFree](https://github.com/mpflaga/Arduino-MemoryFree)
- [PS2Keyboard](https://github.com/PaulStoffregen/PS2Keyboard)
- [RTCLib](https://github.com/adafruit/RTClib)
- Standart libraries from Arduino IDE, such as SD, Wire, etc. 

## Examples
You can compile examples from `/prog` with MicConAsm, copy `*.bin` files into SD and execute it with `io.bin` or `50.bin`

## Info
General description of system you can read in `paradigm.md`.
Assembly language table you can find in `assembly_language.md`.
More details and hardware description on the project page on [Arduino Project Hub](https://create.arduino.cc/projecthub/evost/miccon-vm-and-translator-4a072a)

## License
MicConOS and MicConAsm is open-sourced software licensed under the [GNU GPL v3](http://www.gnu.org/licenses/gpl.html).