#include <WiFi.h>                        // najpierw WiFi (ESP32)
#include <AsyncTCP.h>                   // lub <ESPAsyncTCP.h> dla ESP8266
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>        // Manager korzystający z AsyncWebServer

#include <AsyncElegantOTA.h>
#include <elegantWebpage.h>
#include <Hash.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include <ArduinoJson.h>
#include <DNSServer.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <SPIFFS.h>

#include <Wire.h>
#include <PCF8574.h>

// Ustaw adres ekspandera (Twój: 0x2?)
PCF8574 pcf(0x20);

#define EEPROM_PUMP_MODE 100  // Adres EEPROM dla trybu pracy pompy
bool pumpAlwaysOn = false;    // Domyślny tryb pompy (false = jak teraz)
unsigned long lastSavedClock = 0; //zmienna ostatnia godz


// --- TFT ST7789 1.14" 135x240 ---
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   4


Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#define SCREEN_WIDTH  135
#define SCREEN_HEIGHT 240

#define BUTTON_PIN 0         // GPIO, do którego podłączony jest przycisk
#define BUTTON_PIN2 35         // GPIO, do którego podłączony jest przycisk
int currentScreen = 0;       // Numer aktualnego ekranu
int t=0;

// --- WiFi ---
///const char* ssid = "Galaxy A71 Pawel";//"UPC1780423_EXT2G";
///const char* password ="pawel123"; //"7Ssr4scjPdbk";
AsyncWebServer server(80);

// --- NTP ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // UTC+2, aktualizacja co 60s

// --- Nawadnianie ---
#define NUM_GROUPS 4
#define SECTIONS_PER_GROUP 2
#define SCHEDULES_PER_GROUP 3

 // --- Piny przekaźników --- wirtualnie
// Zakładamy 4 grupy, 2 sekcje = 8 przekaźników
const int relayPins[NUM_GROUPS][SECTIONS_PER_GROUP] = {
  {121, 121},  // Grupa 0: sekcje 0 i 1
  {121, 121},  // Grupa 1: sekcje 0 i 1
  {121, 121},  // Grupa 2: sekcje 0 i 1
  {121, 121}   // Grupa 3: sekcje 0 i 1
};

#define PUMP_RELAY_PIN 20
bool pumpState = false;  // true = pompa włączona, false = wyłączona

struct Schedule {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t durationSec;
  bool enabled;  // nowość: czy harmonogram aktywny

 };

 // --- Lokalny czas (backup po utracie WiFi) ---
unsigned long lastNTPUpdate = 0;
unsigned long lastMillisUpdate = 0;
time_t localTimeSimulated = 0;
bool ntpSynced = false;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 60000; // 60 sekund

////do odczyt nażywo w www
AsyncEventSource events("/events");
unsigned long sectionStartTime[NUM_GROUPS][SECTIONS_PER_GROUP] = {0};
unsigned long lastTime = 0;
////

Schedule schedules[NUM_GROUPS][SCHEDULES_PER_GROUP];
bool manualStates[NUM_GROUPS][SECTIONS_PER_GROUP] = {false};

// --- EEPROM ---
#define EEPROM_SIZE 512

#define EEPROM_TIME_ADDR 500  // końcówka EEPROM, nie koliduje z harmonogramami

#define ONE_WIRE_BUS 4  // GPIO podłączony do Data DS18B20

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float currentTemperature = 0.0;


void saveSchedulesToEEPROM() {
  int addr = 0;
  for (int g = 0; g < NUM_GROUPS; g++) {
    for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
      EEPROM.put(addr, schedules[g][i]);
      addr += sizeof(Schedule);
    }
  }
  EEPROM.commit();
}

void loadSchedulesFromEEPROM() {
  int addr = 0;
  for (int g = 0; g < NUM_GROUPS; g++) {
    for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
      EEPROM.get(addr, schedules[g][i]);
      if (schedules[g][i].hour > 23 || schedules[g][i].minute > 59 || schedules[g][i].durationSec == 0) {
        // Domyślne wartości harmonogramu
        schedules[g][i].hour = 6 + i * 2;
        schedules[g][i].minute = 0;
        schedules[g][i].durationSec = 10;
        schedules[g][i].enabled = true;  // domyślnie włączone
      }
      addr += sizeof(Schedule);
    }
  }
}

void savePumpModeToEEPROM() {
  EEPROM.write(EEPROM_PUMP_MODE, pumpAlwaysOn ? 1 : 0);
  EEPROM.commit();
}

void loadPumpModeFromEEPROM() {
  uint8_t value = EEPROM.read(EEPROM_PUMP_MODE);
  pumpAlwaysOn = (value == 1);
}


void updatePumpState() { 
  if (pumpAlwaysOn) {
    // Pompa zawsze włączona niezależnie od sekcji
    pumpState = true;
    digitalWrite(PUMP_RELAY_PIN, LOW);  // LOW = pompa ON (jeśli tak masz ustawione)
    return;
  }

  bool anyOn = false;

  for (int g = 0; g < NUM_GROUPS; g++) {
    for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
      if (manualStates[g][s]) {
        anyOn = true;
        break;
      }
    }
    if (anyOn) break;
  }

  pumpState = anyOn;
  digitalWrite(PUMP_RELAY_PIN, pumpState ? LOW : HIGH);  // LOW = pompa ON, HIGH = OFF
}


// --- Przekaźniki (symulacja) ---

void controlRelay(int group, int section, bool state) {
  // sterowanie ESP32 (zakładam, że LOW = włączony przekaźnik)
  digitalWrite(relayPins[group][section], state ? LOW : HIGH);

  // sterowanie PCF8574 (zakładam 1 układ, 8 pinów)
  int pcfPin = group * SECTIONS_PER_GROUP + section;
  if (pcfPin < 8) {
    pcf.write(pcfPin, state ? LOW : HIGH);
  }

  manualStates[group][section] = state;

  if (state) {
    sectionStartTime[group][section] = localTimeSimulated;  // start pracy
  } else {
    sectionStartTime[group][section] = 0;  // stop pracy
  }

  Serial.printf("Grupa %d Sekcja %d: %s (pin ESP %d, PCF %d)\n",
                group, section, state ? "WŁ." : "WYŁ.", relayPins[group][section], pcfPin);

  updatePumpState();  // ← DODANE
                
}





