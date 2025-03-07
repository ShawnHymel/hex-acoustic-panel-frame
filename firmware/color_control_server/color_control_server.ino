 /**
 * Simple Web Server and LED controller
 * 
 * Hardware (if using PixelBlaze)
 * ---
 *  - Connect USB-to-Serial adapter (3.3V) to GND, TX, and RX of the PixelBlaze
 *  - Connect button to GND with 10k pullup to IO0
 *  - Connect button to GND with no pullup to RST
 *
 * Dependencies
 * ---
 *  - Install FastLED from the library manager
 *  - Install the https://github.com/espx-cz/arduino-spiffs-upload/releases
 *      - Download latest .vsix under releases
 *      - Copy .vsix file to .arduinoIDE/plugins/ in your Arduino installation
 *      - Restart Arduino IDE
 *
 * Flashing
 * ---
 *  - Replace the ssid and password with your network credentials
 *  - Change num_pixels to the number of LEDs in your LED strip
 *  - Select "ESP32 Dev Module" as your board (if using a PixelBlaze v3)
 *  - Hold IO0 button, press and release RST button, release IO0 button
 *  - Upload data folder with SPIFFS
 *      - Press shift+ctrl+p to bring up the command palette
 *      - Search for and run "Upload SPIFFS to Pico/ESP8266/ESP32"
 *  - Hold IO0 button, press and release RST button, release IO0 button
 *  - Build and upload firmware to ESP32
 *
 * Using
 * ---
 *  - Navigate to hexarray.local on another device in your local network
 *  - Adjust options or set LED values
 *  - Unless CLEAR_PREFS is set to 1, LED values are retained through reboots
 * 
 * License
 * ---
 * Zero-Clause BSD: https://opensource.org/license/0bsd
 * 
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include "SPIFFS.h"
#include "FastLED.h"

#define DEBUG 1       // Print out extra debugging info to the serial port
#define CLEAR_PREFS 0 // WARNING: Will delete all saved preferences every reboot!

// Settings
const char *ssid = "EsperNet";
const char *password = "EsperNetKey";
const char *mdns_name = "hexarray";         // FQDN is <mdns_name>.local
const int server_port = 80;
const uint16_t num_pixels = 132;

// Pins
const int led_pin = 12;
const int8_t dotstar_clk_pin = 18;
const int8_t dotstar_dat_pin = 23;

// Globals
WebServer server(server_port);
CRGB strip[num_pixels];
Preferences prefs;

/******************************************************************************
 * Functions
 */

// Check validity of string value
bool isValidNumber(String value) {

  // Trim whitespace
  value.trim();

  // Check for hex values
  if (value.startsWith("#")) {
    for (int i = 1; i < value.length(); i++) {
      if (!isxdigit(value[i])) return false;
    }
    return true;
  } else if (value.startsWith("0x") || value.startsWith("0X")) {
    for (int i = 2; i < value.length(); i++) {
      if (!isxdigit(value[i])) return false;
    }
    return true;

  // Check for decimal values
  } else {
    for (char c : value) {
      if (!isdigit(c)) return false;
    }
    return true;
  }
}

// Get unsigned 32-bit number from string
uint32_t getNumber(String value) {
  uint32_t val;

  // Convert string to integer
  if (value.startsWith("#")) {
    val = strtoul(value.c_str() + 1, NULL, 16); 
  } else if (value.startsWith("0x") || value.startsWith("0X")) {
    val = strtoul(value.c_str(), NULL, 16); // Convert hex string
  } else {
    val = strtoul(value.c_str(), NULL, 10); // Convert decimal string
  }

  return val;
}

// Separate RGB color into its components
void separateRgb(uint32_t hexColor, uint8_t &rOut, uint8_t &gOut, uint8_t &bOut) {
  rOut = (hexColor >> 16) & 0xFF;
  gOut = (hexColor >> 8) & 0xFF;
  bOut = hexColor & 0xFF;
}

