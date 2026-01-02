# ESP32-CAM Gemini Vision System

An **ESP32-CAM–based AI vision system** that captures images, sends them to **Google Gemini** for analysis, and displays structured AI responses (including **LaTeX-formatted text**) on a connected screen.

This project combines **embedded systems**, **computer vision**, and **LLM-powered reasoning** in a resource-constrained environment.

---

## 🚀 Key Features

### 📷 Camera → Gemini AI Pipeline

* Captures images using **ESP32-CAM**
* Sends image data to **Gemini Vision API**
* Receives structured AI-generated text responses
* Supports contextual analysis (scene understanding, object recognition, explanations)

### 🧠 AI Reasoning Output

* Gemini responses include:

  * Natural language explanations
  * Structured sections
  * **LaTeX-encoded math or formulas** (e.g. `\\(E = mc^2\\)`)

### 🖥️ On-Device Display

* Displays Gemini output on a **TFT/LCD screen**
* Supports:

  * Automatic text wrapping
  * Smooth vertical auto-scrolling
  * Parsing of tagged data blocks (e.g. `<GEMINI_START>` / `<GEMINI_END>`)

### 🔁 Robust Serial Communication

* ESP32 ↔ ESP32-CAM communication over **UART**
* State-machine-based receiver for:

  * Logs
  * Gemini data streams
* Safe buffering to avoid overflow or blocking

### 🧩 Modular Architecture

* Camera capture logic isolated from AI and UI layers
* Display rendering separated from serial parsing
* Easy to extend with:

  * Additional sensors
  * Different AI models
  * Cloud endpoints

---

## 🛠️ Hardware Components

* ESP32-CAM (OV2640)
* ESP32 Dev Board
* TFT / LCD display
* UART connection between boards
* Stable power supply (important for camera stability)

---

## 🧪 Data Flow Overview

1. **Image Capture**

   * ESP32-CAM captures a frame

2. **AI Request**

   * Image sent to Gemini Vision API

3. **AI Response**

   * Gemini returns text + LaTeX-encoded content

4. **Serial Transfer**

   * Response wrapped in markers and sent via UART

5. **Rendering**

   * Main ESP32 parses, wraps, scrolls, and displays output

---

## 📐 LaTeX Rendering Support

* Gemini responses may contain LaTeX expressions
* Encoded safely for serial transport
* Displayed as readable math text on embedded screen

Example:

```
The velocity is given by: \\(v = \\frac{d}{t}\\)
```

---

## 🔒 Configuration & Security

* Sensitive config files are excluded using `.gitignore`
* API keys stored outside version control

```gitignore
*.conf
conf.h
```

---

## 🧩 Future Improvements

* Inline LaTeX symbol rendering
* Image annotation overlays
* Multi-language Gemini responses
* Touch-based scrolling
* Offline fallback modes

---

## 📌 Use Cases

* AI-assisted education tools
* Smart inspection systems
* Embedded AI demos
* Low-cost visual reasoning devices
