# Su Deposu Seviye Takip Sistemi

ESP8266 + HC-SR04 ultrasonik sensör ile bir su deposunun seviyesini ölçen,
WiFi/MQTT üzerinden Node-RED'e gönderen, panelde canlı gösteren ve seviye eşik
altına inince **tek seferlik** Telegram bildirimi atan IoT sistemi. MVP / demo.

```
[ESP8266 + HC-SR04] --WiFi/MQTT--> [Mosquitto] --> [Node-RED] --> [Telegram]
       (cihaz)          (yayın)      (broker)       (panel+eşik)   (bildirim)
```

---

## 1. Donanım Bağlantısı

| HC-SR04 | NodeMCU ESP8266 | Not |
|---------|-----------------|-----|
| VCC     | Vin / VU (5V)   | Sensör 5V ile çalışır |
| GND     | GND             | Ortak toprak |
| TRIG    | D1 (GPIO5)      | Doğrudan bağlanır |
| ECHO    | D2 (GPIO4)      | **GERİLİM BÖLÜCÜ ÜZERİNDEN** (aşağı bak) |

### ⚠️ KRİTİK GÜVENLİK — ECHO Gerilim Bölücü (ZORUNLU)

HC-SR04'ün **ECHO** pini **5V** sinyal verir. ESP8266 pinleri yalnızca **3.3V**
toleranslıdır. ECHO'yu doğrudan bağlarsan **pin yanar**. Araya basit bir
gerilim bölücü (R1 = 1kΩ, R2 = 2kΩ) koyarak 5V'u ~3.3V'a düşür:

```
   HC-SR04 ECHO (5V)
        │
        ├───[ R1 = 1kΩ ]───┐
        │                  │
        │              orta nokta ───────►  NodeMCU D2 (GPIO4)   (~3.3V)
        │                  │
        │              [ R2 = 2kΩ ]
        │                  │
       ─┴─                GND
```

Formül: `Vout = 5V × R2 / (R1 + R2) = 5 × 2k / 3k ≈ 3.33V` ✔ güvenli.

> Elinde 1k/2k yoksa: 2 adet 1kΩ'u seri bağlayıp 2kΩ elde edebilirsin, ya da
> benzer oranı koruyan (R2 ≈ 2×R1) başka değerler kullanabilirsin.

---

## 2. Firmware (ESP8266)

### Gerekli Arduino kütüphaneleri
- **ESP8266 kart paketi** (Boards Manager → "esp8266 by ESP8266 Community")
- **ESP8266WiFi** (kart paketiyle gelir)
- **PubSubClient** (Kütüphane Yöneticisi → "PubSubClient" by Nick O'Leary)

### Ayar dosyasını hazırla (sırlar koda gömülmez)
```bash
cd firmware
cp config.example.h config.h
```
`config.h`'yi aç ve doldur: `WIFI_SSID`, `WIFI_PASS`, `MQTT_HOST` (broker IP'si),
kalibrasyon değerleri vb. **`config.h` `.gitignore`'dadır, repoya gitmez.**

### Yükleme
1. Arduino IDE → Kart: **NodeMCU 1.0 (ESP-12E Module)**.
2. `firmware/su_deposu.ino` dosyasını aç (config.h aynı klasörde olmalı).
3. Yükle → **Seri Monitör'ü 115200 baud** ile aç.
4. Şu logları görmelisin: WiFi bağlandı → MQTT bağlandı → periyodik `[OLCUM]` / `[MQTT] Yayin OK`.

---

## 3. Kalibrasyon

Sensör deponun üstünden suya bakar. Su yükseldikçe **mesafe azalır**.

1. `config.h`'de geçici değerlerle firmware'i yükle, Seri Monitör'ü aç.
2. **Depo boşken** okunan ham `mesafe_cm` değerini not al → `TANK_BOS_CM`.
3. **Depo doluyken** okunan ham `mesafe_cm` değerini not al → `TANK_DOLU_CM`.
4. Bu iki değeri `config.h`'ye yaz, yeniden yükle.

```c
#define TANK_BOS_CM   120.0   // depo boşken sensör-su mesafesi (en büyük)
#define TANK_DOLU_CM   20.0   // depo doluyken sensör-su mesafesi (en küçük)
```
Firmware bu aralığı 0–100 %'ye map'ler ve **her zaman 0–100 arasında clamp'ler**.

---

## 4. Docker (Mosquitto + Node-RED)

### Ayağa kaldırma
```bash
cd docker
docker compose up -d
docker compose ps          # ikisi de "Up" olmalı
```
- Mosquitto broker → `localhost:11883` (ESP8266 buraya bağlanır; host 11883 → container 1883)
- Node-RED → `http://localhost:11880` (editör), `http://localhost:11880/ui` (panel)

> **Önemli:** ESP8266 ve bilgisayar **aynı ağda** olmalı. `config.h`'deki
> `MQTT_HOST`, Docker'ı çalıştıran bilgisayarın **yerel IP'si** olmalı
> (örn. `192.168.1.50`) — `localhost` ESP8266 için çalışmaz.