// --- Harmonogram podlewania ---
int currentSection[NUM_GROUPS] = {0};            // która sekcja działa w każdej grupie
unsigned long groupStartTime[NUM_GROUPS] = {0};  // czas startu sekcji
uint16_t groupDuration[NUM_GROUPS] = {0};        // czas trwania podlewania
bool groupActive[NUM_GROUPS] = {false};          // czy dana grupa podlewa

unsigned long manualStartTime[NUM_GROUPS][SECTIONS_PER_GROUP] = {0};
uint16_t manualDuration[NUM_GROUPS][SECTIONS_PER_GROUP] = {0};

///historia podlewań
void logWateringToSPIFFS(int group, int section, int duration, const String& source) {
  const int MAX_LOGS = 300;

  // 1. Wczytaj wszystkie linie
  File file = SPIFFS.open("/logs.txt", FILE_READ);
  std::vector<String> lines;
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) lines.push_back(line);
    }
    file.close();
  }

  // 2. Utwórz log
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);

  char dateBuf[16];
  strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", timeinfo);
  String today = String(dateBuf);

  char timeBuf[16];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", timeinfo);
  String logEntry = String(timeBuf) + " | G" + group + " S" + section + " | " + duration + "s | " + source;

  // 3. Szukamy czy data już jest
  int dateIndex = -1;
  for (int i = 0; i < lines.size(); i++) {
    if (lines[i] == today) {
      dateIndex = i;
      break;
    }
  }

  if (dateIndex == -1) {
    // Brak daty – dodajemy ją i log na górze
    lines.insert(lines.begin(), logEntry);
    lines.insert(lines.begin(), today);
  } else {
    // Dodajemy log pod nagłówkiem daty
    lines.insert(lines.begin() + dateIndex + 1, logEntry);
  }

  // 4. Usuń najstarsze, jeśli przekroczono limit
  int logCount = 0;
  for (String& l : lines) {
    if (l.indexOf("|") != -1) logCount++;
  }

  while (logCount > MAX_LOGS) {
    // Usuń od końca pierwszy log
    for (int i = lines.size() - 1; i >= 0; i--) {
      if (lines[i].indexOf("|") != -1) {
        lines.erase(lines.begin() + i);
        logCount--;
        break;
      }
    }

    // Usuń też nagłówek, jeśli nie ma pod nim żadnych logów
    for (int i = lines.size() - 1; i >= 0; i--) {
      if (lines[i].indexOf("|") == -1) {
        if (i == lines.size() - 1 || lines[i + 1].indexOf("|") == -1) {
          lines.erase(lines.begin() + i);
        }
      }
    }
  }

  // 5. Zapisz od nowa
  file = SPIFFS.open("/logs.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Błąd zapisu logów");
    return;
  }

  for (const String& l : lines) {
    file.println(l);
  }
  file.close();

  Serial.println("[logWateringToSPIFFS] Dodano log z grupą daty.");
}
///

void stopWatering() {
  for (int g = 0; g < NUM_GROUPS; g++) {
    if (groupActive[g]) {
      for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
        controlRelay(g, s, false);
        manualStates[g][s] = false;
      }
      groupActive[g] = false;
      currentSection[g] = 0;
      groupStartTime[g] = 0;
      groupDuration[g] = 0;
      Serial.printf("Zatrzymano podlewanie grupy %d\n", g);
    }
  }
}


// Na poziomie globalnym – przed setup()
bool wasTriggered[NUM_GROUPS][SCHEDULES_PER_GROUP] = {false};
int lastCheckedMinute = -1;

void checkSchedule() {
  // Pobranie aktualnego czasu
  time_t currentTime = ntpSynced ? localTimeSimulated : timeClient.getEpochTime(); 
  struct tm *timeinfo = localtime(&currentTime);
  int h = timeinfo->tm_hour;
  int m = timeinfo->tm_min;
  int s = timeinfo->tm_sec;

  int currentSeconds = h * 3600 + m * 60 + s;

  // Debug – logowanie czasu
  //Serial.printf("[checkSchedule] Czas: %02d:%02d:%02d (%d)\n", h, m, s, currentSeconds);

  // Reset triggerów przy zmianie minuty
  if (m != lastCheckedMinute) {
    for (int g = 0; g < NUM_GROUPS; g++) {
      for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
        wasTriggered[g][i] = false;
      }
    }
    lastCheckedMinute = m;
    Serial.printf("[checkSchedule] Reset triggerów, nowa minuta: %02d\n", m);
  }

  // Sprawdź harmonogramy – rozpoczęcie podlewania
  for (int g = 0; g < NUM_GROUPS; g++) {
    if (!groupActive[g]) {
      for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
        Schedule& sch = schedules[g][i];
        int scheduleSeconds = sch.hour * 3600 + sch.minute * 60;

        
        if (sch.enabled && 
            currentSeconds >= scheduleSeconds && 
            currentSeconds < scheduleSeconds + 60 &&  // 60-sekundowe okienko
            !wasTriggered[g][i]) {

          groupActive[g] = true;
          currentSection[g] = 0;
          groupDuration[g] = sch.durationSec;
          groupStartTime[g] = millis();
          controlRelay(g, 0, true);
          manualStates[g][0] = true;
          wasTriggered[g][i] = true;

          Serial.printf("🚿 Start podlewania G%d S0 – trwa %d s\n", g, sch.durationSec);
          // logWateringToSPIFFS(g, 0, sch.durationSec, "auto");
          break;  // Przerwij po uruchomieniu 1 harmonogramu dla tej grupy
        }
      }
    }
  }

  // Obsługa trwających podlewań
  for (int g = 0; g < NUM_GROUPS; g++) {
    if (groupActive[g]) {
      if (millis() - groupStartTime[g] >= groupDuration[g] * 1000UL) {
        logWateringToSPIFFS(g, currentSection[g], groupDuration[g], "auto");

        controlRelay(g, currentSection[g], false);
        manualStates[g][currentSection[g]] = false;

        currentSection[g]++;
        if (currentSection[g] >= SECTIONS_PER_GROUP) {
          groupActive[g] = false;
          currentSection[g] = 0;
          Serial.printf("✅ Koniec podlewania G%d\n", g);
        } else {
          controlRelay(g, currentSection[g], true);
          manualStates[g][currentSection[g]] = true;
          groupStartTime[g] = millis();
          //Serial.printf("➡️  Grupa %d: Start sekcji %d\n", g, currentSection[g]);
        }
      }
    }
  }
}


