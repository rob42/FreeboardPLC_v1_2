
NEW: Freeboard interface pcb arrives. See https://www.42.co.nz/freeboard/news/custompcboardprototypearrives.html

If you just want the binary hex files to upload to the arduino mega they are in the Release[Mega_model] directory.

**You can upload these with the installer https://github.com/rob42/freeboard-installer if you dont want to compile code. Windows causes problems with virus/firewall or something, best to use http://russemotto.com/xloader/

You can configure the GPS type, and serial baud rates as follows:

See CONFIG.txt to configure serial baud rates etc if the defaults dont suit.

Develop and Compile 

freeboardPLCv1_2 project rebuilt using Baeyens v2.2 for MUCH easier building - many thanks Jantje!!

To install:
===========

* Download the Arduino IDE Arduino 1.5.7 ( http://arduino.cc/en/Main/Software#toc3 )
* Download http://www.baeyens.it/eclipse/download/product/ - (you will need the current one for your environment)
* Unpack and install the Arduino IDE
* Unpack and install the Eclipse IDE for baeyens
* Configure eclipse to use the Arduino IDE (Windows>Preferences>Arduino)

(git clone this project, and make a local repository on your PC.)

In Eclipse use the git integration to extract a new project:
* File>Import>Project from Git>etc
* Open, clean and build your new project

Notes:
======

This is  targetted at the Freeboard Interface board. It should be the same as the FreeboardPLC project, but there are some code changes and lib locations to suit the new format. 

The update to 1.5.7 Arduino codebase required a change to the Seatalk 9 bit extensions. These are untested, so if you have seatalk errors let me know.

Ive commited my .settings and .cproject files, so your project should be fully set up. But the .settings may cause your project to look for my dir structure, which will probably cause problems. In this case you will need to check the following:

* In Project>Properties>Arduino set the boards.txt file, and select the Mega processor and the type you have (1280/2560)
* Copy the two HardwareSerial.* files from the arduinoMods dir over the top of the same ones you will find in your "${workspace_loc:/arduino/core directory.
* In Project>Properties>C++ Comiler>Settings>Include folders:
```
  "${workspace_loc:/FreeboardMega/arduino/core}"
  "${workspace_loc:/FreeboardMega/arduino/variant}"
  "${workspace_loc:/${ProjName}/lib/AltSoftSerial}"
  "${workspace_loc:/${ProjName}/lib/AverageList}"
  "${workspace_loc:/${ProjName}/lib/EEPROM}"
  "${workspace_loc:/${ProjName}/lib/FlexiTimer2}"
  "${workspace_loc:/${ProjName}/lib/JsonStream}"
  "${workspace_loc:/${ProjName}/lib/Kangaroo}"
  "${workspace_loc:/${ProjName}/lib/MemoryFree}"
  "${workspace_loc:/${ProjName}/lib/MultiSerial}"
  "${workspace_loc:/${ProjName}/lib/NMEA}"
  "${workspace_loc:/${ProjName}/lib/PID_v1}"
  "${workspace_loc:/${ProjName}/lib/PString}"
  "${workspace_loc:/${ProjName}/lib/SPI}"

```

***check they really are there!

* In Project>Properties>C Compiler>Settings>Include folders:
```
  "${workspace_loc:/freeboardMega/arduino/core}"
  "${workspace_loc:/freeboardMega/arduino/variant}"
  

Current build is done on a Kubuntu 14.10 linux laptop. It should build on any OS with a reasonably current version of eclipse IDE.


I found eclipse would not upload for me, so I use the commandline to load arduino on USB0. Install the arduino environment, and upload any simple example sketch. In the console window you will see the commandline used. Copy and adjust.

For me, on linux:

FREEBOARD_HOME= ~/gitrep/FreeboardPLC_v1_2
ARDUINO_HOME=/home/robert/dev/arduino-1.0.2/

cd $FREEBOARD_HOME/Release1280
~/gitrep/Freeboard_v1_2PLC/Release1280$ $ARDUINO_HOME/hardware/tools/avrdude -patmega1280 -carduino -P/dev/ttyUSB0 -b57600 -D -v -v -v -v -Uflash:w:FreeboardPLC_v1_2.hex:a -C$ARDUINO_HOME/hardware/tools/avrdude.conf


#Arduino IDE 1.6.5 to mega2560
/home/robert/dev/arduino-1.6.5/hardware/tools/avr/bin/avrdude -C/home/robert/dev/arduino-1.6.5/hardware/tools/avr/etc/avrdude.conf -v -patmega2560 -cwiring -P/dev/ttyACM0 -b115200 -D -Uflash:w:FreeboardPLC_v1_2.hex:i




