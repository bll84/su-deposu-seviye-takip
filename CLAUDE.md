# CLAUDE.md — Su Deposu Seviye Takip Sistemi (Proje Pusulası)

> Bu dosya proje kökünde durur; Claude Code her oturum başında otomatik okur.
> Buradaki kurallar **çalışan tasarımı** anlatır — değiştirmeden önce nedenini sorgula.
> Kasa geneli kurallar için: `/home/bll/ubuntu_klasoru/CLAUDE.md` (bu dosya onu tamamlar).

---

## 1. Proje Mimarisi

ESP8266 + HC-SR04 ile su deposu seviyesini ölçen, MQTT üzerinden Node-RED'e taşıyan,
panelde gösteren ve eşik altına inince **tek seferlik** Telegram bildirimi atan IoT
sistemi. Hedef: MVP / demo (satış vitrini).

```
┌──────────────────┐   WiFi/MQTT    ┌────────────┐   sub    ┌──────────────┐   eşik   ┌──────────┐
│  ESP8266 +       │  publish       │ Mosquitto  │  ────►   │   Node-RED   │  ────►   │ Telegram │
│  HC-SR04 (cihaz) │  ───────────►  │  (broker)  │          │ panel + eşik │          │ bildirim │
└──────────────────┘ su_deposu/     └────────────┘          └──────────────┘          └──────────┘
                     seviye  (QoS 0)        1883                  gauge+grafik            (tek sefer)
```

- **mosquitto** ve **nodered** aynı Docker ağında. Node-RED broker'a `mosquitto:1883`
  host adıyla ulaşır. ESP8266 ise broker'a **bilgisayarın yerel IP'si:11883** ile bağlanır
  (`localhost` cihaz için çalışmaz).
- Panel: `http://localhost:11880/ui` — Editör: `http://localhost:11880`

---

## 2. MQTT Sözleşmesi (firmware ↔ Node-RED — TEK KAYNAK, İKİSİ DE AYNI)

