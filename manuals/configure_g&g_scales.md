# Initialize G&G Scales for OpenTrickler

To enter the configuration mode, you need to follow below steps:

1. Turn off the scale.

2. Press the CAL button while tuning on the scale (by pressing ON/OFF button)

3. Press CAL button to cycle through settings and TARE button to change values. 

## G&G JJ100B

Configure the scale according to the below table: 

| Variable | Name                        | Recommended Value                     | All Options                                                                               |
| -------- | --------------------------- | ------------------------------------- | ----------------------------------------------------------------------------------------- |
| C1       | Sensitivity                 | 1                                     | 0 (highest)<br>1<br>2 (default)<br>3<br>4 (lowest)                                        |
| C2       | Averaging Window (filter)   | 1                                     | 0 (shortest)<br>1<br>2 (default)<br>3 (longest)                                           |
| C3       | Serial Settings             | 6                                     | 2 (600 baud, default)<br>3 (1200 baud)<br>4 (2400 baud)<br>5 (4800 baud)<br>6 (9600 baud) |
| C4       | Serial Comm Identification  | 33 (press FUNC to +10 and TARE to +1) | 27 (default)                                                                              |
| C5       | Function key (FUNC) mapping | 0                                     | 0 (map to unit conversion)<br>1 (map to print)<br>2 (map to count)                        |
| C6       | Backlight                   | 0                                     | 0 (always on, default)<br>1 (auto)<br>2 (off)                                             |

Press CAL button at C6 menu to save and reboot. 

Reference: http://www.gandg.com.cn/tbc258_upfile/2019216152351337.pdf

## G&G JJ223BF

Configure the scale according to the below table:

| Variable | Name                       | Recommended Value                        | All Options                                                                                                                                                    |
| -------- | -------------------------- | ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1       | Sensitivity                | 1                                        | 0 (highest)<br>1<br>2<br>3<br>4<br>5 (lowest, default)                                                                                                         |
| C2       | Averaging Window (filter)  | 1                                        | 0 (shortest)<br>1<br>2<br>3<br>4 (longest, default)                                                                                                            |
| C3       | Serial Settings            | 6                                        | 0 (9600 baud, stream mode)<br>1 (9600 baud, auto print on stable)<br>2 (600 baud, default)<br>3 (1200 baud)<br>4 (2400 baud)<br>5 (4800 baud)<br>6 (9600 baud) |
| C4       | Serial Comm Identification | 33 (press COUNT to loop through options) | 27 (default)                                                                                                                                                   |
| C5       | Backlight                  | 0                                        | 0 (always on, default)<br>1 (auto)<br>2 (off)                                                                                                                  |
| C6       | Calibration                | 0                                        | 0 (enable internal calibration)<br>1 (disable internal calibration)<br>2 (disable default internal calibration)                                                |

Press CAL button at C6 menu to save and reboot.

Reference: http://www.gandg.com.cn/tbc258_upfile/20192151033464357.pdf