// 🟢 Poprawna definicja (poza funkcją drawScreen / setup / loop)
void drawWiFiBars(int rssi, int x, int y) {
  int bars = 0;

  if (rssi >= -50) bars = 4;
  else if (rssi >= -60) bars = 3;
  else if (rssi >= -70) bars = 2;
  else if (rssi >= -80) bars = 1;
  else bars = 0;

  int barWidth = 6;
  int spacing = 3;
  int maxHeight = 18;

  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * (maxHeight / 4);
    int bx = x + i * (barWidth + spacing);
    int by = y + maxHeight - h;

    if (i < bars)
      display.fillRect(bx, by, barWidth, h, ST77XX_GREEN);
    else
      display.drawRect(bx, by, barWidth, h, ST77XX_WHITE);
  }
}


void drawNoWiFiIcon(int x, int y) {
  // WiFi łuki (większe promienie)
  display.drawCircle(x + 12, y + 12, 10, ST77XX_RED);
  display.drawCircle(x + 12, y + 12, 6, ST77XX_RED);
  display.drawPixel(x + 12, y + 12, ST77XX_RED);

  // Przekreślenie (ukośna linia przez środek)
  //display.drawLine(x + 4, y + 20, x + 20, y + 4, ST77XX_RED);
}


// --- LCD ---
void drawScreen(int screenNum) {
  static String lastTime = "";
  static bool lastStates[NUM_GROUPS][SECTIONS_PER_GROUP] = {false};
  static int lastScreen = -1;
  static int lastRSSI = 0;
  static String lastIP = "";
  static int lastWateringGroup = -2;
  static int lastWateringSection = -1;
  static unsigned long lastRemaining = 99999;
static unsigned long timer = 0;



  bool screenChanged = (screenNum != lastScreen);

  if (screenChanged) {
    display.fillScreen(ST77XX_BLACK);
    lastScreen = screenNum;
    lastTime = "";
    lastWateringGroup = -2;
    lastRemaining = 99999;
  }



  switch (screenNum) {
    case 0: {
    int rssi = WiFi.RSSI();   
 char buffer[6];  // wystarczy 5 znaków + null
if (ntpSynced) {
  time_t now = localTimeSimulated;
  struct tm *timeinfo = localtime(&now);
  sprintf(buffer, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
} else {
  String fullTime = timeClient.getFormattedTime(); // np. "14:23:45"
  fullTime = fullTime.substring(0, 5); // wytnij tylko "14:23"
  strcpy(buffer, fullTime.c_str());
}
String currentTime = String(buffer);


      if (lastTime != currentTime) {
        display.fillRect(50, 0, 160, 30, ST77XX_BLACK);
        display.setCursor(40, 0);
        display.setTextColor(ST77XX_BLUE);
        display.setTextSize(4);
        display.println(currentTime);
        lastTime = currentTime;
      }

      display.setTextSize(2);
      display.setTextColor(ST77XX_RED);
      display.setCursor(0, 40);
      display.println("    S0  S1");

      for (int g = 0; g < NUM_GROUPS; g++) {
        bool changed = false;
        for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
          if (manualStates[g][s] != lastStates[g][s]) {
            changed = true;
            break;
          }
          if (timer<=10) {
           changed = true;
          timer=timer+1;  // już nie wykona się ponownie
          }
        }
      
        if (changed) {
          int y = 60 + g * 20;
          display.fillRect(0, y, 240, 20, ST77XX_BLACK);
          display.setCursor(0, y);
          display.setTextColor(ST77XX_WHITE);
          display.printf("G%d:", g);
          for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
            display.print(manualStates[g][s] ? " ON " : " OFF");
            lastStates[g][s] = manualStates[g][s];
          }
        }
    }


     // Ikona braku WiFi
if (WiFi.status() != WL_CONNECTED) {
  drawNoWiFiIcon(215, 0);  // Prawy górny róg (możesz dopasować)
} else {
  // Wyczyść obszar ikony, jeśli było wcześniej
  //display.fillRect(208, 0, 20, 12, ST77XX_BLACK);
  // drawyesWiFiIcon(215, 0);
  
drawWiFiBars(rssi, 200, 0);  
}

      
      break;
    }

    case 1: {
      int rssi = WiFi.RSSI();
      drawWiFiBars(rssi, 200, 0);
      if (screenChanged) {
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.setTextColor(ST77XX_GREEN);
        for (int g = 0; g < 2; g++) {
          display.printf("Grupa %d\n", g);
          for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
            Schedule& sch = schedules[g][i];
            display.printf("%02d:%02d %ds %s\n", sch.hour, sch.minute, sch.durationSec, sch.enabled ? "ON" : "OFF");
          }
        }
      }
      break;
    }

    case 2: {
      int rssi = WiFi.RSSI();
      drawWiFiBars(rssi, 200, 0);
      if (screenChanged) {
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.setTextColor(ST77XX_GREEN);
        for (int g = 2; g < NUM_GROUPS; g++) {
          display.printf("Grupa %d\n", g);
          for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
            Schedule& sch = schedules[g][i];
            display.printf("%02d:%02d %ds %s\n", sch.hour, sch.minute, sch.durationSec, sch.enabled ? "ON" : "OFF");
          }
        }
      }
      break;
    }

    case 3: {
      int rssi = WiFi.RSSI();
      drawWiFiBars(rssi, 200, 0);
      // STATUS – odśwież, jeśli stan się zmienił
      display.setTextSize(2);
      for (int g = 0; g < NUM_GROUPS; g++) {
        bool changed = false;
        for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
          if (manualStates[g][s] != lastStates[g][s]) {
            changed = true;
            lastStates[g][s] = manualStates[g][s];
          }
        }

        if (screenChanged || changed) {
          int y = 30 + g * 20;
          display.fillRect(0, y, 240, 20, ST77XX_BLACK);
          display.setCursor(0, y);
          display.setTextColor(ST77XX_GREEN);
          display.printf("G%d:", g);
          for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
            display.printf(" S%d:%s ", s, manualStates[g][s] ? "ON" : "OFF");
          }
        }
      }

      if (screenChanged) {
        display.setCursor(0, 0);
        display.setTextSize(3);
        display.setTextColor(ST77XX_GREEN);
        display.println("= STATUS =");
      }
      break;
    }

        case 4: {
          int rssi = WiFi.RSSI();
      drawWiFiBars(rssi, 200, 0);
      display.setTextSize(1);
      display.setTextColor(ST77XX_GREEN);
      display.fillRect(0, 40, 240, 160, ST77XX_BLACK);
      int y = 40;
      bool anyActive = false;

      for (int g = 0; g < NUM_GROUPS; g++) {
        if (groupActive[g]) {
          unsigned long remaining = (groupDuration[g] * 1000UL - (millis() - groupStartTime[g])) / 1000;
          display.setCursor(0, y);
          display.printf("G%d S%d Podlewa\n", g, currentSection[g]);
          display.printf("Pozostalo: %lus\n", remaining);
          y += 40;
          anyActive = true;
        }
      }

      if (!anyActive) {
        display.setCursor(0, y);
        display.println("Nie podlewa");
      }

      if (screenChanged) {
        display.setCursor(0, 0);
        display.setTextSize(3);
        display.setTextColor(ST77XX_BLUE);
        display.println("== LOGI ==");
      }
      
      break;
    }

    case 5: {
      // SYSTEM – aktualizuj IP i RSSI
      String currentIP = WiFi.localIP().toString();
      int rssi = WiFi.RSSI();
      drawWiFiBars(rssi, 200, 0);

      if (screenChanged || currentIP != lastIP || abs(rssi - lastRSSI) > 2) {
        display.fillRect(0, 30, 240, 60, ST77XX_BLACK);
        display.setCursor(0, 30);
        display.setTextColor(ST77XX_GREEN);
        display.setTextSize(2);
        display.print("IP: ");
        display.println(currentIP);
        display.print("Sygnal: ");
        display.println(rssi);
        display.println(WiFi.SSID());
        
        

        lastIP = currentIP;
        lastRSSI = rssi;
      }

      if (screenChanged) {
        display.setCursor(0, 0);
        display.setTextSize(3);
        display.setTextColor(ST77XX_YELLOW);
        display.println("= SYSTEM =");
       timer=0;
      }
      break;
    }
  }
}



