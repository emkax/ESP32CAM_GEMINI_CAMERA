#include "./conf.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid = "ya";
const char* password = "michael28";

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }



  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  WiFiClientSecure client;
  client.setInsecure();              // no cert
  client.setHandshakeTimeout(15);    // IMPORTANT

  HTTPClient http;

  http.useHTTP10(false);  // use HTTP/1.1
  http.setReuse(true);    // keep-alive
  http.setTimeout(20000); // 20 seconds timeout

  String url = 
    "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" 
    + String(API_KEY);

  Serial.println("Requesting: " + url);

  http.begin(client, url);

  http.addHeader("Content-Type", "application/json");

  String body = R"(
  {
    "contents": [
      {
        "parts": [
          {
            "text": "pls solve this mathematical induction: 1 + 2 + 3 + ... + n = n(n+1)/2"
          }
        ]
      }
    ]
  }
  )";

  int httpCode = http.POST(body);

  Serial.println("Status: " + String(httpCode));

  if (httpCode > 0) {
    Serial.println("Response:");
    Serial.println(http.getString());
  } else {
    Serial.print("Error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void loop() {}