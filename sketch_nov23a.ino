/*
  Arduino UNO + GY-GPS6MV2 (u-blox NEO-6M)
  Cada 2 s muestra:
   - tabla de satélites a la vista: número, elevación, azimut (dirección)
     y potencia de la señal (C/N0 en dB-Hz) con una barra
   - satélites en uso, tipo de fix, HDOP, coordenadas y hora UTC

  Conexiones:
    GPS VCC -> 5V
    GPS GND -> GND
    GPS TX  -> D4
    GPS RX  -> sin conectar

  Librería: TinyGPSPlus (Mikal Hart). Monitor serie a 115200.
*/

#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

SoftwareSerial gpsSerial(4, 7);   // RX = D4 (D7 sin usar)
TinyGPSPlus gps;

const bool SHOW_NMEA = false;     // true para ver también las frases en crudo

// --- Campos de $GPGSV (satélites a la vista) ---
TinyGPSCustom gsvTotal(gps, "GPGSV", 1);
TinyGPSCustom gsvMsg(gps, "GPGSV", 2);
TinyGPSCustom gsvInView(gps, "GPGSV", 3);
TinyGPSCustom satNum[4], satElev[4], satAz[4], satSnr[4];
// --- Tipo de fix de $GPGSA (1 = sin fix, 2 = 2D, 3 = 3D) ---
TinyGPSCustom gsaMode(gps, "GPGSA", 2);

struct Sat { uint8_t prn; int8_t elev; int16_t az; uint8_t snr; };
const uint8_t MAX_SATS = 16;
Sat building[MAX_SATS]; uint8_t nBuilding = 0;
Sat shown[MAX_SATS];    uint8_t nShown = 0;
uint8_t inViewShown = 0;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);
  for (uint8_t i = 0; i < 4; i++) {
    satNum[i].begin(gps, "GPGSV", 4 + 4 * i);
    satElev[i].begin(gps, "GPGSV", 5 + 4 * i);
    satAz[i].begin(gps, "GPGSV", 6 + 4 * i);
    satSnr[i].begin(gps, "GPGSV", 7 + 4 * i);
  }
  Serial.println(F("\nGPS NEO-6M + Arduino UNO: tabla de satelites"));
}

int fieldOr(TinyGPSCustom &f, int empty) {
  const char *v = f.value();
  return v[0] ? atoi(v) : empty;
}

void handleGSV() {
  uint8_t total = atoi(gsvTotal.value());
  uint8_t msg = atoi(gsvMsg.value());
  uint8_t inView = atoi(gsvInView.value());
  if (msg == 1) nBuilding = 0;
  int n = (int)inView - (msg - 1) * 4;     // satélites en este mensaje
  if (n > 4) n = 4;
  for (int i = 0; i < n && nBuilding < MAX_SATS; i++) {
    Sat &s = building[nBuilding++];
    s.prn  = fieldOr(satNum[i], 0);
    s.elev = fieldOr(satElev[i], -1);
    s.az   = fieldOr(satAz[i], -1);
    s.snr  = fieldOr(satSnr[i], 0);         // 0 = lo ve en la órbita pero no lo recibe
  }
  if (msg == total) {
    memcpy(shown, building, sizeof(Sat) * nBuilding);
    nShown = nBuilding;
    inViewShown = inView;
  }
}

const char *dirName(int az) {
  static const char *names[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
  if (az < 0) return "?";
  return names[((az + 22) / 45) % 8];
}

void printTable() {
  char line[64];
  Serial.println(F("\n================================================"));
  Serial.print(F("Satelites a la vista: ")); Serial.print(inViewShown);
  Serial.print(F("   En uso: "));
  Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
  Serial.print(F("   Fix: "));
  const char *m = gsaMode.value();
  Serial.println(m[0] == '3' ? F("3D") : m[0] == '2' ? F("2D") : F("ninguno"));
  Serial.println(F(" PRN  Elev  Azim  Dir  Senal"));
  for (uint8_t i = 0; i < nShown; i++) {
    Sat &s = shown[i];
    char elev[6], az[6];
    if (s.elev >= 0) snprintf(elev, sizeof(elev), "%3d", s.elev); else strcpy(elev, "  -");
    if (s.az >= 0) snprintf(az, sizeof(az), "%3d", s.az); else strcpy(az, "  -");
    snprintf(line, sizeof(line), " %3d   %s   %s  %-3s  ", s.prn, elev, az, dirName(s.az));
    Serial.print(line);
    if (s.snr == 0) {
      Serial.print(F("-- (no se recibe)"));
    } else {
      Serial.print(s.snr); Serial.print(F(" dB-Hz "));
      for (uint8_t b = 0; b < s.snr / 3; b++) Serial.print('#');
    }
    if (s.prn >= 33) Serial.print(F("  (SBAS)"));
    Serial.println();
  }
  if (nShown == 0) Serial.println(F("  (ningun satelite todavia)"));

  Serial.print(F("HDOP: "));
  if (gps.hdop.isValid()) Serial.println(gps.hdop.hdop(), 1); else Serial.println(F("-"));
  if (gps.location.isValid()) {
    Serial.print(F("Posicion: "));
    Serial.print(gps.location.lat(), 6); Serial.print(F(", "));
    Serial.println(gps.location.lng(), 6);
    Serial.print(F("Altitud: ")); Serial.print(gps.altitude.meters(), 1); Serial.println(F(" m"));
    Serial.print(F("Mapa: https://maps.google.com/?q="));
    Serial.print(gps.location.lat(), 6); Serial.print(',');
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println(F("Posicion: sin fix todavia"));
  }
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2000) {
    snprintf(line, sizeof(line), "UTC: %04d-%02d-%02d %02d:%02d:%02d",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    Serial.println(line);
  }
}

void loop() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (SHOW_NMEA) Serial.write(c);
    if (gps.encode(c) && gsvTotal.isUpdated()) handleGSV();
  }
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();
    if (gps.charsProcessed() < 10) Serial.println(F("No llegan datos del GPS: revisa TX -> D4, 5V y GND."));
    else printTable();
  }
}
