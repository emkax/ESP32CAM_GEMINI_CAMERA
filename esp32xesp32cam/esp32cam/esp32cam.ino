// Add these includes at the top of your file if not already present
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <SPI.h>
#include <WebServer.h>
#include "./conf.h"

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// ======================== BUTTON ========================
#define BUTTON_CAPTURE 14

#define API_URL "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key="


//Setting API KEY
#include <Preferences.h>
Preferences prefs;
WebServer server(80);

String gemini_api_key = API_KEY;
String gemini_prompt;

const char* DEFAULT_PROMPT = "describe what is this.";

unsigned long lastCapturePress = 0;
unsigned long lastScrollPress = 0;


int httpResponseCode = 0;


// Helper function to convert markdown to simple text (from your code)
String markdownToSimpleText(const String& md) {
  String text = md;

  int startBold = -1;
  while ((startBold = text.indexOf("**", startBold + 1)) != -1) {
    int endBold = text.indexOf("**", startBold + 2);
    if (endBold == -1) break;

    String boldText = text.substring(startBold + 2, endBold);
    String upperText = boldText;
    upperText.toUpperCase();

    text = text.substring(0, startBold) + "**" + upperText + "**" + text.substring(endBold + 2);
    startBold = startBold + 4 + upperText.length();
  }

  startBold = -1;
  while ((startBold = text.indexOf("__", startBold + 1)) != -1) {
    int endBold = text.indexOf("__", startBold + 2);
    if (endBold == -1) break;

    String boldText = text.substring(startBold + 2, endBold);
    String upperText = boldText;
    upperText.toUpperCase();

    text = text.substring(0, startBold) + "**" + upperText + "**" + text.substring(endBold + 2);
    startBold = startBold + 4 + upperText.length();
  }

  text.replace("\\n", "\n");
  text.replace("\\r\\n", "\n");
  text.replace("*", "");
  text.replace("_", "");
  text.replace("`", "");
  text.replace("[", "");
  text.replace("]", "");
  text.replace("(", "");
  text.replace(")", "");
  text.replace("#", "");
  text.replace(">", "");

  return text;
}

// Simple Base64 encoding utility (from your code)
String base64Encode(const uint8_t* data, size_t len) {
  static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded_string;
  unsigned int i;

  for (i = 0; i < len; i += 3) {
    uint32_t octet_a = i < len ? data[i] : 0;
    uint32_t octet_b = (i + 1) < len ? data[i + 1] : 0;
    uint32_t octet_c = (i + 2) < len ? data[i + 2] : 0;

    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    encoded_string += encoding[(triple >> 18) & 0x3F];
    encoded_string += encoding[(triple >> 12) & 0x3F];
    encoded_string += ((i + 1) < len) ? encoding[(triple >> 6) & 0x3F] : '=';
    encoded_string += ((i + 2) < len) ? encoding[triple & 0x3F] : '=';
  }

  return encoded_string;
}

/**
 * Synchronously requests a description from Gemini API for the given base64 image
 * 
 * @param base64Image The base64-encoded JPEG image string
 * @param prompt The text prompt to send (default: "describe what is this.")
 * @return String containing the Gemini response or error message
 */
String getGeminiResponseSync(const String& base64Image, const String& prompt = "describe what is this.") {
  // Create secure WiFi client
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate verification

  HTTPClient http;

  // Construct the API URL
  String url = API_URL;
  url += gemini_api_key;

  Serial.println("<GEMINI_START>");
  Serial.println("LOGS : using api key = " + gemini_api_key);
  Serial.println("LOGS : using prompt = " + prompt);
  Serial.println("<GEMINI_END>");

  // Begin HTTP connection
  if (!http.begin(client, url)) {
    Serial.println("<GEMINI_START>");
    Serial.println("HTTP begin failed");
    Serial.println("<GEMINI_END>");
    return "ERROR: HTTP begin failed";
  }

  // Set timeout to 30 seconds (Gemini can take time to process)
  http.setTimeout(30000);

  // Construct the JSON payload
  String payload = "{\"contents\":[{\"parts\":[";
  payload += "{\"text\":\"" + prompt + "\"},";
  payload += "{\"inlineData\":{\"mimeType\":\"image/jpeg\",\"data\":\"";
  payload += base64Image;
  payload += "\"}}";
  payload += "]}]}";

  Serial.println("Sending request to Gemini...");

  // Set content type header
  http.addHeader("Content-Type", "application/json");

  // Make the POST request (this blocks until response is received)
  httpResponseCode = http.POST(payload);

  String response = "ERROR: No response received.";

  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    response = http.getString();

    if (httpResponseCode == 200) {  // HTTP_CODE_OK
      // Parse the JSON response to extract the text
      int start = response.indexOf("\"text\": \"");
      if (start != -1) {
        start += 9;
        int end = response.indexOf("\"", start);
        if (end != -1) {
          response = response.substring(start, end);
          // Clean up markdown formatting
          response = markdownToSimpleText(response);
        } else {
          response = "ERROR: Failed to parse Gemini response text (missing end quote).";
        }
      } else {
        response = "ERROR: No 'text' field found in Gemini response.";
      }
    } else {
      response = "ERROR: HTTP error code " + String(httpResponseCode) + ". Response: " + response;
      Serial.println("<GEMINI_START>");
      Serial.println(response);
      Serial.println("<GEMINI_END>");
    }
  } else {
    response = "ERROR: HTTP POST failed: " + http.errorToString(httpResponseCode);
    Serial.println("<GEMINI_START>");
    Serial.println(response);
    Serial.println("<GEMINI_END>");
  }

  // Clean up HTTP connection
  http.end();

  return response;
}