// Send the right file to the client (if it exists)
bool serveFile(String path) {

#if DEBUG
  Serial.println("Reading file: " + path);
#endif

  // If path is a folder, use index.html in that folder
  if (path.endsWith("/")) path += "index.html";

  // Check if the file exists, read it, and send it out
  if (SPIFFS.exists(path)) {
    
    // Read file into String
    File file = SPIFFS.open(path, "r");
    String html = file.readString();
    file.close();

    // If serving index.html, replace the color correction placeholders
    if (path == "/index.html") {

      uint8_t corr_r, corr_g, corr_b;
      bool ret;

      // Load saved color correction data
      ret = prefs.begin("color_corr", true);
      if (ret) {
        corr_r = prefs.getUChar("corr_r", 0xFF);
        corr_g = prefs.getUChar("corr_g", 0xFF);
        corr_b = prefs.getUChar("corr_b", 0xFF);
      } else {
        Serial.println("Error: Could not open Preferences. Using defaults for color correction");
        corr_r = 0xFF;
        corr_g = 0xFF;
        corr_b = 0xFF;
      }
      prefs.end();

      // Replace values in HTML string
      html.replace("%CORR_RED%", String(corr_r));
      html.replace("%CORR_GREEN%", String(corr_g));
      html.replace("%CORR_BLUE%", String(corr_b));
    }
    
    // Send HTML to client
    server.send(200, "text/html", html);

    return true;
  }

  return false;
}

/******************************************************************************
 * Web Server Callbacks
 */


// Main web page to be served
void handleRoot() {

  String html;
  bool ret;

  // Toggle LED to show activity
  digitalWrite(led_pin, HIGH);

  // Print message
#if DEBUG
  Serial.println("Root web page requested. Sending /index.html.");
#endif

  // Send /index.html
  ret = serveFile("/");
  if (!ret) {
    server.send(404, "text/plain", "File Not Found");
#if DEBUG
    Serial.println("Error: Could not load HTML file");
#endif
  }

  // Turn off LED
  digitalWrite(led_pin, LOW);
}

// Apply and store collor correction
void handleColorCorrection() {

  // LED on to show activity
  digitalWrite(led_pin, HIGH);

  // Make sure we have actual data
  if (server.hasArg("correctionRed") && 
      server.hasArg("correctionRed") && 
      server.hasArg("correctionRed")) {

      uint8_t corr_r, corr_g, corr_b;
      bool ret;

      // Get arguments
      String corr_r_str = server.arg("correctionRed");
      String corr_g_str = server.arg("correctionGreen");
      String corr_b_str = server.arg("correctionBlue");

      // Remove whitespace
      corr_r_str.trim();
      corr_g_str.trim();
      corr_b_str.trim();

      // Validate values
      if (isValidNumber(corr_r_str) && 
          isValidNumber(corr_g_str) &&
          isValidNumber(corr_b_str)) {
          
        // Parse values and clamp values
        corr_r = (uint8_t)(constrain(getNumber(corr_r_str), 0, 255));
        corr_g = (uint8_t)(constrain(getNumber(corr_g_str), 0, 255));
        corr_b = (uint8_t)(constrain(getNumber(corr_b_str), 0, 255));

        // Set correction values
        FastLED.setCorrection(CRGB(corr_r, corr_g, corr_b));

        // Save color correction
        ret = prefs.begin("color_corr", false);
        if (ret) {
          prefs.putUChar("corr_r", corr_r);
          prefs.putUChar("corr_g", corr_g);
          prefs.putUChar("corr_b", corr_b);
        } else {
          Serial.println("Error: Could not open NVS for saving color correction values");
        }
        prefs.end();

        // Update LED strip
        FastLED.show();

        // Notify client that we processed the values
        server.send(200, "text/plain", "Color correction values applied.");
#if DEBUG
        Serial.print("Color correction applied: 0x");
        Serial.println((uint32_t)((corr_r << 16) | (corr_g << 8) | (corr_b)), HEX);
#endif

      } else {
        Serial.println("Error: Could not parse color correction values");
        server.send(200, 
                    "text/plain", 
                    "Error: One or more values are invalid. " \
                    "Please enter only decimal or hex values.");
      }

  // Could not process form
  } else {
    Serial.println("Error: Could not process color correction form");
    server.send(200, 
                "text/plain", 
                "Error: Could not process color correction form");
  }

  // Turn LED off
  digitalWrite(led_pin, LOW);
}

