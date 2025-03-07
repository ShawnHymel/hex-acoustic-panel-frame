#include <Preferences.h>
#include "SPIFFS.h"

Preferences prefs;

void setup() {
  Serial.begin(115200);

  Serial.println("Clearing preferences...");
  prefs.begin("color_correction", false);
  prefs.clear();  // WARNING: This wipes all stored keys!
  prefs.end();
  
  Serial.println("Formatting SPIFFS...");
  if (!SPIFFS.begin(true)) {  // `true` forces format if unformatted
    Serial.println("ERROR: SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS successfully mounted!");
}

void loop() {
  delay(10000);
}