/**
 * Capture an image from camera and get Gemini description - all in one synchronous call
 * 
 * @param prompt Optional custom prompt (default: "describe what is this.")
 * @return String containing the Gemini response or error message
 */
String captureAndAnalyzeSync(const String& prompt = "describe what is this.") {
  // Capture image from camera
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "ERROR: Camera capture failed";
  }

  Serial.println("Image captured, encoding to base64...");

  // Encode to base64
  String base64Image = base64Encode(fb->buf, fb->len);

  // Return the frame buffer to free memory
  esp_camera_fb_return(fb);

  Serial.println("Sending to Gemini...");

  // Get response from Gemini (this will block)
  String result = getGeminiResponseSync(base64Image, prompt);

  return result;
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  if (psramFound()) {
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // --- USE YOUR DESIRED RESOLUTION (QVGA is 320x240) ---
  config.frame_size = FRAMESIZE_VGA;
  // --------------------------------------------------------

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
  s->set_framesize(s,FRAMESIZE_VGA);  // Ensure sensor is also set

  return true;
}

String ssid = "ya";
String password = "michael28";

String globalResult = "";

#define FLASH_LED_PIN 4

void setup() {
  Serial.begin(9600);

  prefs.begin("gemini", false);

  gemini_api_key = prefs.getString("api_key", "");
  gemini_prompt  = prefs.getString("prompt", "");

  if (gemini_api_key == API_KEY || gemini_api_key == "") {
    Serial.println("LOG : Using default API");
  } else {
    Serial.println("LOG : Gemini API key loaded");
    Serial.println(gemini_api_key);
  }

  if (gemini_prompt == "") {
    Serial.println("LOG : Using default prompt");
  } else {
    Serial.println("LOG : Custom prompt loaded");
    Serial.println(gemini_prompt);
  }


  //---------------------------
  //check if it's turning on
  // Attach pin to PWM channel (NEW API)
  // pinMode(FLASH_LED_PIN,INPUT_PULLUP);
  //----------------------

  Serial.println("LOG : Camera starting");

  if (!initCamera()) {
    Serial.println("LOG : Camera Init Failed");
    return;
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("<GEMINI_START>");
  Serial.println("\nLOG : WiFi connected");
  Serial.print("LOG : Camera Ready! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("<GEMINI_END>");

  server.on("/", []() {
    String html =
      "<h2>Gemini Configuration</h2>"
      "<form action='/set' method='POST'>"

      "<label>API Key</label><br>"
      "<input type='text' name='key' style='width:100%'><br><br>"

      "<label>Prompt (optional)</label><br>"
      "<textarea name='prompt' rows='4' style='width:100%'>"
      + gemini_prompt +
      "</textarea><br>"
      "<small>Leave empty to use default prompt.</small><br><br>"

      "<input type='submit' value='Save'>"
      "</form>";

    server.send(200, "text/html", html);
  });

  server.on("/set", HTTP_POST, []() {

    if (server.hasArg("key")) {
      gemini_api_key = server.arg("key");
      gemini_api_key.trim();
      prefs.putString("api_key", gemini_api_key);
    }

    if (server.hasArg("prompt")) {
      gemini_prompt = server.arg("prompt");
      gemini_prompt.trim();
      prefs.putString("prompt", gemini_prompt);
    }

    server.send(200, "text/html",
      "<h3>Saved</h3>"
      "<p>API key and prompt updated.</p>"
      "<p>You may now close this page.</p>"
    );

    Serial.println("LOG : Gemini config updated");
  });

  server.begin();
  Serial.println("LOG : Config web server started");


  pinMode(BUTTON_CAPTURE,INPUT_PULLUP);
  // test_code();
}

// void test_code(){
//   String promptToUse = (gemini_prompt == "") ? DEFAULT_PROMPT : gemini_prompt;
//   globalResult = captureAndAnalyzeSync(promptToUse);
//   Serial.println(globalResult);
// }

int buttonState;
int lastButtonState = HIGH;

unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 50; 

void loop() {
  server.handleClient();
  int reading = digitalRead(BUTTON_CAPTURE);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
    // Serial.println("BUTTON PRESSED");
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        String promptToUse = (gemini_prompt == "") ? DEFAULT_PROMPT : gemini_prompt;
        globalResult = captureAndAnalyzeSync(promptToUse);
        if (globalResult == "" || httpResponseCode != 200) {
            Serial.println("<GEMINI_START>");
            Serial.println("ERROR:");
            Serial.println(globalResult);
            Serial.println("<GEMINI_END>");
        }else{
          Serial.println("<GEMINI_START>");
          Serial.println(globalResult);
          Serial.println("<GEMINI_END>");
        }
      }
    }
  }
  
  lastButtonState = reading;
}