// Set raw pixel values
void handleSolidColor() {

  // LED on to show activity
  digitalWrite(led_pin, HIGH);

  // Read values from submission
  if (server.hasArg("solidColor")) {

    String color_str = server.arg("solidColor");
    int color_correction;
    int pos = 0;
    uint32_t idx = 0;
    uint32_t color_val = 0;
    uint32_t pixels[num_pixels];
    bool ret;

    // Validate number
    if (isValidNumber(color_str)) {

      // Get RGB value from string
      color_val = getNumber(color_str);

      // Update LED values
      FastLED.showColor(CRGB((color_val & 0xFFFFFF)));

      // Update LED values
      for (int i = 0; i < num_pixels; i++) {
        pixels[i] = color_val;
        strip[i] = CRGB(color_val & 0xFFFFFF);
      }

      // Save values in NVS
      ret = prefs.begin("pixels", false);
      if (ret) {
        prefs.putBytes("color_vals", pixels, num_pixels * sizeof(uint32_t));
#if DEBUG
        Serial.print("Saved ");
        Serial.print(num_pixels * sizeof(uint32_t));
        Serial.print(" bytes of LED color data all to 0x");
        Serial.println(color_val, HEX);
#endif
      } else {
        Serial.println("Error: Could not open preferences to save pixel values");
      }
      prefs.end();

      // Display LEDs
      FastLED.show();

      // Notify client that we processed the value
      server.send(200, "text/plain", "LEDs updated successfully!");

    // Number is not valid, don't update LEDs
    } else {
      Serial.println("Invalid values found. LEDs not updated.");
      server.send(200, 
                  "text/plain", 
                  "Error: Invalid value. Please enter only decimal or hex.");
    }

  // Form submission did not have valid arguments
  } else {
    Serial.println("Error: Form not found");
    server.send(200, "text/plain", "Error: Could not process form");
  }

  // Turn LED off
  digitalWrite(led_pin, LOW);
}

