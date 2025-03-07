#include <NeoPixelBus.h>

const uint16_t PixelCount = 132;

// LED pin for debugging
const uint8_t debug_led_pin = 12;

// make sure to set this to the correct pins
const uint8_t DotClockPin = 18;
const uint8_t DotDataPin = 23;  

// for software bit bang
NeoPixelBus<DotStarBgrFeature, DotStarMethod> strip(PixelCount, DotClockPin, DotDataPin);

RgbColor color_black(0, 0, 0);
RgbColor color_custom(0xf9, 0x50, 0x04);

void setup()
{
  pinMode(debug_led_pin, OUTPUT);

  strip.Begin();
  strip.ClearTo(color_custom);
  strip.Show();
}

void loop()
{
  // "I'm alive" blink
  digitalWrite(debug_led_pin, HIGH);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  digitalWrite(debug_led_pin, LOW);

  // Sleep
  esp_sleep_enable_timer_wakeup(1000000 * 5);
  esp_light_sleep_start();
}

