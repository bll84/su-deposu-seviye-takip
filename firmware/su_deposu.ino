// =====================================================================
//  Su Deposu Seviye Takip Sistemi  —  ESP8266 Firmware
// ---------------------------------------------------------------------
//  Donanım : NodeMCU ESP8266 + HC-SR04 ultrasonik mesafe sensörü
//  Görev   : Mesafeyi ölç -> yüzdeye çevir -> MQTT'ye JSON yayınla
//
//  Akış    : [ESP8266 + HC-SR04] --WiFi/MQTT--> [Mosquitto] --> [Node-RED]
//
//  Kütüphaneler (Arduino IDE -> Kütüphane Yöneticisi):
//    - ESP8266WiFi   (ESP8266 kart paketi ile gelir)
//    - PubSubClient  (Nick O'Leary) — MQTT istemcisi
//
//  GÜVENLİK: HC-SR04 ECHO pini 5V verir, ESP8266 3.3V toleranslıdır.
//            ECHO ile D2 arasına GERİLİM BÖLÜCÜ ZORUNLU (README'ye bak).
//
//  Sırlar (WiFi/MQTT) bu dosyada DEĞİL — config.h içindedir.
// =====================================================================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"   // WiFi/MQTT/kalibrasyon ayarları (kopyalanıp doldurulur)

// ----------------------- Sabitler ------------------------------------
const float SES_BOLEN      = 58.0;     // pulseIn süresi(us) / 58 = mesafe(cm)
const float MENZIL_MAX_CM  = 400.0;    // HC-SR04 üst menzil sınırı
const int   ORNEK_SAYISI   = 5;        // Her ölçümde alınacak örnek sayısı (medyan için)
const unsigned long ECHO_TIMEOUT_US = 25000UL;  // ~430cm karşılığı; daha uzaksa geçersiz

// ----------------------- Global Durum --------------------------------
WiFiClient   wifiIstemci;
PubSubClient client(wifiIstemci);

int   sonGecerliYuzde   = -1;     // En son geçerli yüzde (henüz yoksa -1)
float sonGecerliMesafe  = -1.0;   // En son geçerli mesafe (cm)

unsigned long sonYayinMs    = 0;  // En son MQTT yayını zamanı
unsigned long sonMqttDeneme = 0;  // En son MQTT reconnect denemesi zamanı
const unsigned long MQTT_DENEME_ARALIGI_MS = 5000UL;  // MQTT yeniden bağlanma aralığı

// =====================================================================
//  WiFi — bloklamayan bağlantı
//  setup()'ta WiFi.begin çağrılır; loop()'ta durum kontrol edilir.
// =====================================================================
void wifiBaslat() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);            // flash aşınmasını azalt
  WiFi.setAutoReconnect(true);       // kopunca çekirdek otomatik dener
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("[WiFi] Baglaniliyor: "));
  Serial.println(WIFI_SSID);
}

// loop içinde çağrılır; SADECE durumu izler/loglar — BLOKLAMAZ.
// Yeniden bağlanmayı ESP'nin kendi setAutoReconnect(true) mekanizması yapar;
// burada WiFi.begin ÇAĞRILMAZ. (Mükerrer begin, zaten iyi olan bağlantıyı
// yeniden-ilişkilendirmeye zorlayıp kopmalara yol açıyordu.)
void wifiKontrolEt() {
  static bool oncedenBagliydi = false;
  static unsigned long sonHatirlatma = 0;

  if (WiFi.status() == WL_CONNECTED) {
    if (!oncedenBagliydi) {
      Serial.print(F("[WiFi] Baglandi. IP: "));
      Serial.println(WiFi.localIP());
      oncedenBagliydi = true;
    }
    return;
  }

  // Bağlı değil: auto-reconnect arka planda dener; burada SADECE bilgi logla.
  unsigned long simdi = millis();
  if (oncedenBagliydi) {
    // Bağlıyken koptu: bir kez uyar.
    Serial.println(F("[WiFi] Baglanti koptu — otomatik yeniden baglanma bekleniyor."));
    oncedenBagliydi = false;
    sonHatirlatma = simdi;
  } else if (simdi - sonHatirlatma >= 10000UL) {
    // Henüz hiç bağlanmadı / bekliyor: 10 sn'de bir hatırlat (spam yok).
    sonHatirlatma = simdi;
    Serial.println(F("[WiFi] Baglanti bekleniyor (auto-reconnect aktif)..."));
  }
}

// =====================================================================
//  MQTT — bloklamayan yeniden bağlanma
// =====================================================================
void mqttBaglanmayiDene() {
  // WiFi yoksa MQTT denemenin anlamı yok.
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long simdi = millis();
  if (simdi - sonMqttDeneme < MQTT_DENEME_ARALIGI_MS) return;  // henüz erken
  sonMqttDeneme = simdi;

  Serial.print(F("[MQTT] Broker'a baglaniliyor... "));
  bool baglandi;
  // MQTT_USER boşsa anonim, doluysa kimlikli bağlan.
  if (strlen(MQTT_USER) > 0) {
    baglandi = client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
  } else {
    baglandi = client.connect(MQTT_CLIENT_ID);
  }

  if (baglandi) {
    Serial.println(F("baglandi."));
  } else {
    Serial.print(F("basarisiz, rc="));
    Serial.print(client.state());
    Serial.println(F(" (5 sn sonra tekrar)"));
  }
}