// --- Web UI ---
void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Nawadnianie - Panel</title>
  <style>
  
  .container {
  max-width: 900px;
  margin: 0 auto;
  padding: 20px;
  background-color: #1a1a1a;  /* Tło kontenera, pasujące do stylu */
  border-radius: 10px;
  box-shadow: 0 0 15px rgba(0,0,0,0.4);
}

    body {
      background-color: #121212;
      color: #e0e0e0;
      font-family: Arial, sans-serif;
      font-size: 20px;
      zoom: 1;
      margin: 10px;
    }

    /* --- Style zakładek --- */
    .tabs {
      display: flex;
      gap: 10px;
      margin-bottom: 15px;
    }
    .tabs button {
      background-color: #1e1e1e;
      color: #e0e0e0;
      border: none;
      padding: 12px 20px;
      border-radius: 6px;
      cursor: pointer;
      font-weight: bold;
      font-size: 18px;
      transition: background-color 0.3s ease;
    }
    .tabs button.active,
    .tabs button:hover {
      background-color: #32CD32;
      color: black;
    }

    .tabcontent {
      display: none;
    }
    .tabcontent.active {
      display: block;
    }

    input, select, button {
      font-size: 18px;
      padding: 12px;
      margin: 8px 0;
      background-color: #1e1e1e;
      color: #e0e0e0;
      border: 1px solid #444;
      border-radius: 4px;
    }
    input[type="time"] {
      text-align: center;
    }
    button {
      background-color: #32CD32;
      color: black;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-weight: bold;
      font-size: 20px;
      transition: background-color 0.3s ease;
    }
    button:hover {
      background-color: #55a049;
    }
    h2, h3 {
      color: #90caf9;
    }
    #resp {
      background: #1e1e1e;
      padding: 16px;
      border: 1px solid #444;
      width: fit-content;
      color: #e0e0e0;
      white-space: pre-wrap;
    }

    input[type="checkbox"] {
      width: 20px;
      height: 20px;
    }

    /* Reszta stylów svg pompy */
    .pump-icon {
      width: 40px;
      height: 40px;
    }
    @keyframes spinRotor {
      0%   { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .pump-on .rotor {
      animation: spinRotor 0.8s linear infinite;
      transform-origin: 50% 50%;
    }
    .pump-off .rotor {
      animation: none;
    }
    .pump-on .pump-bg {
      fill: #1e1e1e;
      stroke: #90caf9;
      stroke-width: 3;
    }
    .pump-on .rotor-blade {
      fill: #32CD32;
    }
    .pump-on .pump-center {
      fill: #90caf9;
    }
    .pump-off .pump-bg {
      fill: #2e2e2e;
      stroke: #888;
    }
    .pump-off .rotor-blade {
      fill: #888;
    }
    .pump-off .pump-center {
      fill: #aaa;
    }

    #status {
    zoom: 0.7;
  }

  #schedule,
  #settings {
    zoom: 0.6;
  }
  </style>

  <script>
    // Pokazywanie i ukrywanie kart
    function showTab(tabId, btn) {
      document.querySelectorAll('.tabcontent').forEach(div => div.classList.remove('active'));
      document.getElementById(tabId).classList.add('active');
      
      document.querySelectorAll('.tabs button').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
     
    }

    const settingsPassword = "1234";
  function askPassword(btn) {
    const userInput = prompt("🔒 Podaj hasło dostępu:");
    if (userInput === settingsPassword) {
      showTab('settings', btn);
      fetchWateringLogs(); // <-- Dodaj to
    } else {
      alert("❌ Błędne hasło!");
    }
  }

    // Domyślnie pokaż pierwszą kartę po załadowaniu
    window.onload = function() {
      document.querySelector('.tabs button').click();
      fetchSchedules();  // Wczytaj harmonogramy od razu
      fetchTemperature();
      
    }

    function getWifiSignalIcon(rssi) {
      if (rssi >= -50) return "📶📶📶📶";
      else if (rssi >= -60) return "📶📶📶";
      else if (rssi >= -70) return "📶📶";
      else if (rssi >= -80) return "📶";
      else return "❌";
    }

    if (!!window.EventSource) {
      const source = new EventSource('/events');

      source.addEventListener('status', function(e) {
        const data = JSON.parse(e.data);
        const czas = new Date(data.time * 1000).toLocaleTimeString();
        document.getElementById("czas").textContent = czas;
        document.getElementById("ip").textContent = data.wifiIP;
        document.getElementById("rssi").textContent = getWifiSignalIcon(data.wifiRSSI);

        const pump = document.getElementById("pump");
        if (data.pumpOn) {
          pump.classList.add("pump-on");
          pump.classList.remove("pump-off");
        } else {
          pump.classList.add("pump-off");
          pump.classList.remove("pump-on");
        }

 let stanHTML = "";
data.states.forEach((group, g) => {
  console.log(`Grupa ${g}:`, group);

  const groupIsActive = group.includes(1); // czy jakaś sekcja w grupie jest aktywna?

  stanHTML += `Grupa ${g} `;

  // jeśli aktywna, pokaż ikonę, jeśli nie - nic
  if (groupIsActive) {
    stanHTML += `<span style="font-size: 20px; color: #32CDFF;">💦</span>`;
  }

  stanHTML += `<br>`;

  group.forEach((val, s) => {
    const isOn = val === 1;
    const color = isOn ? "#32CD32" : "#b22222";
    const text = isOn ? "ON" : "OFF";

    let timeLeft = "";
    if (isOn && data.remainingTime && data.remainingTime[g] && data.remainingTime[g][s] > 0) {
      timeLeft = ` - ${data.remainingTime[g][s]}s`;
    }

    stanHTML += `S ${s}: <span style="color:${color}">${text}</span>${timeLeft} &nbsp;&nbsp;`;
  });

  stanHTML += `<br>`;
});
document.getElementById("stany").innerHTML = stanHTML;

 // 👉 Dodaj to:
  let relayHTML = "";
  data.states.forEach((group, g) => {
    relayHTML += `<b>Grupa ${g}</b><br>`;
    group.forEach((val, s) => {
      const isOn = val === 1;
      const icon = isOn ? "💦" : "❌";
      const color = isOn ? "#32CD32" : "#b22222";
      relayHTML += `S${s}: <span style="color:${color}">${icon}</span> &nbsp;&nbsp;`;
    });
    relayHTML += "<br>";
  });
  document.getElementById("relayStatus").innerHTML = relayHTML;


document.getElementById("freeHeap").textContent = data.freeHeap;

let uptimeSec = data.uptimeSec || 0;
let minutes = Math.floor(uptimeSec / 60);
let seconds = uptimeSec % 60;
document.getElementById("uptime").textContent = `${minutes} min ${seconds} sek`;



      });

      source.addEventListener('error', function(e) {
        console.error("Błąd SSE", e);
      });
    }

    // Manual Control
    function sendManual() {
      const g = document.getElementById('group').value;
      const s = document.getElementById('section').value;
      const state = document.getElementById('state').value;
      const duration = document.getElementById('duration').value;

      const url = `/manual?group=${g}&section=${s}&state=${state}&duration=${duration}`;

      fetch(url, {
        method: "POST"
      })
      .then(response => response.text())
      .then(data => {
        document.getElementById("resp").textContent = data;
      });
    }

    // Harmonogramy
    let schedules = [];

    function fetchSchedules() {
      fetch('/schedules')
        .then(r => r.json())
        .then(data => {
          schedules = data;
          let html = '';
          for(let g=0; g<data.length; g++) {
            html += '<b>Grupa ' + g + '</b><br>';
            for(let i=0; i<data[g].length; i++) {
              let sch = data[g][i];
              let hh = String(sch.hour).padStart(2,'0');
              let mm = String(sch.minute).padStart(2,'0');
              html += `Czas: <input type="time" id="time${g}_${i}" value="${hh}:${mm}" step="60"> - `;

              html += `<select id="d${g}_${i}">`;
              for (let sec = 10; sec <= 1200; sec += 10) {
                let selected = (sch.durationSec === sec) ? "selected" : "";
                html += `<option value="${sec}" ${selected}>${sec}s</option>`;
              }
              html += `</select> `;

              html += `<label><input type="checkbox" id="e${g}_${i}" ${sch.enabled ? "checked" : ""}> Aktywny</label><br>`;
            }
            html += '<br>';
          }
          document.getElementById('schedules').innerHTML = html;
        });
    }

    function saveSchedules() {
      for(let g=0; g<schedules.length; g++) {
        for(let i=0; i<schedules[g].length; i++) {
          let timeStr = document.getElementById(`time${g}_${i}`).value;
          let [hh, mm] = timeStr.split(':');
          schedules[g][i].hour = parseInt(hh);
          schedules[g][i].minute = parseInt(mm);
          schedules[g][i].durationSec = parseInt(document.getElementById(`d${g}_${i}`).value);
          schedules[g][i].enabled = document.getElementById(`e${g}_${i}`).checked;
        }
      }
      fetch('/saveSchedules', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(schedules)
      }).then(r => r.text()).then(t => alert(t));
    }

    function stopWatering() {
      fetch('/stopWatering', {method:'POST'})
        .then(r => r.text()).then(t => alert(t));
    }
  </script>
