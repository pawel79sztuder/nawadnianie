#include <WiFi.h>
#include <SuplaDevice.h>

// Dane WiFi
#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// Twoje dane SUPLA (podaj własne)
#define SUPLA_GUID    "0000000000002593"  // 16 znaków HEX
#define SUPLA_AUTHKEY "d39f0361"          // 8 znaków HEX

// Pin przekaźnika
#define RELAY_PIN 2

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Inicjalizacja SuplaDevice na globalnym obiekcie SuplaDevice
  SuplaDevice.begin(SUPLA_GUID, SUPLA_AUTHKEY, WIFI_SSID, WIFI_PASSWORD, SUPLA_CHANNELTYPE_RELAY, RELAY_PIN);
  Serial.println("SuplaDevice started");
}

void loop() {
  SuplaDevice.loop();
}
