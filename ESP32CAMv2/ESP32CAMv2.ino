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

#include <JPEGDEC.h>

JPEGDEC jpeg;

// // TFT Display Pins
// #define TFT_CS   2
// #define TFT_DC   15
// #define TFT_RST  12
// #define TFT_SCLK 14
// #define TFT_MOSI 13

#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 12  // software reset
#define TFT_SCLK 14
#define TFT_MOSI 13

// Button Pins
// #define BTN_SEQ  15
// #define BTN_SCROLL   12

// ======================== DISPLAY SETUP ========================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2;

// Callback function for JPEGDEC to draw decoded image
int JPEGDraw(JPEGDRAW* pDraw) {
  // pDraw->x, pDraw->y = position of this block
  // pDraw->iWidth, pDraw->iHeight = size of this block
  // pDraw->pPixels = RGB565 pixel data

  // Draw the block to TFT
  tft.drawRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);

  return 1;  // Continue decoding
}


#include "dejavu_sans.h"
extern const uint8_t dejavu_sans[];

int marginLeft = 8;
int marginTop = 8;
int virtualWidth = 160 - 16;
int virtualHeight = 80 - 16;
int scrollOffset = 0;
int scrollStep = 16;

// ======================== STATE MACHINE ========================
#define STATE_PREVIEW 0
#define STATE_TEXT 1


unsigned long lastCapturePress = 0;
unsigned long lastScrollPress = 0;

const unsigned long debounceDelay = 200;
int video_state = STATE_PREVIEW;

// Your Gemini API key
const char* gemini_api_key = API_KEY;  // Replace with your actual key

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
  String url = "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=";
  url += gemini_api_key;
  // url += "1";  // make api key invalid

  // Begin HTTP connection
  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
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
  int httpResponseCode = http.POST(payload);

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
      Serial.println(response);
    }
  } else {
    response = "ERROR: HTTP POST failed: " + http.errorToString(httpResponseCode);
    Serial.println(response);
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
  config.frame_size = FRAMESIZE_240X240;
  // --------------------------------------------------------

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
  s->set_framesize(s, FRAMESIZE_240X240);  // Ensure sensor is also set

  return true;
}

String ssid = "ya";
String password = "michael28";


// ============ DISPLAY LIVE PREVIEW ============
/**
 * Display continuous camera preview on TFT (call repeatedly in loop)
 * 
 * @param fps Target frames per second (default 10)
 */
void displayCameraPreview(uint8_t fps = 10) {
  static unsigned long lastFrameTime = 0;
  unsigned long frameDelay = 1000 / fps;

  if (millis() - lastFrameTime >= frameDelay) {
    lastFrameTime = millis();

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      if (jpeg.openRAM(fb->buf, fb->len, JPEGDraw)) {
        jpeg.setPixelType(RGB565_BIG_ENDIAN);
        jpeg.decode(0, 0, 1);  // Scale 1/2
        jpeg.close();
      }
      esp_camera_fb_return(fb);
    }
  }
}


// ============ EXAMPLE USAGE ============


String toSuperscript(String s) {
  if (s.length() == 0) return "";
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '9') {
    switch (s[0]) {
      case '0': return "⁰";
      case '1': return "¹";
      case '2': return "²";
      case '3': return "³";
      case '4': return "⁴";
      case '5': return "⁵";
      case '6': return "⁶";
      case '7': return "⁷";
      case '8': return "⁸";
      case '9': return "⁹";
    }
  }
  return "^(" + s + ")";
}

String toSubscript(String s) {
  if (s.length() == 0) return "";
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '9') {
    switch (s[0]) {
      case '0': return "₀";
      case '1': return "₁";
      case '2': return "₂";
      case '3': return "₃";
      case '4': return "₄";
      case '5': return "₅";
      case '6': return "₆";
      case '7': return "₇";
      case '8': return "₈";
      case '9': return "₉";
    }
  }
  return "_(" + s + ")";
}

