#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// -------------------- OLED Settings --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- Eye Animation --------------------
int leftEyeX = 45, rightEyeX = 80, eyeY = 18;
int eyeWidth = 25, eyeHeight = 30;
int targetOffsetX = 0, targetOffsetY = 0;
int moveSpeed = 5, blinkState = 0;
unsigned long lastBlinkTime = 0, moveTime = 0;

// -------------------- Hardware Pins --------------------
#define BUTTON_PIN 15
#define RECORD_LED 13

// -------------------- I2S Settings --------------------
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_DOUT 19
#define I2S_BCLK 18
#define I2S_LRC 23

// -------------------- Audio Settings --------------------
#define SAMPLE_RATE 16000
#define CHUNK_SIZE 2048  // Reduced for better stability

// -------------------- Server Settings --------------------
const char* ssid = "WIFI ADI";
const char* password = "SIFRE";
#define SERVER_UPLOAD_URL "http://IP:5000/chat"
#define SERVER_UPLOAD_END "http://IP:5000/chat/end"
#define SERVER_AUDIO_URL "http://IP:5000/response"
#define SERVER_TIMEOUT 10000  // 10 seconds


// -------------------- System Variables --------------------
bool isRecording = false;
bool isPlaying = false;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;  // Wait for serial to initialize

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED initialization failed!");
    while (true)
      ;
  }
  display.display();
  delay(1000);

  // WiFi Connection
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Connecting to WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  unsigned long wifiStartTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStartTime > 10000) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("WiFi Failed!");
      display.display();
      while (true)
        ;
    }
    delay(300);
    display.print(".");
    display.display();
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connected!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(1000);

  // Pin settings
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RECORD_LED, OUTPUT);
  digitalWrite(RECORD_LED, LOW);
}

void loop() {
  static unsigned long lastWifiCheck = 0;

  // Check WiFi status periodically
  if (millis() - lastWifiCheck > 5000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
      delay(1000);
    }
    lastWifiCheck = millis();
  }

  updateEyes();

  if (!isRecording && !isPlaying && digitalRead(BUTTON_PIN) == LOW) {
    delay(50);  // debounce
    if (digitalRead(BUTTON_PIN)) return;

    isRecording = true;
    displayRecordingAnimation();
    setupI2SForMic();
    recordAudioChunked();
    i2s_driver_uninstall(I2S_NUM_0);
    isRecording = false;

    displayUploadingAnimation();

    isPlaying = true;
    displayProcessingAnimation();
    setupI2SForSpeaker();
    if (!playWavFromHTTP()) {
      displayError("OYNATMA HATASI!");
    }
    i2s_driver_uninstall(I2S_NUM_1);
    isPlaying = false;
  }
}

// -------------------- Eye Animation Functions --------------------
void updateEyes() {
  unsigned long currentTime = millis();

  // Blinking
  if (currentTime - lastBlinkTime > 4000 && blinkState == 0) {
    blinkState = 1;
    lastBlinkTime = currentTime;
  } else if (currentTime - lastBlinkTime > 150 && blinkState == 1) {
    blinkState = 0;
    lastBlinkTime = currentTime;
  }

  // Random eye movement
  if (currentTime - moveTime > random(1500, 3000) && blinkState == 0) {
    targetOffsetX = random(-10, 11);
    targetOffsetY = random(-8, 9);
    moveTime = currentTime;
  }

  static int offsetX = 0, offsetY = 0;
  offsetX += (targetOffsetX - offsetX) / moveSpeed;
  offsetY += (targetOffsetY - offsetY) / moveSpeed;

  display.clearDisplay();

  if (blinkState == 0) {
    drawEye(leftEyeX + offsetX, eyeY + offsetY);
    drawEye(rightEyeX + offsetX, eyeY + offsetY);
  } else {
    display.fillRect(leftEyeX + offsetX, eyeY + eyeHeight / 2 - 2, eyeWidth, 4, WHITE);
    display.fillRect(rightEyeX + offsetX, eyeY + eyeHeight / 2 - 2, eyeWidth, 4, WHITE);
  }

  display.display();
  delay(30);
}

void drawEye(int x, int y) {
  display.fillRoundRect(x, y, eyeWidth, eyeHeight, 5, WHITE);
}

// -------------------- Display Animation Functions --------------------
void displayRecordingAnimation() {
  display.clearDisplay();
  drawEye(leftEyeX, eyeY);
  display.fillCircle(leftEyeX + eyeWidth / 2, eyeY + eyeHeight / 2, 5, BLACK);
  drawEye(rightEyeX, eyeY);
  display.fillCircle(rightEyeX + eyeWidth / 2, eyeY + eyeHeight / 2, 5, BLACK);
  display.display();
}