</head>
<body>
<div class="container">
  <div class="tabs">
    <button onclick="showTab('status', this)">Stan Pracy</button>
    <button onclick="showTab('schedule', this)">Ustaw Harmon</button>
   <button onclick="askPassword(this)">Serwis</button>
  </div>

  <!-- Stan Pracy -->
  <div id="status" class="tabcontent">
    <p>
      Czas: <span id="czas">--:--:--</span>
      - IP: <span id="ip">---</span>
      - WIFI: <span id="rssi">---</span><br>

      Pompa: <span id="pump" class="pump-off">
        <svg class="pump-icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
          <circle cx="50" cy="50" r="45" class="pump-bg"/>
          <g class="rotor">
            <rect x="47" y="20" width="6" height="20" class="rotor-blade" rx="2" ry="2"/>
            <rect x="47" y="60" width="6" height="20" class="rotor-blade" rx="2" ry="2"/>
            <rect x="20" y="47" width="20" height="6" class="rotor-blade" rx="2" ry="2"/>
            <rect x="60" y="47" width="20" height="6" class="rotor-blade" rx="2" ry="2"/>
            <circle cx="50" cy="50" r="10" class="pump-center"/>
          </g>
        </svg>
      </span>
    </p>

    <h3>Stany</h3>
    <div id="stany">Ładowanie...</div>

    <h3>Ręczne sterowanie</h3>
    <form onsubmit="sendManual(); return false;">
      <label>Gr:
        <select id="group">
          <option value="0">0</option>
          <option value="1">1</option>
          <option value="2">2</option>
          <option value="3">3</option>
        </select>
      
      <label>S:
        <select id="section">
          <option value="0">0</option>
          <option value="1">1</option>
        </select>
     
      <label>Stan:
        <select id="state">
          <option value="1">ON</option>
          <option value="0">OFF</option>
        </select>
      
      <label>Czas:
  <select id="duration">
    <option value="5">5 sek</option>
    <option value="10">10 sek</option>
    <option value="20">20 sek</option>
    <option value="30">30 sek</option>
    <option value="60">1 min</option>
    <option value="300">5 min</option>
    <option value="600">10 min</option>
    <option value="900">15 min</option>
    <option value="1200">20 min</option>
  </select>

      </label><br>
      <button type="submit">Start</button>

    </form>

    <pre id="resp"></pre>
  
  </div>

  <!-- Harmonogram -->
  <div id="schedule" class="tabcontent">
    <h3>Harmonogramy</h3>
    <button onclick="saveSchedules()">Zapisz Harmonogram</button>
    <button type="button" onclick="stopWatering()">Zatrzymaj podlewanie</button>
    <div id="schedules">Ładowanie...</div>
    
  </div>

  <!-- Ustawienia -->