String processLatex(const String& t);

String extractBraced(const String& text, int& pos) {
  if (pos >= text.length() || text[pos] != '{') return "";

  pos++;
  String content = "";
  int braceDepth = 1;

  while (pos < text.length() && braceDepth > 0) {
    char c = text[pos];
    if (c == '{') {
      braceDepth++;
      content += c;
    } else if (c == '}') {
      braceDepth--;
      if (braceDepth > 0) content += c;
    } else {
      content += c;
    }
    pos++;
  }

  return content;
}

String getLatexSymbol(const String& cmd) {
  // Calculus & analysis - longest first
  if (cmd == "limsup") return "limsup";
  if (cmd == "liminf") return "liminf";
  if (cmd == "infty") return "∞";
  if (cmd == "partial") return "∂";
  if (cmd == "nabla") return "∇";
  if (cmd == "iiint") return "∭";
  if (cmd == "iint") return "∬";
  if (cmd == "oint") return "∮";
  if (cmd == "int") return "∫";
  if (cmd == "sum") return "Σ";
  if (cmd == "prod") return "Π";
  if (cmd == "lim") return "lim";
  if (cmd == "sup") return "sup";
  if (cmd == "inf") return "inf";
  if (cmd == "max") return "max";
  if (cmd == "min") return "min";

  // Greek letters - variants first
  if (cmd == "varepsilon") return "ε";
  if (cmd == "vartheta") return "ϑ";
  if (cmd == "varpi") return "ϖ";
  if (cmd == "varrho") return "ϱ";
  if (cmd == "varsigma") return "ς";
  if (cmd == "varphi") return "ϕ";
  if (cmd == "epsilon") return "ε";
  if (cmd == "alpha") return "α";
  if (cmd == "beta") return "β";
  if (cmd == "gamma") return "γ";
  if (cmd == "delta") return "δ";
  if (cmd == "zeta") return "ζ";
  if (cmd == "eta") return "η";
  if (cmd == "theta") return "θ";
  if (cmd == "iota") return "ι";
  if (cmd == "kappa") return "κ";
  if (cmd == "lambda") return "λ";
  if (cmd == "mu") return "μ";
  if (cmd == "nu") return "ν";
  if (cmd == "xi") return "ξ";
  if (cmd == "pi") return "π";
  if (cmd == "rho") return "ρ";
  if (cmd == "sigma") return "σ";
  if (cmd == "tau") return "τ";
  if (cmd == "upsilon") return "υ";
  if (cmd == "phi") return "φ";
  if (cmd == "chi") return "χ";
  if (cmd == "psi") return "ψ";
  if (cmd == "omega") return "ω";

  // Greek uppercase
  if (cmd == "Gamma") return "Γ";
  if (cmd == "Delta") return "Δ";
  if (cmd == "Theta") return "Θ";
  if (cmd == "Lambda") return "Λ";
  if (cmd == "Xi") return "Ξ";
  if (cmd == "Pi") return "Π";
  if (cmd == "Sigma") return "Σ";
  if (cmd == "Upsilon") return "Υ";
  if (cmd == "Phi") return "Φ";
  if (cmd == "Psi") return "Ψ";
  if (cmd == "Omega") return "Ω";

  // Math operators
  if (cmd == "pm") return "±";
  if (cmd == "mp") return "∓";
  if (cmd == "times") return "×";
  if (cmd == "div") return "÷";
  if (cmd == "cdot") return "·";
  if (cmd == "ast") return "*";
  if (cmd == "star") return "⋆";
  if (cmd == "circ") return "∘";
  if (cmd == "bullet") return "•";
  if (cmd == "oplus") return "⊕";
  if (cmd == "ominus") return "⊖";
  if (cmd == "otimes") return "⊗";

  // Relations - longest first
  if (cmd == "subseteq") return "⊆";
  if (cmd == "supseteq") return "⊇";
  if (cmd == "subset") return "⊂";
  if (cmd == "supset") return "⊃";
  if (cmd == "notin") return "∉";
  if (cmd == "neq") return "≠";
  if (cmd == "ne") return "≠";
  if (cmd == "leq") return "≤";
  if (cmd == "le") return "≤";
  if (cmd == "geq") return "≥";
  if (cmd == "ge") return "≥";
  if (cmd == "ll") return "≪";
  if (cmd == "gg") return "≫";
  if (cmd == "approx") return "≈";
  if (cmd == "equiv") return "≡";
  if (cmd == "simeq") return "≃";
  if (cmd == "cong") return "≅";
  if (cmd == "propto") return "∝";
  if (cmd == "sim") return "∼";
  if (cmd == "in") return "∈";
  if (cmd == "ni") return "∋";

  // Functions
  if (cmd == "arcsin") return "arcsin";
  if (cmd == "arccos") return "arccos";
  if (cmd == "arctan") return "arctan";
  if (cmd == "sinh") return "sinh";
  if (cmd == "cosh") return "cosh";
  if (cmd == "tanh") return "tanh";
  if (cmd == "sqrt") return "√";
  if (cmd == "sin") return "sin";
  if (cmd == "cos") return "cos";
  if (cmd == "tan") return "tan";
  if (cmd == "log") return "log";
  if (cmd == "ln") return "ln";
  if (cmd == "exp") return "exp";

  // Logic
  if (cmd == "nexists") return "∄";
  if (cmd == "forall") return "∀";
  if (cmd == "exists") return "∃";
  if (cmd == "implies") return "⇒";
  if (cmd == "iff") return "⇔";
  if (cmd == "lnot") return "¬";
  if (cmd == "neg") return "¬";
  if (cmd == "land") return "∧";
  if (cmd == "wedge") return "∧";
  if (cmd == "lor") return "∨";
  if (cmd == "vee") return "∨";

  // Set theory
  if (cmd == "emptyset") return "∅";
  if (cmd == "varnothing") return "∅";
  if (cmd == "cup") return "∪";
  if (cmd == "cap") return "∩";

  // Arrows - longest first
  if (cmd == "longrightarrow") return "⟶";
  if (cmd == "longleftarrow") return "⟵";
  if (cmd == "leftrightarrow") return "↔";
  if (cmd == "Leftrightarrow") return "⇔";
  if (cmd == "leftarrow") return "←";
  if (cmd == "rightarrow") return "→";
  if (cmd == "Leftarrow") return "⇐";
  if (cmd == "Rightarrow") return "⇒";
  if (cmd == "gets") return "←";
  if (cmd == "to") return "→";

  // Miscellaneous
  if (cmd == "hbar") return "ℏ";
  if (cmd == "ell") return "ℓ";
  if (cmd == "aleph") return "ℵ";
  if (cmd == "angle") return "∠";
  if (cmd == "perp") return "⊥";
  if (cmd == "parallel") return "∥";
  if (cmd == "prime") return "′";

  // Dots
  if (cmd == "ldots") return "…";
  if (cmd == "cdots") return "⋯";
  if (cmd == "vdots") return "⋮";
  if (cmd == "ddots") return "⋱";

  // Spaces
  if (cmd == "qquad") return "    ";
  if (cmd == "quad") return "  ";
  if (cmd == " ") return " ";
  if (cmd == ",") return " ";
  if (cmd == ":") return " ";
  if (cmd == ";") return " ";

  return "";
}

