#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

const char* ssid;
const char* password;
const char* mqtt_server;
const char* mqtt_username;
const char* mqtt_password;
const char* description;
const char* uuid;
const char* server_name;
const char* server_path;
uint16_t server_port;
uint16_t mqtt_port;
uint32_t interval;

String client_id;
String str_ssid;
String str_password;
String str_mqtt_server;
String str_mqtt_username;
String str_mqtt_password;
String str_description;
String str_uuid;
String str_server_name;
String str_server_path;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_FLASH_PIN 4

void loadConfig() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }
  JsonDocument doc;
  
  DeserializationError error = deserializeJson(doc, configFile);

  if (error) {
    Serial.print("Failed to read file, error: ");
    Serial.println(error.f_str());
    return;
  }

  configFile.close();

  uuid = doc["uuid"] | "";
  description = doc["description"] | "";
  ssid = doc["wifi_ssid"] | "";
  password = doc["wifi_password"] | "";
  mqtt_server = doc["mqtt_server"] | "";
  mqtt_password = doc["mqtt_password"] | "";
  mqtt_username = doc["mqtt_username"] | "";
  mqtt_port = doc["mqtt_port"].as<int>()| 1883;
  interval = doc["interval"].as<int>() | 10000;
  server_name = doc["server_name"] | "";
  server_path = doc["server_path"] | ";";
  server_port = doc["server_port"] | 8000;

  str_ssid = String(ssid);
  str_password = String(password);
  str_mqtt_server = String(mqtt_server);
  str_mqtt_username = String(mqtt_username);
  str_mqtt_password = String(mqtt_password);
  str_description = String(description);
  str_uuid = String(uuid);
  str_server_name = String(server_name);
  str_server_path = String(server_path);
  Serial.printf("\n=================================\n\r");
  Serial.printf("ssid: %s\n", str_ssid);
  Serial.printf("password: %s\n", str_password);
}

void connectToWiFi() {
  Serial.println("Wi-Fi'ye bağlanıyor...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(str_ssid.c_str(), str_password.c_str());
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED & tries < 20) {
    Serial.printf("SSID: %s\n", str_ssid.c_str());
    Serial.printf("password: %s\n", str_password.c_str());
    delay(1000);
    Serial.println("Wi-Fi'ye bağlanırken sorun oluştu...");
    Serial.println(WiFi.status());
    tries++;
  }
  Serial.println("Wi-Fi bağlantısı başarılı!");
  client_id = WiFi.macAddress();
}