<div id="settings" class="tabcontent">
  <h3>Ustawienia</h3>
  <p>Temperatura: <span id="tempValue">--</span> °C</p>
  <p>Stan pamięci: <span id="freeHeap">--</span> B<br>
  Czas działania: <span id="uptime">--</span></p>

  <h3>Stany przekaźników</h3>
  <div id="relayStatus">Ładowanie...</div>
  <label for="pumpMode">Tryb pracy pompy:</label>
<select id="pumpMode">
  <option value="0">Sterowanie automatyczne</option>
  <option value="1">Zawsze włączona</option>
</select>
<h3>📘 Historia podlewania</h3>
<pre id="wateringLogs">Ładowanie...</pre>
<button onclick="fetchWateringLogs()">🔄 Odśwież</button>
<button onclick="clearHistory()">🗑 Wyczyść historię</button>

</div>

<script>
window.addEventListener('load', () => {
  fetch('/api/getPumpMode')
    .then(res => res.text())
    .then(mode => {
      document.getElementById("pumpMode").value = mode;
    });

  document.getElementById("pumpMode").addEventListener("change", function() {
    fetch("/api/setPumpMode", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "alwaysOn=" + this.value
    }).then(res => {
      if (res.ok) alert("✅ Zapisano tryb pompy");
      else alert("❌ Błąd zapisu");
    });
  });
});

function fetchTemperature() {
  fetch('/getTemperature')
    .then(response => response.json())
    .then(data => {
      document.getElementById('tempValue').textContent = data.temperature.toFixed(2);
    })
    .catch(err => {
      console.error('Błąd odczytu temperatury:', err);
    });
}

// Odświeżaj co 1 sekund tylko jeśli zakładka Ustawienia jest aktywna
setInterval(() => {
  if (document.getElementById('settings').classList.contains('active')) {
    fetchTemperature();
  }
}, 1000);

function fetchWateringLogs() {
  fetch('/wateringHistory')
    .then(res => res.text())
    .then(data => {
      document.getElementById("wateringLogs").textContent = data || "(Brak wpisów)";
    })
    .catch(err => {
      document.getElementById("wateringLogs").textContent = "Błąd odczytu logów";
      console.error("Błąd logów:", err);
    });
}

function clearHistory() {
  if (confirm("❗ Czy na pewno usunąć historię podlewania?")) {
    fetch('/clearHistory', { method: "POST" })
      .then(res => res.text())
      .then(msg => {
        alert(msg);
        fetchWateringLogs();
      });
  }
}


</script>