String processLatex(const String& t) {
  String out = "";
  int i = 0;

  while (i < t.length()) {
    char c = t[i];

    if (c == '^') {
      i++;
      if (i < t.length() && t[i] == '{') {
        String content = extractBraced(t, i);
        String processed = processLatex(content);
        out += toSuperscript(processed);
      } else if (i < t.length()) {
        // Handle single character or command after ^
        if (t[i] == '\\') {
          int saveI = i;
          i++;
          if (i < t.length() && isAlpha(t[i])) {
            String cmd = "";
            while (i < t.length() && isAlpha(t[i])) {
              cmd += t[i];
              i++;
            }
            String sym = getLatexSymbol(cmd);
            out += toSuperscript(sym.length() > 0 ? sym : cmd);
          } else {
            out += toSuperscript(String(t[i]));
            i++;
          }
        } else {
          out += toSuperscript(String(t[i]));
          i++;
        }
      }
      continue;
    }

    if (c == '_') {
      i++;
      if (i < t.length() && t[i] == '{') {
        String content = extractBraced(t, i);
        String processed = processLatex(content);
        out += toSubscript(processed);
      } else if (i < t.length()) {
        // Handle single character or command after _
        if (t[i] == '\\') {
          int saveI = i;
          i++;
          if (i < t.length() && isAlpha(t[i])) {
            String cmd = "";
            while (i < t.length() && isAlpha(t[i])) {
              cmd += t[i];
              i++;
            }
            String sym = getLatexSymbol(cmd);
            out += toSubscript(sym.length() > 0 ? sym : cmd);
          } else {
            out += toSubscript(String(t[i]));
            i++;
          }
        } else {
          out += toSubscript(String(t[i]));
          i++;
        }
      }
      continue;
    }

    if (c == '\\') {
      i++;
      if (i >= t.length()) {
        out += '\\';
        break;
      }

      if (!isAlpha(t[i])) {
        if (t[i] == ',' || t[i] == ';' || t[i] == ':' || t[i] == ' ') {
          out += " ";
          i++;
          continue;
        }
        out += t[i];
        i++;
        continue;
      }

      String cmd = "";
      while (i < t.length() && isAlpha(t[i])) {
        cmd += t[i];
        i++;
      }

      if (cmd == "frac") {
        while (i < t.length() && t[i] == ' ') i++;
        if (i < t.length() && t[i] == '{') {
          String num = extractBraced(t, i);
          while (i < t.length() && t[i] == ' ') i++;
          if (i < t.length() && t[i] == '{') {
            String den = extractBraced(t, i);
            String numProc = processLatex(num);
            String denProc = processLatex(den);
            out += "(" + numProc + "/" + denProc + ")";
            continue;
          }
        }
        out += "frac";
        continue;
      }

      if (cmd == "sqrt") {
        while (i < t.length() && t[i] == ' ') i++;
        if (i < t.length() && t[i] == '{') {
          String content = extractBraced(t, i);
          String processed = processLatex(content);
          out += "√(" + processed + ")";
          continue;
        }
        out += "√";
        continue;
      }

      if (cmd == "binom") {
        while (i < t.length() && t[i] == ' ') i++;
        if (i < t.length() && t[i] == '{') {
          String n = extractBraced(t, i);
          while (i < t.length() && t[i] == ' ') i++;
          if (i < t.length() && t[i] == '{') {
            String k = extractBraced(t, i);
            String nProc = processLatex(n);
            String kProc = processLatex(k);
            out += "C(" + nProc + "," + kProc + ")";
            continue;
          }
        }
        out += "C";
        continue;
      }

      if (cmd == "mathbf" || cmd == "mathrm" || cmd == "mathit" || cmd == "text" || cmd == "hat" || cmd == "bar" || cmd == "tilde" || cmd == "vec") {
        while (i < t.length() && t[i] == ' ') i++;
        if (i < t.length() && t[i] == '{') {
          String content = extractBraced(t, i);
          String processed = processLatex(content);
          out += processed;
          continue;
        }
        continue;
      }

      if (cmd == "left" || cmd == "right" || cmd == "big" || cmd == "Big" || cmd == "bigg" || cmd == "Bigg") {
        continue;
      }

      String sym = getLatexSymbol(cmd);
      if (sym.length() > 0) {
        out += sym;
        continue;
      }

      out += "\\" + cmd;
      continue;
    }

    out += c;
    i++;
  }

  return out;
}

