#include <WiFi.h>
#include <SuplaDevice.h>

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

#define SUPLA_GUID    "2593"
#define SUPLA_AUTHKEY "d39f0361"

#define RELAY_PIN 2

// Tworzymy kanał przekaźnika (id kanału = 1, steruje pinem RELAY_PIN)
SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Inicjalizacja SuplaDevice z GUID i authkey
  SuplaDevice.begin(SUPLA_GUID, SUPLA_AUTHKEY);

  // Dodaj kanał przekaźnika
  SuplaDevice.addChannel(&relay);

  Serial.println("SuplaDevice started");
}

void loop() {
  // Obsługa komunikacji z SUPLA
  SuplaDevice.loop();
}
