// ##################################################################################
// DRIVER CONFIGURATION
// ##################################################################################
// #define ILI9341_DRIVER
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_INVERSION_ON
#define ST7789_240x320
#define TFT_INIT_DELAY 10

// ##################################################################################
// PIN CONFIGURATION FOR ESP32
// ##################################################################################
// These match your specific wiring:
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin

// ##################################################################################
// FONT SELECTION
// ##################################################################################
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font
#define LOAD_FONT8  // Font 8. Large 75 pixel font
#define LOAD_GFXFF  // FreeFonts
#define SMOOTH_FONT

// ##################################################################################
// SPI SETTINGS
// ##################################################################################
// 27MHz is a stable speed for ILI9341. 
// If audio still glitches, you can try lowering this to 20000000
#define SPI_FREQUENCY  20000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  20000000