#include <WiFi.h>
#include <SuplaDevice.h>

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// GUID i AUTHKEY dopasuj do wymogów Supla (najczęściej 24 i 16 znaków HEX)
const char SUPLA_GUID[]    = "000000000000259300000000"; // przykładowe 24 znaki
const char SUPLA_AUTHKEY[] = "d39f0361d39f0361";         // przykładowe 16 znaków

#define RELAY_PIN 2

SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Inicjalizacja SuplaDevice z ustawieniami WiFi i Supla
  bool started = SuplaDevice.begin(
    SUPLA_GUID,
    SUPLA_AUTHKEY,
    WIFI_SSID,
    WIFI_PASSWORD,
    23   // wersja protokołu SUPLA, możesz zostawić 23
  );

  if (!started) {
    Serial.println("Failed to start SuplaDevice");
    while (1) delay(1000);
  } else {
    Serial.println("SuplaDevice started");
  }

  SuplaDevice.addChannel(&relay);
}

void loop() {
  SuplaDevice.loop();
}
