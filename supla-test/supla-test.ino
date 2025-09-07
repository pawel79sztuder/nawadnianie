#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaDeviceESP32.h>   // konieczne dla ESP32
#include <SuplaConfigESP.h>     // konfiguracja WiFi i Supla

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// GUID musi mieć dokładnie 12 bajtów (24 znaki hex), authkey 8 bajtów (16 znaków hex)
const char SUPLA_GUID[]    = "0000000000002593";  // 12 bajtów w hex (tutaj 16 znaków, dopasuj dokładnie do wymagań Supla!)
const char SUPLA_AUTHKEY[] = "d39f0361";          // 8 bajtów w hex (tutaj 8 znaków, dopasuj!)

#define RELAY_PIN 2

// Utworzenie obiektu przekaźnika (kanał)
SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Konfiguracja Supla - jeśli nie chcesz ustawiać ręcznie przez webconfig,
  // możesz zakomentować poniższy blok, jeśli konfiguracja jest już zapisana.
  SuplaConfig.setSSID(WIFI_SSID);
  SuplaConfig.setPassword(WIFI_PASSWORD);
  SuplaConfig.setGUID(SUPLA_GUID);
  SuplaConfig.setAuthKey(SUPLA_AUTHKEY);
  SuplaConfig.save();

  // Uruchomienie SuplaDevice
  if (!SuplaDevice.begin()) {
    Serial.println("SuplaDevice.begin() failed");
    while (1) delay(1000);
  } else {
    Serial.println("SuplaDevice started");
  }

  // Dodanie kanału przekaźnika
  SuplaDevice.addChannel(&relay);
}

void loop() {
  SuplaDevice.loop();
}