void connectToMQTT() {
  while (!mqtt_client.connected()) {
    Serial.println("MQTT broker'a bağlanıyor...");
    Serial.printf("Mqtt Username: %s\n", str_mqtt_username.c_str());
    Serial.printf("Mqtt Password: %s\n", str_mqtt_password.c_str());
    if (mqtt_client.connect("ESP32Client", str_mqtt_username.c_str(), str_mqtt_password.c_str()))
    {
      Serial.println("MQTT Bağlantısı Başarılı!");
      if (mqtt_client.subscribe("esp32/#")) {
        Serial.println("MQTT Subscribe başarılı!");
      } else {
        Serial.println("MQTT Subscribe başarısız!");
      }
    } else {
      Serial.print("Bağlantı hatası, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" tekrar denenecek...");
      delay(5000);
    }
  }
}
void requestUuid()
{
  if( str_uuid.c_str()[0] == '\0')
  {
    JsonDocument uuid_doc;
    uuid_doc["type"] = "request";
    uuid_doc["client_id"] = WiFi.macAddress();
    uuid_doc["description"] = str_description.c_str();

    char buffer[128];
    serializeJson(uuid_doc, buffer);

    mqtt_client.publish("esp32/uuid_exchange", buffer);
    Serial.printf("\nUUID isteği gönderildi");
  }
}
void updateUUID(const char* new_uuid) {

  File file = LittleFS.open("/config.json", "r");
  if (!file) {
      Serial.println("Config dosyası açılamadı!");
      return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close(); 

  if (error) {
      Serial.println("Config JSON ayrıştırma hatası!");
      return;
  }

  doc["uuid"] = new_uuid;

  file = LittleFS.open("/config.json", "w");
  if (!file) {
      Serial.println("Config dosyası yazma için açılamadı!");
      return;
  }

  serializeJson(doc, file);
  file.close();

  Serial.println("UUID başarıyla güncellendi!");
  Serial.println("10 saniye sonra sistem yeniden başlatılacak.");
  delay(10000);
  ESP.restart();
    
}
void callBack(char* topic, byte* payload, unsigned int length)
{
    Serial.println("Yeni bir mesaj geldi.");
    Serial.println(topic);


    JsonDocument uuid_doc;
    deserializeJson(uuid_doc, payload, length);

    Serial.print("[MQTT] Gelen Mesaj: ");
    serializeJson(uuid_doc, Serial);  
    Serial.println();

    if (uuid_doc["type"].is<String>() && uuid_doc["type"] == "client_response" && uuid_doc["client_id"].is<String>() && uuid_doc["uuid"].is<String>())
    {
      Serial.println("Gelen mesaj bir uuid client_response.");
      String incoming_client_id = uuid_doc["client_id"].as<String>(); 

      if (incoming_client_id.equals(client_id)) 
      {
          uuid = uuid_doc["uuid"];
          Serial.println("Yeni bir uuid alındı, kaydedilmeye çalışılıyor.");
          updateUUID(uuid);
          Serial.println("Yeni UUID alındı ve kaydedildi: " + uuid_doc["uuid"].as<String>());
      }
      else{
        Serial.printf("Gelen client_id, cihazdaki client_id'ye eş değil.");
        Serial.printf("\nGelen client_id: %s", incoming_client_id);
        Serial.printf("\nCihazdaki client_id: %s", client_id);
      }
    }
}
String sendPhoto() {
  String getAll;
  String getBody;

  digitalWrite(LED_FLASH_PIN, HIGH);
  delay(250);

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }
  digitalWrite(LED_FLASH_PIN, LOW);
  Serial.printf("Connecting to server: %s\n", str_server_name.c_str());

  if (wifi_client.connect(str_server_name.c_str(), server_port)) {
    Serial.println("Connection successful!"); 
    String boundary = "ESP32CAM";
    String getAll = "", getBody = "";

    String data = "--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"meter_uuid\"\r\n\r\n"
                  + String(str_uuid.c_str()) +"\r\n";

    String imagePart = "--" + boundary + "\r\n"
                       "Content-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\n"
                       "Content-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = data.length() + imagePart.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    wifi_client.printf("POST %s HTTP/1.1\n", str_server_path.c_str());
    wifi_client.printf("Host: %s\n", str_server_name.c_str());
    wifi_client.println("Content-Length: " + String(totalLen));
    wifi_client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    wifi_client.println();

    wifi_client.print(data);
    wifi_client.print(imagePart);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    size_t chunkSize = 1024;

    for (size_t n = 0; n < fbLen; n += chunkSize) {
        size_t len = (n + chunkSize < fbLen) ? chunkSize : (fbLen % chunkSize);
        wifi_client.write(fbBuf, len);
        fbBuf += len;
    }

    wifi_client.print(tail);
    esp_camera_fb_return(fb);

    long startTimer = millis();
    int timeoutTimer = 10000;
    bool state = false;

    while ((millis() - startTimer) < timeoutTimer) {
        while (wifi_client.available()) {
            char c = wifi_client.read();
            if (c == '\n') {
                if (getAll.length() == 0) { state = true; }
                getAll = "";
            } else if (c != '\r') {
                getAll += String(c);
            }
            if (state) { getBody += String(c); }
            startTimer = millis();  // Yanıt geliyorsa zamanlayıcıyı sıfırla
        }
        if (getBody.length() > 0) { break; }
    }

    Serial.println();
    wifi_client.stop();
    Serial.println("Server response:");
    Serial.println(getBody);
  } 
  else {
    char new_buff[100];
    snprintf(new_buff, sizeof(new_buff), "Connection to %s failed.", str_server_name.c_str());
    Serial.println(getBody);
  }
  return getBody;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  
  Serial.begin(115200);
  loadConfig();

  if (str_ssid == "" || str_password == "" || str_mqtt_server == "" || mqtt_port == 0) {
    Serial.println("Config dosyası hatalı ya da eksik.");
    return;
  }

  connectToWiFi();
  Serial.printf("\nMqtt Server: %s", str_mqtt_server.c_str());
  Serial.printf("\nMqtt Port: %d", mqtt_port);
  mqtt_client.setServer(str_mqtt_server.c_str(), mqtt_port);
  mqtt_client.setCallback(callBack);
  mqtt_client.setKeepAlive(60);

  connectToMQTT();

  delay(interval);

  Serial.printf("\nUUID: %s\n", str_uuid.c_str());
  requestUuid();
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 5;  
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 7;
    config.fb_count = 1;
  }

  pinMode(LED_FLASH_PIN, OUTPUT);
  digitalWrite(LED_FLASH_PIN, LOW);
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
  Serial.printf("Setup sonrası Free heap: %d\n", ESP.getFreeHeap());
}

void loop() {
  delay(interval);
  if (str_uuid.c_str()[0] != '\0')
  {
    sendPhoto();
  }
  mqtt_client.loop();
}



