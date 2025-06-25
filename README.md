ESP32 Voice Assistant with Wi-Fi, OLED Animation and AI Response

Bu proje, ESP32 geliştirici kartı kullanarak sesli asistan oluşturmayı amaçlar. Ses kaydedilir, sunucuya gönderilir, metne çevrilir ve ChatGPT ile yanıt oluşturularak geri oynatılır. Proje aşağıdaki bileşenleri içerir:

 🧠 Özellikler

- 🎙️ Mikrofon ile gerçek zamanlı ses kaydı (chunked şekilde)
- 📡 Wi-Fi ile sunucuya veri aktarımı
- 🤖 OpenAI ChatGPT entegrasyonu
- 🗣️ Yanıtları TTS ile seslendirme ve ESP32 üzerinden oynatma
- 👁️ OLED ekran üzerinden göz animasyonu
- 🔊 Ses seviyesi %200 artırma işlemi (Python tarafında)

🧱 Kullanılan Donanım Kartları ve Bileşenler
Bileşen	Model / Açıklama
🧠 Geliştirme Kartı	ESP32 DevKit v1 (Wi-Fi destekli, çift çekirdekli mikrodenetleyici)
🎤 Mikrofon Modülü	INMP441 I2S Mikrofon (dijital, düşük gürültülü, I2S protokolü ile çalışır)
📺 OLED Ekran	SSD1306 128x64 I2C OLED (göz animasyonları için kullanılıyor)
🔊 Ses Çıkışı	MAX98357A I2S DAC (dijitalden analog ses sinyali çevirici) veya doğrudan I2S hoparlör
🔌 Hoparlör	4Ω 3W mini hoparlör
⚡ Güç Kaynağı	USB 5V veya harici LiPo batarya (ESP32 için)


 ⚙️ Kullanım

 1. ESP32 Kodu Yükleme


- Gerekli kütüphaneler: `Adafruit_SSD1306`, `WiFi`, `HTTPClient`, `I2S`
- WiFi SSID ve şifreyi kendi ağınıza göre güncelleyin.
- Sunucu IP adresini ESP kodunda tanımlayın.