int totalTextHeight = 0;

void drawWrappedText(String text) {
  tft.fillScreen(ST77XX_BLACK);

  int x = marginLeft;
  int y = marginTop - scrollOffset;

  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setBackgroundColor(ST77XX_BLACK);

  // Use a font that supports Unicode
  // Available fonts: u8g2_font_unifont_t_symbols, u8g2_font_helvR08_tf, etc.
  // u8g2.setFont(noto_full);
  u8g2.setFont(dejavu_sans);


  String processedText = processLatex(text);

  totalTextHeight = marginTop;

  int charHeight = 10;   // Font height
  int avgCharWidth = 6;  // Average character width

  // Word wrapping with Unicode support
  int startIdx = 0;
  while (startIdx < processedText.length()) {
    int lineEnd = startIdx;
    int lastSpace = -1;
    int lineWidth = 0;

    // Find how much text fits on this line
    while (lineEnd < processedText.length()) {
      char c = processedText[lineEnd];

      if (c == '\n') {
        break;
      }

      if (c == ' ') {
        lastSpace = lineEnd;
      }

      // Estimate width (rough approximation)
      int charW = avgCharWidth;
      if ((uint8_t)c >= 0xC0) {
        charW = avgCharWidth + 2;  // Unicode chars slightly wider
      }

      if (lineWidth + charW > virtualWidth) {
        // Line too long, wrap at last space
        if (lastSpace > startIdx) {
          lineEnd = lastSpace;
        }
        break;
      }

      lineWidth += charW;
      lineEnd++;
    }

    // Extract line text
    String lineText = processedText.substring(startIdx, lineEnd);

    // Draw line if visible
    if (y + charHeight > 0 && y < 80) {
      u8g2.setCursor(x, y + charHeight - 2);  // Adjust for baseline
      u8g2.print(lineText);
    }

    // Move to next line
    y += charHeight;
    totalTextHeight += charHeight;

    // Skip spaces at start of next line
    startIdx = lineEnd;
    if (startIdx < processedText.length() && (processedText[startIdx] == ' ' || processedText[startIdx] == '\n')) {
      startIdx++;
    }

    if (lineEnd >= processedText.length()) break;
  }

  if (y > marginTop) {
    totalTextHeight = y - marginTop + scrollOffset;
  }
}