// =====================================================================
//  HC-SR04 — tek mesafe ölçümü
//  Dönüş: cm cinsinden mesafe; geçersizse -1.0
// =====================================================================
float tekMesafeOlc() {
  // TRIG: 10us'lik tetik darbesi
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // ECHO: HIGH süresini ölç (mikrosaniye). Timeout olursa 0 döner.
  unsigned long sure = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (sure == 0) return -1.0;  // okuma yok / menzil dışı

  float mesafe = sure / SES_BOLEN;
  // Geçerli aralık dışını ele (0 veya >400cm).
  if (mesafe <= 0.0 || mesafe > MENZIL_MAX_CM) return -1.0;
  return mesafe;
}

// Küçük diziler için basit insertion sort (medyan almak amacıyla).
void diziSirala(float* dizi, int n) {
  for (int i = 1; i < n; i++) {
    float anahtar = dizi[i];
    int j = i - 1;
    while (j >= 0 && dizi[j] > anahtar) {
      dizi[j + 1] = dizi[j];
      j--;
    }
    dizi[j + 1] = anahtar;
  }
}

// =====================================================================
//  Medyan mesafe — 5 örnek al, geçersizleri ele, medyanı döndür.
//  Hiç geçerli örnek yoksa -1.0 döner.
// =====================================================================
float medyanMesafe() {
  float gecerliler[ORNEK_SAYISI];
  int adet = 0;

  for (int i = 0; i < ORNEK_SAYISI; i++) {
    float m = tekMesafeOlc();
    if (m > 0.0) {
      gecerliler[adet++] = m;
    }
    delay(40);  // HC-SR04 ardışık ölçüm arası önerilen min ~60ms; ölçüm süresi dahil yeterli
  }

  if (adet == 0) return -1.0;          // tüm örnekler geçersiz

  diziSirala(gecerliler, adet);
  return gecerliler[adet / 2];          // ortadaki eleman = medyan
}

// =====================================================================
//  Yüzde hesapla — mesafeyi 0-100 aralığına map'le + clamp.
//  Sensör-su mesafesi azaldıkça (su yükseldikçe) yüzde artar.
// =====================================================================
int yuzdeHesapla(float mesafe) {
  // map: BOS_CM -> %0,  DOLU_CM -> %100  (lineer)
  float oran = (TANK_BOS_CM - mesafe) / (TANK_BOS_CM - TANK_DOLU_CM);
  int yuzde = (int)(oran * 100.0 + 0.5);  // yuvarla

  // Taşmayı engelle: her zaman 0-100.
  if (yuzde < 0)   yuzde = 0;
  if (yuzde > 100) yuzde = 100;
  return yuzde;
}

// =====================================================================
//  MQTT'ye JSON yayınla.
//  Payload: {"yuzde":<int>,"mesafe_cm":<float>,"zaman":<millis>}
// =====================================================================
void yayinla(int yuzde, float mesafe) {
  char yuk[96];
  snprintf(yuk, sizeof(yuk),
           "{\"yuzde\":%d,\"mesafe_cm\":%.1f,\"zaman\":%lu}",
           yuzde, mesafe, millis());

  bool ok = client.publish(MQTT_TOPIC, yuk);
  Serial.print(F("[MQTT] Yayin "));
  Serial.print(ok ? F("OK   ") : F("HATA "));
  Serial.println(yuk);
}

// =====================================================================
//  setup
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("=== Su Deposu Seviye Takip — ESP8266 ==="));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  wifiBaslat();

  client.setServer(MQTT_HOST, MQTT_PORT);

  sonYayinMs = millis() - YAYIN_ARALIGI_MS;  // ilk ölçümü hemen yap
}

// =====================================================================
//  loop — bloklamayan ana döngü
// =====================================================================
void loop() {
  // 1) Bağlantıları canlı tut (delay'siz).
  wifiKontrolEt();
  if (!client.connected()) {
    mqttBaglanmayiDene();
  }
  client.loop();  // MQTT keep-alive / gelen mesaj işleme

  // 2) Zamanı geldiyse ölç ve yayınla.
  unsigned long simdi = millis();
  if (simdi - sonYayinMs >= YAYIN_ARALIGI_MS) {
    sonYayinMs = simdi;

    float mesafe = medyanMesafe();
    if (mesafe > 0.0) {
      // Geçerli okuma: durumu güncelle.
      sonGecerliMesafe = mesafe;
      sonGecerliYuzde  = yuzdeHesapla(mesafe);
      Serial.print(F("[OLCUM] mesafe="));
      Serial.print(mesafe, 1);
      Serial.print(F(" cm  ->  yuzde="));
      Serial.println(sonGecerliYuzde);
    } else {
      // Geçersiz okuma: son geçerli değeri KORU.
      Serial.println(F("[OLCUM] Gecersiz okuma — son gecerli deger korunuyor."));
    }

    // Elimizde hiç geçerli değer yoksa yayınlama.
    if (sonGecerliYuzde >= 0 && client.connected()) {
      yayinla(sonGecerliYuzde, sonGecerliMesafe);
    }
  }
}
