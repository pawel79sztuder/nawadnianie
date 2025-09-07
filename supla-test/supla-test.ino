#include <WiFi.h>
#include <supla/network/esp_wifi.h>
#include <supla/esp32.h>

// Dane WiFi
#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// Dane Supla (Twoje GUID i hasło do urządzenia SUPLA)
#define SUPLA_GUID    "2593"       // GUID urządzenia SUPLA
#define SUPLA_AUTHKEY "d39f0361"    // Hasło/authkey do urządzenia SUPLA

// Pin przekaźnika
#define RELAY_PIN 2

// Utworzenie obiektu WiFi i urządzenia Supla
Supla::ESPWifi wifi(WIFI_SSID, WIFI_PASSWORD);
Supla::ESP32 device(wifi, SUPLA_GUID, SUPLA_AUTHKEY);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  device.begin();
  Serial.println("SuplaDevice started");
}

void loop() {
  device.loop();

  // Przykładowa obsługa przekaźnika - sterowanie przez SUPLA kanał (jeśli dodane)
  // Można rozszerzyć o obsługę kanałów i zdarzeń, zależnie od potrzeb
}