String globalResult = "";


// #define BUTTON_SEQ_PIN 4
#define BUTTON_ACTIVE LOW
#define AUTO_SCROLL_MS 3000

void setup() {
  Serial.begin(115200);

  Serial.println();

  if (!initCamera()) {
    Serial.println("Camera Init Failed");
    return;
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("Camera Ready! IP address: ");
  Serial.println(WiFi.localIP());

  //Initialize TFT
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  u8g2.begin(tft);

  // pinMode(BUTTON_SEQ_PIN, INPUT_PULLUP);
  // pinMode(BTN_SCROLL, INPUT_PULLUP); -> PROBLEMATIK

  Serial.println("System ready!");

  for (int i = 1;i < 6;i++){
    Serial.print("Capturing in ");
    Serial.println(i);
    String loadingText = "Capturing in" + i;
    drawWrappedText(loadingText);
    delay(1000);
  }

  drawWrappedText("Waiting for capture.");
  delay(1000000);
  // globalResult = captureAndAnalyzeSync("Solve This Question");
  // globalResult = "HACIU letsgo";

  // Serial.println(globalResult);
  // drawWrappedText(globalResult);

}
const unsigned long debounceMs = 250;

bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
unsigned long lastAutoScroll = 0;

void loop() {
  unsigned long now = millis();
  if (now - lastAutoScroll >= AUTO_SCROLL_MS) {
    lastAutoScroll = now;

    scrollOffset += scrollStep;

    int maxScroll = totalTextHeight - virtualHeight;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollOffset > maxScroll) scrollOffset = 0;

    drawWrappedText(globalResult);
  }
}