### Durdurma
```bash
docker compose down        # volume'lar (flows, kalibrasyon) korunur
```

---

## 5. Node-RED Akışını Yükleme

### a) Gerekli paletleri kur
Node-RED editöründe (sağ üst menü) **Manage Palette → Install** sekmesinden:
- `node-red-dashboard`  → gauge + grafik (panel) için
- `node-red-contrib-telegrambot`  → Telegram bildirimi için

İkisini de kurduktan sonra Node-RED'i bir kez yeniden başlat
(`docker compose restart nodered`).

### b) Akışı içe aktar
Menü → **Import** → `node-red/flows.json` içeriğini yapıştır → **Import** → **Deploy**.

Akış şeması:
```
MQTT in (su_deposu/seviye) → JSON ayrıştır ┬→ Yüzde çıkar → Gauge + Grafik
                                            └→ Eşik kontrol (debounce) → Telegram
```

---

## 6. Telegram Bot Kurulumu

> Token `flows.json`'a **düz metin yazılmaz** — Node-RED'in şifreli credential
> deposunda (`*_cred.json`) tutulur. Bu dosya `.gitignore`'dadır.

1. Telegram'da **@BotFather**'a yaz → `/newbot` → bir bot adı ver → **token** al
   (örn. `123456789:AAExxxx…` — bunu kimseyle paylaşma).
2. Botunla bir sohbet başlat ("Start"a bas) ki sana mesaj atabilsin.
3. **chat_id'ni öğren:** botuna bir mesaj at, sonra tarayıcıda aç:
   `https://api.telegram.org/bot<TOKEN>/getUpdates` → dönen JSON'da
   `"chat":{"id":123456789}` → bu sayı senin `chat_id`'in.
4. **Node-RED'de token'ı gir:** akıştaki **"Telegram bildirimi"** node'una çift
   tıkla → bot config'ini (kalem ikonu) düzenle → **Token** alanına yapıştır → kaydet.
5. **chat_id'yi gir:** **"Eşik kontrol + debounce"** function node'unu aç,
   en üstteki `const CHAT_ID = "";` satırına kendi chat_id'ni yaz (örn. `"123456789"`).
   Aynı yerden `const ESIK = 20;` ile bildirim eşiğini de değiştirebilirsin.
6. **Deploy** et.

---

## 7. MQTT Sözleşmesi (firmware ↔ Node-RED AYNI olmalı)

- **Topic:** `su_deposu/seviye`
- **Payload:** `{"yuzde": <int 0-100>, "mesafe_cm": <float>, "zaman": <millis>}`

Bir tarafta değiştirirsen diğerini de güncelle.

---

## 8. Hızlı Test (cihaz olmadan)

Firmware'i beklemeden, sahte mesaj yayınlayıp tüm zinciri test edebilirsin:

```bash
# Eşik altı (%15) → gauge düşer + Telegram bildirimi gelir
docker exec -it su_deposu_mosquitto \
  mosquitto_pub -t su_deposu/seviye \
  -m '{"yuzde":15,"mesafe_cm":120.5,"zaman":12345}'

# Aynı mesajı tekrar yolla → İKİNCİ bildirim GELMEMELİ (debounce çalışıyor)

# Eşik üstü (%60) → durum sıfırlanır
docker exec -it su_deposu_mosquitto \
  mosquitto_pub -t su_deposu/seviye \
  -m '{"yuzde":60,"mesafe_cm":40.0,"zaman":23456}'

# Tekrar eşik altı (%10) → YENİ bildirim gelir
docker exec -it su_deposu_mosquitto \
  mosquitto_pub -t su_deposu/seviye \
  -m '{"yuzde":10,"mesafe_cm":130.0,"zaman":34567}'
```
Paneli `http://localhost:11880/ui` adresinden izle.

---

## 9. Davranış / Edge Case Garantileri

- Sensör **0** veya **menzil dışı (>400cm)** okursa değer **yok sayılır**, son geçerli korunur.
- WiFi/MQTT koparsa firmware **donmadan** (bloklamadan) yeniden bağlanır.
- Yüzde **her zaman 0–100** arasında (taşma yok).
- Bildirim **spam yapmaz**: eşik altına ilk inişte 1 kez; üste çıkıp tekrar inince yeniden.

---

## 10. Dosya Yapısı

```
firmware/
  su_deposu.ino        ESP8266 firmware
  config.example.h     ayar şablonu (kopyala → config.h)
node-red/
  flows.json           Node-RED akışı (panel + eşik + Telegram)
docker/
  docker-compose.yml   Mosquitto + Node-RED
  mosquitto/config/mosquitto.conf
.gitignore
README.md
```

## ⛔ YAPMA
- Sırları (config.h, Telegram token) repoya commit etme.
- ECHO'yu gerilim bölücüsüz doğrudan ESP8266'ya bağlama.
- MQTT topic/payload şemasını tek tarafta değiştirme.