void displayUploadingAnimation() {
  display.clearDisplay();
  for (int i = 0; i < 3; i++) {
    drawEye(leftEyeX, eyeY);
    display.fillCircle(leftEyeX + 5 + (i * 8), eyeY + eyeHeight / 2, 2, BLACK);
    drawEye(rightEyeX, eyeY);
    display.fillCircle(rightEyeX + 5 + (i * 8), eyeY + eyeHeight / 2, 2, BLACK);
  }
  display.display();
}

void displayProcessingAnimation() {
  static int dots = 0;
  display.clearDisplay();
  drawEye(leftEyeX, eyeY);
  drawEye(rightEyeX, eyeY);
  for (int i = 0; i <= dots; i++) {
    display.fillCircle(leftEyeX + 8 + (i * 8), eyeY + eyeHeight + 5, 2, WHITE);
    display.fillCircle(rightEyeX + 8 + (i * 8), eyeY + eyeHeight + 5, 2, WHITE);
  }
  dots = (dots + 1) % 4;
  display.display();
  delay(200);
}

void displayError(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(msg);
  display.display();
  delay(2000);
}

// -------------------- I2S Configuration Functions --------------------
void setupI2SForMic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = -1
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupI2SForSpeaker() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,
    .dma_buf_len = 1024,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_1, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
  i2s_set_sample_rates(I2S_NUM_1, SAMPLE_RATE);
  i2s_zero_dma_buffer(I2S_NUM_1);
}

// -------------------- Audio Recording and Upload Functions --------------------
bool sendChunkToServer(uint8_t* chunk, size_t length) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected during chunk send!");
    return false;
  }

  HTTPClient http;
  http.begin(SERVER_UPLOAD_URL);
  http.addHeader("Content-Type", "application/octet-stream");
  http.setTimeout(SERVER_TIMEOUT);

  Serial.printf("Sending %d bytes to server...\n", length);
  int code = http.POST(chunk, length);

  if (code > 0) {
    Serial.printf("HTTP Response: %d\n", code);
    Serial.printf("Response: %s\n", http.getString().c_str());
  } else {
    Serial.printf("Error: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  return (code == HTTP_CODE_OK);
}

void recordAudioChunked() {
  size_t bytes_read = 0;
  int16_t buffer[CHUNK_SIZE / 2];  // 16 bit = 2 byte

  digitalWrite(RECORD_LED, HIGH);
  unsigned long start = millis();

  while ((digitalRead(BUTTON_PIN) == LOW || millis() - start < 2000)) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost during recording!");
      break;
    }

    esp_err_t res = i2s_read(I2S_NUM_0, buffer, CHUNK_SIZE, &bytes_read, portMAX_DELAY);
    if (res == ESP_OK && bytes_read > 0) {
      if (!sendChunkToServer((uint8_t*)buffer, bytes_read)) {
        Serial.println("Failed to send chunk!");
        break;
      }
    } else {
      Serial.printf("I2S read error: %d\n", res);
    }
  }

  digitalWrite(RECORD_LED, LOW);

  // Send recording end notification
  HTTPClient http;
  http.begin(SERVER_UPLOAD_END);
  http.setTimeout(SERVER_TIMEOUT);
  int code = http.POST("");
  Serial.printf("End notification response: %d\n", code);
  http.end();
}

// -------------------- Audio Playback Functions --------------------
bool playWavFromHTTP() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Kayıttan yürütme için WiFi bağlı değil!");
        return false;
    }

    HTTPClient http;
    http.begin(SERVER_AUDIO_URL);
    http.setTimeout(SERVER_TIMEOUT);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET başarısız oldu: %d\n", httpCode);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t header[44];
    if (stream->readBytes(header, 44) != 44) {
        Serial.println("WAV başlığı okunamadı");
        http.end();
        return false;
    }

    uint8_t buffer[4096];
    size_t bytesRead = 0;

    while (http.connected() && !isRecording) {
        bytesRead = stream->readBytes(buffer, sizeof(buffer));
        if (bytesRead == 0) break;

        // Ses seviyesini %200 artır (16-bit örnekler)
        for (int i = 0; i < bytesRead; i += 2) {
            int16_t sample = (int16_t)(buffer[i] | (buffer[i + 1] << 8));
            sample *= 1.4;
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            buffer[i] = sample & 0xFF;
            buffer[i + 1] = (sample >> 8) & 0xFF;
        }

        size_t totalWritten = 0;
        while (totalWritten < bytesRead) {
            size_t written = 0;
            esp_err_t ret = i2s_write(I2S_NUM_1, buffer + totalWritten, bytesRead - totalWritten, &written, portMAX_DELAY);
            if (ret != ESP_OK) {
                Serial.println("I2S yazma hatası!");
                http.end();
                return false;
            }
            totalWritten += written;
        }
    }

    http.end();
    return true;
}