</body>
</html>
    )rawliteral";

    request->send(200, "text/html", html);
  });


  server.on("/getTemperature", HTTP_GET, [](AsyncWebServerRequest *request){
    String jsonResponse = "{\"temperature\":";
    jsonResponse += String(currentTemperature, 2);
    jsonResponse += "}";
    request->send(200, "application/json", jsonResponse);
  });


  // Manual control POST
  server.on("/manual", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (request->hasParam("group", false) && request->hasParam("section", false) &&
      request->hasParam("state", false)) {
    int g = request->getParam("group", false)->value().toInt();
    int s = request->getParam("section", false)->value().toInt();
    int st = request->getParam("state", false)->value().toInt();
    int dur = request->hasParam("duration", false) ? request->getParam("duration", false)->value().toInt() : 0;

    if (g < NUM_GROUPS && s < SECTIONS_PER_GROUP) {
      manualStates[g][s] = st != 0;
      controlRelay(g, s, manualStates[g][s]);

      if (st != 0 && dur > 0) {
        manualStartTime[g][s] = millis();
        manualDuration[g][s] = dur;
        logWateringToSPIFFS(g, s, dur, "manual");

      } else {
        manualStartTime[g][s] = 0;
        manualDuration[g][s] = 0;
      }

      request->send(200, "text/plain", "");//ok
      return;
    }
  }
  request->send(400, "text/plain", "Błąd");//błąd
});


  // Pobierz harmonogramy (GET)
  server.on("/schedules", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int g = 0; g < NUM_GROUPS; g++) {
      json += "[";
      for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
        Schedule& s = schedules[g][i];
        json += "{\"hour\":" + String(s.hour) + 
                ",\"minute\":" + String(s.minute) + 
                ",\"durationSec\":" + String(s.durationSec) + 
                ",\"enabled\":" + (s.enabled ? "true" : "false") + "}";
        if (i < SCHEDULES_PER_GROUP - 1) json += ",";
      }
      json += "]";
      if (g < NUM_GROUPS - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // Zapis harmonogramów POST JSON - obsługa onRequestBody
  server.on("/saveSchedules", HTTP_POST, [](AsyncWebServerRequest *request) {
    // pusty handler, body obsługiwane poniżej
  }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      request->send(400, "text/plain", "Niepoprawny JSON");
      return;
    }
    for (int g = 0; g < NUM_GROUPS; g++) {
      for (int i = 0; i < SCHEDULES_PER_GROUP; i++) {
        schedules[g][i].hour = doc[g][i]["hour"] | 6;
        schedules[g][i].minute = doc[g][i]["minute"] | 0;
        schedules[g][i].durationSec = doc[g][i]["durationSec"] | 10;
        schedules[g][i].enabled = doc[g][i]["enabled"] | true;
      }
    }
    saveSchedulesToEEPROM();
    request->send(200, "text/plain", "Zapisano");
  });

  // Zatrzymanie podlewania POST
  server.on("/stopWatering", HTTP_POST, [](AsyncWebServerRequest *request) {
    stopWatering();
    request->send(200, "text/plain", "Podlewanie zatrzymane");
  });

  server.on("/api/setPumpMode", HTTP_POST, [](AsyncWebServerRequest *request){
  if (request->hasParam("alwaysOn", true)) {
    String value = request->getParam("alwaysOn", true)->value();
    pumpAlwaysOn = (value == "1");

    // Przełącz przekaźnik pompy od razu
    digitalWrite(PUMP_RELAY_PIN, pumpAlwaysOn ? LOW : HIGH);

    savePumpModeToEEPROM();  // zapisz tryb
    request->send(200, "text/plain", "Pump mode updated");
  } else {
    request->send(400, "text/plain", "Missing parameter");
  }
});

server.on("/api/getPumpMode", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(200, "text/plain", pumpAlwaysOn ? "1" : "0");
});

//
server.on("/wateringHistory", HTTP_GET, [](AsyncWebServerRequest *request){
  File logFile = SPIFFS.open("/logs.txt", FILE_READ);
  if (!logFile) {
    request->send(500, "text/plain", "Błąd odczytu logów");
    return;
  }

  String content;
  while (logFile.available()) {
    content += logFile.readStringUntil('\n') + "\n";
  }
  logFile.close();
  request->send(200, "text/plain", content);
});

server.on("/clearHistory", HTTP_POST, [](AsyncWebServerRequest *request){
  if (SPIFFS.exists("/logs.txt")) {
    SPIFFS.remove("/logs.txt");
  }
  request->send(200, "text/plain", "🗑 Historia została wyczyszczona");
});

  server.begin();
}
///////////////////////////////////////////
void saveClockToEEPROM() {
  EEPROM.put(EEPROM_TIME_ADDR, localTimeSimulated);
  EEPROM.commit();
  Serial.println("[EEPROM] Zapisano czas lokalny");
}




void loadClockFromEEPROM() {
  EEPROM.get(EEPROM_TIME_ADDR, localTimeSimulated);
  lastMillisUpdate = millis();
  ntpSynced = true;
  Serial.printf("[EEPROM] Wczytano czas lokalny: %lu (%s)\n", 
                localTimeSimulated, ctime(&localTimeSimulated));
}
////do odczyt na żywo na www
void sendLiveStatus() {
  StaticJsonDocument<512> doc;
  doc["time"] = timeClient.getEpochTime() - 7200; 
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["wifiIP"] = WiFi.localIP().toString();

  doc["pumpOn"] = pumpState;

  // Dodajemy stan pamięci i uptime
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptimeSec"] = millis() / 1000;

  JsonArray states = doc.createNestedArray("states");
  for (int g = 0; g < NUM_GROUPS; g++) {
    JsonArray group = states.createNestedArray();
    for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
      group.add(manualStates[g][s] ? 1 : 0);
    }
  }

  JsonArray remaining = doc.createNestedArray("remainingTime");
  for (int g = 0; g < NUM_GROUPS; g++) {
    JsonArray groupRemaining = remaining.createNestedArray();
    for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
      if (groupActive[g] && s == currentSection[g]) {
        unsigned long elapsed = millis() - groupStartTime[g];
        long remainingSecs = groupDuration[g] - (elapsed / 1000);
        if (remainingSecs < 0) remainingSecs = 0;
        groupRemaining.add(remainingSecs);
      } else if (manualStates[g][s] && manualDuration[g][s] > 0) {
        unsigned long elapsed = millis() - manualStartTime[g][s];
        long remainingSecs = manualDuration[g][s] - (elapsed / 1000);
        if (remainingSecs < 0) remainingSecs = 0;
        groupRemaining.add(remainingSecs);
      } else {
        groupRemaining.add(0);
      }
    }
  }

  String output;
  serializeJson(doc, output);
  events.send(output.c_str(), "status", millis());

}

////
///manual
void checkManualTimers() {
  for (int g = 0; g < NUM_GROUPS; g++) {
    for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
      if (manualStates[g][s] && manualDuration[g][s] > 0) {
        if (millis() - manualStartTime[g][s] >= manualDuration[g][s] * 1000UL) {
          controlRelay(g, s, false);
          manualStates[g][s] = false;
          manualStartTime[g][s] = 0;
          manualDuration[g][s] = 0;
          Serial.printf("[Manual Timeout] Grupa %d Sekcja %d – wyłączona po %ds\n", g, s, manualDuration[g][s]);
        }
      }
    }
  }
}