| Alan | Değer |
|---|---|
| **Topic** | `su_deposu/seviye` |
| **QoS** | **0** (en fazla bir kez; teslim garantisi yok — demo için yeterli) |
| **Yön** | ESP8266 publish → Node-RED subscribe |
| **Yayın aralığı** | `YAYIN_ARALIGI_MS` (varsayılan **10000 ms = 10 sn**, `config.h`'den ayarlanır) |

**Payload (Türkçe anahtarlar — DEĞİŞTİRME, iki tarafı birden bozar):**
```json
{"yuzde": 65, "mesafe_cm": 41.3, "zaman": 123456}
```
- `yuzde` — int, **0–100 arası clamp** (taşma yok)
- `mesafe_cm` — float, ham sensör-su mesafesi (medyan filtreli)
- `zaman` — `millis()` (cihaz açılışından bu yana ms; **gerçek saat/NTP yok**)

> ⚠️ Anahtar adları, topic veya QoS'u **tek tarafta** değiştirmek zinciri kırar.
> İngilizce anahtar / ISO timestamp / `status` alanı bu tasarımda **yok** — eklemek
> NTP senkronu + iki tarafı yeniden yazmayı gerektirir (bilinçli olarak tercih edilmedi).

---

## 3. Pin Haritası (NodeMCU ESP8266)

| HC-SR04 | NodeMCU etiket | GPIO | Not |
|---|---|---|---|
| TRIG | **D1** | GPIO5 | Doğrudan bağlanır |
| ECHO | **D2** | GPIO4 | **GERİLİM BÖLÜCÜ ZORUNLU** (R1=1kΩ, R2=2kΩ → 5V'u ~3.3V'a düşür) |
| VCC | Vin / VU | — | HC-SR04 5V ile çalışır |
| GND | GND | — | Ortak toprak |

> ⚠️ ECHO 5V verir, ESP8266 pinleri 3.3V toleranslı. Gerilim bölücüsüz bağlarsan
> **pin yanar** (diyagram README §1'de). Pin tanımları `config.example.h` içindedir;
> kod sabit pin no kullanmaz, makrodan okur.

---

## 4. Kodlama Kuralları (mevcut firmware'in fiili stili — uyumlu kal)

- **Değişken / fonksiyon adları:** Türkçe **camelCase** — `medyanMesafe()`, `sonGecerliYuzde`,
  `wifiKontrolEt()`. (Spec "snake_case" demişti ama repodaki gerçek stil camelCase'tir.)
- **Sabitler / makrolar:** `UPPER_SNAKE_CASE` — `TANK_BOS_CM`, `YAYIN_ARALIGI_MS`, `ECHO_PIN`.
- **Her fonksiyonun başında yorum:** ne yapar + dönüş değeri + edge case (örn. "geçersizse -1.0").
- **Ana döngü bloklamaz (non-blocking):** `loop()` içinde `delay()` yok; WiFi/MQTT yeniden
  bağlanma `millis()` aralığıyla yapılır. (Tek istisna: HC-SR04 örnekleme arası `delay(40)`
  — sensörün fiziksel gereği, ana akışı kilitleyecek yerde değil.)
- **Sır YOK:** firmware'de WiFi/MQTT bilgisi yalnızca `config.h`'den gelir (gitignore'lu).
- **Türkçe Serial log etiketleri:** `[WiFi]`, `[MQTT]`, `[OLCUM]`.

---

## 5. Sır Hijyeni (bu projede nasıl uygulanır)

- **Firmware sırları** (WiFi/MQTT) → yalnızca `firmware/config.h` (`.gitignore`'da).
  Şablon `config.example.h`'ye gerçek değer **yazılmaz**.
- **Telegram bot token + chat_id** → **Node-RED'in şifreli credential deposunda**
  (`node-red/*_cred.json`, gitignore'lu) ve eşik fonksiyon node'unda chat_id.
  Token `flows.json`'a **düz metin yazılmaz**, `config.h`'ye de konmaz.
- Sır gördüğünde maskele: `123456789:AAE****…****xxxx`.

---

## 6. Bilinen Limitler

- **HC-SR04 kör bölge (~2 cm):** sensöre çok yakın yüzeyi ölçemez; depo tam dolu sınırında
  sapma olabilir. `TANK_DOLU_CM`'yi 2 cm'in altına ayarlama.
- **MQTT QoS 0:** teslim garantisi yok; WiFi anlık koparsa o yayın kaybolabilir
  (10 sn sonra yenisi gelir, kritik değil).
- **Gerçek saat yok:** `zaman` alanı `millis()`; mutlak zaman damgası gerekirse NTP eklenmeli.
- **WiFi gecikmesi / kopması:** firmware donmadan yeniden bağlanır; bu sırada panel veri
  almayabilir (son geçerli değer cihazda korunur).
- **`allow_anonymous true`:** broker demo modunda kimlik doğrulamasız. **Üretimde**
  kullanıcı/şifre veya TLS eklenmeli (mosquitto.conf'ta not var).

---

## 7. Bellek / Dosya Açma Tetikleyicileri

Kasa kuralı (`/home/bll/ubuntu_klasoru/CLAUDE.md` §4) burada da geçerli:

| Tetik | Hedef |
|---|---|
| Mimari/teknoloji/fiyat kararı | `kararlar/YYYY-MM-DD-<konu>.md` (frontmatter'lı) |
| Hata + çözüm | `hatalar/YYYY-MM-DD-<hata>.md` |
| Durum özeti (`/durum`) | `DURUM-YYYY-AA-GG-SSDD.md` (kökte) |

---

## 8. Ajan / Araç Kullanımı (bu projeye özel doktorlar)

İlgili tanı doktorları (hepsi **salt-okuma** — değişikliği lider yapar):

| Durum | Çağrılacak doktor |
|---|---|
| ESP8266 boot/flash/firmware mantığı | `iot-yazici` (yazım), `esphome-doktor` (YAML tanısı — bu proje Arduino .ino, ESPHome değil; sınırlı fayda) |
| MQTT veri akmıyor, sensör veri göndermiyor | `sensor-doktor` (ESP→mosquitto→Node-RED pipeline) |
| Mosquitto / Node-RED container ayağa kalkmıyor, restart loop | `docker-doktor` |
| Node-RED akış/dashboard/gauge bozuk, deploy görünmüyor | `node-red-doktor` |
| Kod kalitesi / güvenlik / sır sızması kapısı | `inceleyici-doktor` |

- **Genel kod yazımı / düzeltme:** lider (ana ajan) yapar.
- **"takım kur" tetiği:** kasa playbook'u çalışır (bkz. kasa CLAUDE.md "takım kur").

---

## ⛔ YAPMA

- MQTT payload anahtarlarını / topic / QoS'u **tek tarafta** değiştirme.
- ECHO'yu gerilim bölücüsüz bağlama.
- Telegram token'ı veya WiFi şifresini repoya / `config.h` dışına / `flows.json`'a yazma.
- Çalışan Türkçe-anahtar şemasını veya D1/D2 pin haritasını gerekçesiz değiştirme.
