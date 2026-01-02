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
#define TFT_RST 4  // software reset
#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_BLK  27
// Button Pins
#define BTN_PIN  5


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


String globalResult = "CONNECTING..";

int marginLeft = 8;
int marginTop = 8;
int virtualWidth = 160 - 16;
int virtualHeight = 80 - 16;
int scrollOffset = 0;
int scrollStep = 16;


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

void drawWrappedText() {
  tft.fillScreen(ST77XX_BLACK);

  int x = marginLeft;
  int y = marginTop - scrollOffset;

  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setBackgroundColor(ST77XX_BLACK);

  // Use a font that supports Unicode
  // Available fonts: u8g2_font_unifont_t_symbols, u8g2_font_helvR08_tf, etc.
  // u8g2.setFont(noto_full);
  u8g2.setFont(dejavu_sans);


  String processedText = processLatex(globalResult);

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



// #define BUTTON_SEQ_PIN 4
#define BUTTON_ACTIVE LOW
#define AUTO_SCROLL_MS 3000

#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600  , SERIAL_8N1, RXD2, TXD2);

  //Initialize TFT
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  u8g2.begin(tft);

  pinMode(BTN_PIN, INPUT_PULLUP);
  // pinMode(BTN_SCROLL, INPUT_PULLUP); -> PROBLEMATIK

  Serial.println("System ready!");
  drawWrappedText();
}
unsigned long lastPress = 0;
const int debounceDelay = 200;

String data = "";
String buffer = "";
String geminiData = "";
enum RxState { LOGS, GEMINI_DATA };
RxState receiving_state = LOGS;
void loop() {
   while (Serial2.available()) {
    char c = Serial2.read();
    buffer += c;

    if (buffer.endsWith("\n")) {
      buffer.trim();   // remove newline

      // ---- STATE SWITCH TOKENS ----
      if (buffer == "<GEMINI_START>") {
        receiving_state = GEMINI_DATA;
        geminiData = "";
      }
      else if (buffer == "<GEMINI_END>") {
        receiving_state = LOGS;

        globalResult = geminiData;
        drawWrappedText();
      }
      // ---- DATA ----
      else {
        if (receiving_state == LOGS) {
          Serial.println(buffer);
        } else {
          geminiData += buffer + "\n";
        }
      }

      buffer = "";
    }
  }

  bool buttonPressed = (digitalRead(BTN_PIN) == LOW);
  
  if (buttonPressed && (millis() - lastPress > debounceDelay)) {
    Serial.println("Scroll button pressed");
    Serial.println(globalResult);
    lastPress = millis();

    scrollOffset += scrollStep;

    int maxScroll = totalTextHeight - virtualHeight;
    if (maxScroll < 0) maxScroll = 0;
    
    if (scrollOffset > maxScroll) {
      scrollOffset = 0;
    }
    drawWrappedText();
  }
}