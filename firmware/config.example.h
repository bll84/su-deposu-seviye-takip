// =====================================================================
//  config.example.h  —  AYAR ŞABLONU
// ---------------------------------------------------------------------
//  Bu dosyayı KOPYALA, adını "config.h" yap ve kendi bilgilerini gir.
//      cp config.example.h config.h
//  config.h .gitignore'dadır; sırlar repoya ASLA gitmez.
//  Bu şablon dosyasına gerçek şifre/anahtar YAZMA — sadece örnek değerler.
// =====================================================================

#ifndef CONFIG_H
#define CONFIG_H

// ----------------------- WiFi Ayarları -------------------------------
#define WIFI_SSID       "WIFI_AGINIZIN_ADI"      // WiFi ağ adınız
#define WIFI_PASS       "WIFI_SIFRENIZ"          // WiFi şifreniz

// ----------------------- MQTT Broker Ayarları ------------------------
// MQTT_HOST: Mosquitto broker'ın IP'si veya host adı.
//   - Aynı makinede docker ise: bilgisayarınızın yerel IP'si (ör. 192.168.1.50)
//   - VPS'te ise: sunucunun IP'si veya alan adı
#define MQTT_HOST       "192.168.1.50"
#define MQTT_PORT       11883                    // Host port (compose: 11883->1883; 1883 kumesguard'da)
#define MQTT_CLIENT_ID  "su_deposu_esp8266"      // MQTT istemci kimliği (ağda benzersiz)
#define MQTT_TOPIC      "su_deposu/seviye"        // Yayın yapılacak topic (Node-RED ile AYNI olmalı)

// Broker anonim bağlantıya izin veriyorsa boş bırakın.
// Kullanıcı/şifre tanımlıysa doldurun (varsa).
#define MQTT_USER       ""
#define MQTT_PASS       ""

// ----------------------- Kalibrasyon ---------------------------------
// Sensör deponun ÜSTÜNE bakar; su yükseldikçe sensör-su mesafesi AZALIR.
//   TANK_BOS_CM  : Depo BOŞKEN ölçülen sensör-su mesafesi (EN BÜYÜK mesafe).
//   TANK_DOLU_CM : Depo DOLUYKEN ölçülen sensör-su mesafesi (EN KÜÇÜK mesafe).
// Kalibrasyon: Seri Monitör'den (115200) ham "mesafe_cm" değerini oku.
//   - Depo boşken okunan değeri TANK_BOS_CM'ye,
//   - Depo doluyken okunan değeri TANK_DOLU_CM'ye yaz.
#define TANK_BOS_CM     120.0   // örnek: depo boşken ~120 cm
#define TANK_DOLU_CM    20.0    // örnek: depo doluyken ~20 cm

// ----------------------- Davranış ------------------------------------
#define YAYIN_ARALIGI_MS  10000UL   // İki yayın arası süre (ms). Varsayılan 10 sn.

// ----------------------- Pin Tanımları -------------------------------
// NodeMCU pin etiketleri ile GPIO numaraları:
//   D1 = GPIO5,  D2 = GPIO4
// DİKKAT: ECHO 5V verir. D2'ye GERİLİM BÖLÜCÜ üzerinden bağlanır (README'ye bak).
#define TRIG_PIN        5   // D1 (GPIO5) — HC-SR04 TRIG
#define ECHO_PIN        4   // D2 (GPIO4) — HC-SR04 ECHO (gerilim bölücü sonrası)

#endif  // CONFIG_H
