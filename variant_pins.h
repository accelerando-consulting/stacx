#pragma once

#if defined(ARDUINO_ACCELERANDO_CORINDA)
#undef LED_BUILTIN 
#define HELLO_PIXEL 0
#define PIXEL_COUNT 1
#define PIXEL_PIN 9
#define A0 0
#define D0 10
#define D5 5
#define D6 6
#define D7 7
#define D8 9
#define D1 1
#define D2 2
#define D3 3
#define D4 4

#undef SCK
#define SCK 5
#undef MISO
#define MISO 6
#undef MOSI
#define MOSI 7
#undef SS
#define SS 8

#elif defined(ARDUINO_TTGO_T_OI_PLUS_DEV)
#define LED_BUILTIN 3
#define HELLO_PIN LED_BUILTIN
#define A0 2
#define D0 4
#define D5 5
#define D6 6
#define D7 7
#define D8 10
#define D1 19
#define D2 18
#define D3 9
#define D4 8
#elif defined(ARDUINO_TTGO_T7_V13_Mini32)
#define LED_BUILTIN 22
#define A0 36
#define D0 26
#define D5 18
#define D6 19
#define D7 23
#define D8  5

#define SCL 22
#define D1 22
#define SDA 21
#define D2 21
#define D3 17
#define D4 16
#endif


