# mercury236
Mercury 236 power meter communication utility.

RS485 USB dongle is used to connect to the power meter and to collect grid power measures
including voltage, current, consumption power, counters, cos(f) etc.

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
