#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaConfigESP.h>     // konfiguracja WiFi i Supla

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// GUID musi mieć 12 bajtów (24 znaki hex), AuthKey 8 bajtów (16 znaków hex)
const char SUPLA_GUID[]    = "000000000000259300000000";  // dopasuj do 24 znaków!
const char SUPLA_AUTHKEY[] = "d39f0361d39f0361";          // dopasuj do 16 znaków!

#define RELAY_PIN 2

SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  SuplaConfig.setSSID(WIFI_SSID);
  SuplaConfig.setPassword(WIFI_PASSWORD);
  SuplaConfig.setGUID(SUPLA_GUID);
  SuplaConfig.setAuthKey(SUPLA_AUTHKEY);
  SuplaConfig.save();

  if (!SuplaDevice.begin()) {
    Serial.println("SuplaDevice.begin() failed");
    while (1) delay(1000);
  } else {
    Serial.println("SuplaDevice started");
  }

  SuplaDevice.addChannel(&relay);
}

void loop() {
  SuplaDevice.loop();
}
