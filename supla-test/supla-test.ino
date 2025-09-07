#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaConfigESP.h>  // Konfiguracja WiFi dla ESP32

#define RELAY_PIN 2

// Tworzymy kanał przekaźnika o ID 1, podłączony do pinu RELAY_PIN
SuplaRelay relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Ustawienie danych WiFi i SUPLA - można ustawić ręcznie lub przez webconfig
  // Jeśli chcesz ustawić ręcznie, odkomentuj poniższy blok (jednorazowo),
  // potem wykomentuj i zaprogramuj ponownie, żeby nie nadpisywać configu.
  /*
  SuplaConfig.setSSID("Pawel_LTE");
  SuplaConfig.setPassword("pawel2580s");
  SuplaConfig.setGUID("0000000000002593");    // 12 znaków HEX
  SuplaConfig.setAuthKey("d39f0361");         // 8 znaków HEX
  SuplaConfig.save();
  */

  // Inicjalizacja SuplaDevice z domyślną wersją protokołu (23)
  if (!SuplaDevice.begin()) {
    Serial.println("SuplaDevice.begin() failed");
    while(1) { delay(1000); }
  } else {
    Serial.println("SuplaDevice started");
  }

  // Dodanie kanału przekaźnika do urządzenia Supla
  SuplaDevice.addChannel(&relay);
}

void loop() {
  // Obsługa komunikacji z SUPLA, musi być wywoływane w loop()
  SuplaDevice.loop();
}