// Calculate and set gradient pattern
void handleGradient() {

  // LED on to show activity
  digitalWrite(led_pin, HIGH);

  // Make sure we have actual data
  if (server.hasArg("gradientLedsPerSegment") && 
      server.hasArg("gradientStartColor") &&
      server.hasArg("gradientMiddleColor") && 
      server.hasArg("gradientEndColor")) {

      uint32_t pixels[num_pixels];
      uint32_t leds_per_seg = 0;
      uint32_t num_segs = 0;
      uint32_t start_color = 0;
      uint32_t mid_color = 0;
      uint32_t end_color = 0;
      CHSV start_hsv;
      CHSV mid_hsv;
      CHSV end_hsv;
      float ratio;
      uint8_t hue;
      uint32_t idx;
      bool ret;

      // Get values in string form
      String leds_per_seg_str = server.arg("gradientLedsPerSegment");
      String start_color_str = server.arg("gradientStartColor");
      String mid_color_str = server.arg("gradientMiddleColor");
      String end_color_str = server.arg("gradientEndColor");

      // Validate numbers
      if  (isValidNumber(leds_per_seg_str) &&
           isValidNumber(start_color_str) &&
           isValidNumber(mid_color_str) &&
           isValidNumber(end_color_str)) {

        // Convert strings to numbers
        leds_per_seg = getNumber(leds_per_seg_str);
        start_color = getNumber(start_color_str);
        mid_color = getNumber(mid_color_str);
        end_color = getNumber(end_color_str);

        // Make sure we have a valid number of LEDs
        if (leds_per_seg > num_pixels) {
          Serial.print("Error: LEDs per segment larger than total LEDs (");
          Serial.print(num_pixels);
          Serial.println(")");
          server.send(200, "text/plain", "Error: LEDs per segment larger than total LEDs");
          return;
        }

        //Calculate number of discrete segments (with an extra for any remainder)
        num_segs = (num_pixels + leds_per_seg - 1) / leds_per_seg;

        // Calculate gradient
        fill_gradient_RGB(strip, 
                          num_segs, 
                          CRGB(start_color & 0xFFFFFF),
                          CRGB(mid_color & 0xFFFFFF),
                          CRGB(end_color & 0xFFFFFF));

        // Fill out pixel values based on segments
        idx = 0;
        for (int i = 0; i < num_segs; i++) {
#if DEBUG
          Serial.print("Segment ");
          Serial.print(i);
          Serial.print(" val: 0x");
          Serial.println((strip[i].as_uint32_t() & 0xFFFFFF), HEX);
#endif
          for (int j = 0; j < leds_per_seg; j++) {
            if (idx < num_pixels) {
              pixels[idx] = strip[i].as_uint32_t() & 0xFFFFFF;
              idx++;
            }
          }
        }

        // Update LED values
        for (int i = 0; i < num_pixels; i++) {
          strip[i] = CRGB(pixels[i] & 0xFFFFFF);
        }

        // Save values in NVS
        ret = prefs.begin("pixels", false);
        if (ret) {
          prefs.putBytes("color_vals", pixels, num_pixels * sizeof(uint32_t));
  #if DEBUG
          Serial.print("Saved ");
          Serial.print(num_pixels * sizeof(uint32_t));
          Serial.println(" bytes of LED color data");
  #endif
        } else {
          Serial.println("Error: Could not open preferences to save pixel values");
        }
        prefs.end();

        // Display LEDs
        FastLED.show();
        
        // Notify client that we processed the value
        server.send(200, "text/plain", "LEDs updated successfully!");
      
      // Invalid values found in form submission
      } else {
        Serial.println("Invalid values found. LEDs not updated.");
        server.send(200, 
                    "text/plain", 
                    "Error: One or more values are invalid. " \
                    "Please enter only decimal or hex values.");
      }

  // Form submission did not have valid arguments
  } else {
    Serial.println("Error: Form not found");
    server.send(200, "text/plain", "Error: Could not process form");
  }

  // Turn LED off
  digitalWrite(led_pin, LOW);
}

// Set raw pixel values
void handleSetPixels() {

  // LED on to show activity
  digitalWrite(led_pin, HIGH);

  // Read values from submission
  if (server.hasArg("pixelValues")) {

    String values = server.arg("pixelValues");
    int pos = 0;
    uint32_t idx = 0;
    bool valid = true;
    String token;
    uint32_t pixels[num_pixels];
    uint32_t color_val = 0;
    uint8_t r, g, b;
    bool ret;

    // Clear LEDs
    memset(pixels, 0, sizeof(pixels));

    // Parse input (comma is delimiter)
    while ((pos = values.indexOf(',')) != -1) {

      // Pop off first value and remove whitespace
      token = values.substring(0, pos);
      values.remove(0, pos + 1);
      token.trim();

      // Validate number
      if (!isValidNumber(token)) {
        valid = false;
        break;
      }

      // Get RGB values from string and save to array
      color_val = getNumber(token);
      pixels[idx] = color_val;

      // Increment counter
      idx++;
      if (idx >= num_pixels) {
        break;
      }
    }

    // Get value after last comma
    if (valid && isValidNumber(values)) {

      // Get RGB values from string and save to array
      values.trim();
      color_val = getNumber(values);
      pixels[idx] = color_val;

      // Notify client that we processed the values
      server.send(200, "text/plain", "LEDs updated successfully!");

    // Notify client that there was an error in parsing the values
    } else {
      valid = false;
    }

    // Update LEDs and save values
    if (valid) {

      // Update LED values
      for (uint32_t i = 0; i <= idx; i++) {
        strip[i] = CRGB(pixels[i] & 0xFFFFFF);
      }

      // Save values in NVS
      ret = prefs.begin("pixels", false);
      if (ret) {
        prefs.putBytes("color_vals", pixels, num_pixels * sizeof(uint32_t));
#if DEBUG
        Serial.print("Saved ");
        Serial.print(num_pixels * sizeof(uint32_t));
        Serial.println(" bytes of LED color data");
#endif
      } else {
        Serial.println("Error: Could not open preferences to save pixel values");
      }
      prefs.end();

      // Display LEDs
      FastLED.show();

    // Invalid values found in form submission
    } else {
      Serial.println("Invalid values found. LEDs not updated.");
      server.send(200, 
                  "text/plain", 
                  "Error: One or more values are invalid. " \
                  "Please enter only decimal or hex values.");
    }

  // Form submission did not have valid arguments
  } else {
    Serial.println("Error: Form not found");
    server.send(200, "text/plain", "Error: Could not process form");
  }

  // Turn LED off
  digitalWrite(led_pin, LOW);
}

