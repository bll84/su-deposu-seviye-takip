// =====================================================================
//  Node-RED settings.js  —  Su Deposu Seviye Takip Sistemi
// ---------------------------------------------------------------------
//  OPSİYONEL / REFERANS dosyası.
//
//  Mevcut kurulum (docker-compose.yml) Node-RED'i `nodered_data` volume'u
//  ile çalıştırır ve image'ın VARSAYILAN settings.js'ini kullanır — bu yüzden
//  sistem bu dosya OLMADAN da çalışır. Burası "ayarları açıkça sabitlemek
//  isteyen" için hazır bir şablondur.
//
//  ⚠️ ÖNEMLİ — credentialSecret tuzağı:
//  Node-RED, ilk çalıştığında otomatik bir anahtar üretip credential'ları
//  (örn. Telegram token) onunla şifreler ve `/data/.config.runtime.json`
//  içinde saklar. Eğer ÇALIŞAN bir kuruluma SONRADAN `credentialSecret`
//  eklersen, eski anahtar geçersiz olur ve **Telegram token'ı yeniden
//  girmen gerekir**. Yani:
//    - Yeni/temiz kurulum  → credentialSecret baştan ayarla (önerilir).
//    - Çalışan kurulum     → DOKUNMA, varsayılanda bırak (token korunur).
//
//  Bu dosyayı kullanmak istersen container'a şu yolla mount et:
//    nodered:
//      volumes:
//        - nodered_data:/data
//        - ./node-red/settings.js:/data/settings.js:ro   # <-- ekle
//  (docker-compose.yml'i Seçenek A gereği DEĞİŞTİRMEDİK; isteyen elle ekler.)
// =====================================================================

module.exports = {
    // --- Ağ / port ---
    uiPort: process.env.PORT || 1880,        // editör + dashboard portu

    // --- Akış dosyası ---
    flowFile: 'flows.json',                  // /data/flows.json
    flowFilePretty: true,                    // diff'i okunur tutar

    // --- Credential şifreleme ---
    // Sırrı KODA GÖMME. Ortam değişkeninden oku. Tanımlı değilse Node-RED
    // kendi otomatik anahtarını kullanmaya devam eder (çalışan kurulum korunur).
    // Temiz kurulumda compose'da: environment: NODE_RED_CREDENTIAL_SECRET=...
    credentialSecret: process.env.NODE_RED_CREDENTIAL_SECRET || undefined,

    // --- Çalışma zamanı / log ---
    uiHost: '0.0.0.0',
    logging: {
        console: {
            level: 'info',                   // info | debug | trace
            metrics: false,
            audit: false
        }
    },

    // --- Editör tema/dil ---
    editorTheme: {
        page: { title: 'Su Deposu — Node-RED' },
        // node-red-dashboard paneli buradan yayınlanır:
        //   http://localhost:1880/ui
        projects: { enabled: false }
    },

    // --- Function node global context ---
    // Eşik fonksiyonunun ihtiyaç duyabileceği yardımcılar buraya eklenir.
    functionGlobalContext: {
        // ornek: os: require('os'),
    },

    // --- Dashboard (node-red-dashboard) yolu ---
    ui: { path: 'ui' },                      // -> http://localhost:1880/ui

    // --- Çeşitli ---
    exportGlobalContextKeys: false,
    mqttReconnectTime: 15000,                // broker koparsa yeniden bağlanma (ms)
    socketReconnectTime: 10000
};
