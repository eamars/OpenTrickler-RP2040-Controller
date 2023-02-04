// 
//            -----
//        5V |10  9 | GND
//        -- | 8  7 | --
//    (DIN)  | 6  5#| (RESET)
//  (LCD_A0) | 4  3 | (LCD_CS)
// (BTN_ENC) | 2  1 | --
//            ------
//             EXP1
//            -----
//        -- |10  9 | --
//   (RESET) | 8  7 | --
//   (MOSI)  | 6  5#| (EN2)
//        -- | 4  3 | (EN1)
//  (LCD_SCK)| 2  1 | --
//            ------
// 
// 
//             EXP2
// For Pico W
// EXP1_3 (CS) <-> PIN22 (SPI0 CS)
// EXP1_4 (A0) <-> PIN26 (GP20)
// EXP2_6 (MOSI) <-> PIN19 (SPI0 TX)
// EXP2_2 (SCK) <-> PIN24 (SPI0 SCK)
// EXP1_5 (RST) <-> PIN27 (GP21)