// Page not found
void handleNotFound() {
  digitalWrite(led_pin, HIGH);
  server.send(404, "text/plain", "404: Not Found");
  digitalWrite(led_pin, LOW);
}

/******************************************************************************
 * Main
 */

void setup() {

  uint8_t corr_r, corr_g, corr_b;
  uint32_t pixels[num_pixels];
  uint8_t r, g, b;
  bool ret;
  size_t len = 0;
  size_t bytes_read = 0;

  // Initialize LED
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  // Pour a bowl of serial
  Serial.begin(115200);

  // Initialize LED strip
  FastLED.addLeds<SK9822, dotstar_dat_pin, dotstar_clk_pin, BGR>(strip, num_pixels);

  // Optionally clear preferences
#if CLEAR_PREFS
  ret = prefs.begin("color_corr", false);
  if (ret) {
    prefs.clear();
    Serial.println("NVS cleared");
  } else {
    Serial.println("Error: Could not open Preferences for clearing");
  }
  prefs.end();
#endif

  // Load saved color correction data
  ret = prefs.begin("color_corr", true);
  if (ret) {
    corr_r = prefs.getUChar("corr_r", 0xFF);
    corr_g = prefs.getUChar("corr_g", 0xFF);
    corr_b = prefs.getUChar("corr_b", 0xFF);
  } else {
    Serial.println("Error: Could not open Preferences. Using defaults for color correction");
    corr_r = 0xFF;
    corr_g = 0xFF;
    corr_b = 0xFF;
  }
  prefs.end();

  // Set color correction values
  FastLED.setCorrection(CRGB(corr_r, corr_g, corr_b));
#if DEBUG
  Serial.print("Color correction applied: 0x");
  Serial.println((uint32_t)((corr_r << 16) | (corr_g << 8) | (corr_b)), HEX);
#endif

  // Load saved color values
  ret = prefs.begin("pixels", true);
  if (ret) {
    len = prefs.getBytesLength("color_vals");
    if (len > 0) {
      bytes_read = prefs.getBytes("color_vals", pixels, len);
    }
  }
  prefs.end();

  // Draw saved color values
  if (bytes_read > 0) {
    for (int i = 0; i < (bytes_read / sizeof(uint32_t)); i++) {
      strip[i] = CRGB(pixels[i] & 0xFFFFFF);
    }
#if DEBUG
    Serial.print("Loaded ");
    Serial.print(bytes_read);
    Serial.println(" bytes of LED color data");
#endif
  } else {
#if DEBUG
    Serial.println("No saved LED values found. Clearing strip.");
#endif
    FastLED.clear();
  }
  FastLED.show();

  // Initialize and mount SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error: SPIFFS failed to mount");
    return;
  }
#if DEBUG
  Serial.println("SPIFFS mounted");
#endif

  // Inititalize WiFi in STA mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Show connection details
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Inititalize mDNS
  if (MDNS.begin(mdns_name)) {
    Serial.println("MDNS responder started");
  }

  // Assign callbacks
  server.on("/", handleRoot);
  server.on("/color-correction", HTTP_POST, handleColorCorrection);
  server.on("/solid", HTTP_POST, handleSolidColor);
  server.on("/gradient", HTTP_POST, handleGradient);
  server.on("/pixels", HTTP_POST, handleSetPixels);
  server.onNotFound(handleNotFound);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
