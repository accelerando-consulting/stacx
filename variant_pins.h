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

#elif defined(ARDUINO_NODEMCU_C3)

#define EXTERNAL_NUM_INTERRUPTS 22
#define NUM_DIGITAL_PINS        22
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

// User LEDs are also connected to USB D- and D+
static const uint8_t LED_WARM = 18;
static const uint8_t LED_COLD = 19;

// RGB LED 
static const uint8_t LED_RED = 3;
static const uint8_t LED_GREEN = 4;
static const uint8_t LED_BLUE = 5;

static const uint8_t LED_BUILTIN = LED_WARM;

// Standard ESP32-C3 GPIOs
static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS    = 7;
static const uint8_t MOSI  = 6;
static const uint8_t MISO  = 5;
static const uint8_t SCK   = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

#endif


