[![Build Status](https://travis-ci.org/Shden/mercury236.svg?branch=master)](https://travis-ci.org/Shden/mercury236)

# Mercury/Меркурий 236
Mercury 236 (http://www.incotexcom.ru/m236art.htm) power meter communication utility.

RS485 USB dongle is used to connect to the power meter and to collect grid power measures
including voltage, current, consumption power, counters, cos(f) etc.

## License

This sources is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

You should have received a copy of the GNU Lesser General Public License along with this script; if not, please visit http://www.gnu.org/copyleft/gpl.html for more information.

## Build and use  
```
git clone https://github.com/Shden/mercury236.git
cd mercury236
make

./mercury236 /dev/ttyS0 --help
Usage: mercury236 RS485 [OPTIONS] ...
RS485 address of RS485 dongle (e.g. /dev/ttyUSB0), required
--debug to print extra debug info
--testRun dry run to see output sample, no hardware required
Output formatting:
....
--help prints this screen
```

then try to run with json switch:  
```
./mercury236 /dev/ttyS0 --json  
{
                "U": {
                               "p1": 0.35,
                               "p2": 0.35,
                               "p3": 226.86
                },
                "I": {
                               "p1": 0.00,
                               "p2": 0.00,
                               "p3": 0.39
                },
                "CosF": {
                               "p1": 0.00,
                               "p2": 0.00,
                               "p3": 0.60,
                               "sum": 0.60
                },
                "F": 50.00,
                "A": {
                               "p1": 41943.03,
                               "p2": 41943.03,
                               "p3": 41943.03
                },
                "P": {
                               "p1": 0.00,
                               "p2": 0.00,
                               "p3": 53.45,
                               "sum": 53.45
                },
                "S": {
                               "p1": 0.00,
                               "p2": 0.00,
                               "p3": 89.83,
                               "sum": 89.83
                },
                "PR": {
                               "ap": 120.51
                },
                "PR-day": {
                               "ap": 86.00
                },
                "PR-night": {
                               "ap": 34.51
                },
                "PY": {
                               "ap": 0.00
                },
                "PT": {
                               "ap": 0.04
                }
}
```

## See also

Small port for OpenWrt package here - https://github.com/ZigFisher/Glutinium/tree/master/mercury236.