// --- Setup ---
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadPumpModeFromEEPROM();
  loadSchedulesFromEEPROM();
  sensors.begin();

  if (!SPIFFS.begin(true)) {
  Serial.println("Błąd inicjalizacji SPIFFS");
  return;
}


///display  
  display.init(135, 240);
  display.setRotation(1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

////Przekaźniki po komunikacji
  Wire.begin(21, 22);  // SDA, SCL - zmień jeśli używasz innych pinów

if (!pcf.begin()) {
    Serial.println("❌ Nie można zainicjalizować PCF8574");
   // while (1);
  }

  if (!pcf.isConnected()) {
    Serial.println("❌ PCF8574 nie jest podłączony");
  //  while (1);
  }
   Serial.println("✅ PCF8574 gotowy");
  
  // Ustaw wszystkie wyjścia w stan HIGH (nieaktywne)
  for (uint8_t i = 0; i < 8; i++) {
    pcf.write(i, HIGH);
  }
  ///

  

 pinMode(PUMP_RELAY_PIN, OUTPUT);
digitalWrite(PUMP_RELAY_PIN, pumpAlwaysOn ? LOW : HIGH);  // Tryb zależny od ustawienia


  // --- Konfiguracja WiFiManagera ---
  DNSServer dns;
  
  AsyncWiFiManager wifiManager(&server, &dns);  // użyj głównego serwera

  WiFi.mode(WIFI_STA); // ważne – tryb klienta Wi-Fi
  Serial.print("Adres MAC ESP: ");
  Serial.println(WiFi.macAddress());

  wifiManager.setTimeout(1);  // 10 sekund

  bool connected = wifiManager.autoConnect("Nawadnianie_Config");

  if (!connected) {
    Serial.println("❌ Nie udało się połączyć z WiFi – tryb Access Point.");
    loadClockFromEEPROM();  // fallback – lokalny czas z EEPROM
    Serial.println(WiFi.SSID());
  } else {
    Serial.println("✅ Połączono z WiFi: ");
    Serial.println(WiFi.SSID());
    Serial.println(WiFi.localIP());
  }

  // ✅ Te linie muszą być ZAWSZE wykonane – niezależnie od WiFi
  setupServer();
  AsyncElegantOTA.begin(&server);
    server.begin();

    

////do odczyt na żywo w wwww
  
  server.addHandler(&events);
  events.onConnect([](AsyncEventSourceClient *client){
    if (client->lastId()) {
      Serial.printf("Client reconnected, last message ID it got: %u\n", client->lastId());
    }
    client->send("connected", "init", millis());
  });
////

  timeClient.begin();   // działa nawet bez internetu
  timeClient.update();  // jeśli nie ma WiFi, użyj danych z EEPROM

  pinMode(BUTTON_PIN, INPUT_PULLUP);

 

  
for (int g = 0; g < NUM_GROUPS; g++) {
  for (int s = 0; s < SECTIONS_PER_GROUP; s++) {
    pinMode(relayPins[g][s], OUTPUT);
    digitalWrite(relayPins[g][s], HIGH);  // Domyślnie wyłączone
  }
}

  
  
}




////do odczyt na żywo na www
unsigned long lastSend = 0;
////

///-----loop-----

void loop() {

 checkSchedule();
  
if (millis() - lastTime >= 500) {
    lastTime = millis();

    sensors.requestTemperatures();
currentTemperature = sensors.getTempCByIndex(0);


  updatePumpState();  // <- to ustawi pompę zgodnie z pumpAlwaysOn
 
  checkManualTimers();

    drawScreen(currentScreen);
    sendLiveStatus();
  }

  ////do odczyt na żywo na www
  if (millis() - lastSend > 1000) {
   // sendLiveStatus();
    lastSend = millis();
  }

  ////


 static unsigned long lastTimeSync = 0;
if (WiFi.status() == WL_CONNECTED) {
  if (millis() - lastTimeSync >= 1000) {
    if (timeClient.update()) {
      localTimeSimulated = timeClient.getEpochTime();
      lastMillisUpdate = millis();
      ntpSynced = true;
    }
    lastTimeSync = millis();
  }
} else {
  // Offline – użyj lokalnego czasu symulowanego
  unsigned long delta = millis() - lastMillisUpdate;
  if (delta >= 1000) {
    localTimeSimulated += delta / 1000;
    lastMillisUpdate += (delta / 1000) * 1000;
  }



  // Automatyczna próba ponownego połączenia co minutę
  if (millis() - lastReconnectAttempt >= reconnectInterval) {
    Serial.println("[WiFi] Próba ponownego połączenia...");
    WiFi.disconnect(); // resetujemy poprzednie próby
    DNSServer dns;
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.setTimeout(1000);
  wifiManager.autoConnect("Nawadnianie_Config");
    //WiFi.begin(ssid, password);                     /////odznacz do automatu
    lastReconnectAttempt = millis();
  
  }
}



  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && buttonState == LOW) {
    currentScreen = (currentScreen + 1) % 6;
    drawScreen(currentScreen);  // Odśwież ekran po zmianie
    delay(50);  // debounce
  }
  lastButtonState = buttonState;

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    drawScreen(currentScreen);  // Odśwież tylko bieżący ekran
    lastUpdate = millis();
  }
  static unsigned long lastClockSave = 0;
if (millis() - lastClockSave >= 60UL * 60 * 1000) {  // co 60 minut
  if (localTimeSimulated != lastSavedClock) {       // tylko jeśli się zmienił
    saveClockToEEPROM();
    lastSavedClock = localTimeSimulated;
    Serial.println("💾 Zapisano czas do EEPROM (zmiana wykryta).");
  } else {
    Serial.println("⏱️ Brak zmian w czasie – pomijam zapis do EEPROM.");
  }
  lastClockSave = millis();
}


delay(10);  
}
