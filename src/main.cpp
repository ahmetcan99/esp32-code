#include <WiFi.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* ssid;
const char* password;
const char* mqtt_server;
const char* mqtt_username;
const char* mqtt_password;
const char* description;
const char* uuid;
int mqtt_port;
int interval;

String client_id;
String str_ssid;
String str_password;
String str_mqtt_server;
String str_mqtt_username;
String str_mqtt_password;
String str_description;
String str_uuid;

WiFiClient espClient;
PubSubClient client(espClient);

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

  str_ssid = String(ssid);
  str_password = String(password);
  str_mqtt_server = String(mqtt_server);
  str_mqtt_username = String(mqtt_username);
  str_mqtt_password = String(mqtt_password);
  str_description = String(description);
  str_uuid = String(uuid);
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
  while (!client.connected()) {
    Serial.println("MQTT broker'a bağlanıyor...");
    Serial.printf("Mqtt Username: %s\n", str_mqtt_username.c_str());
    Serial.printf("Mqtt Password: %s\n", str_mqtt_password.c_str());
    if (client.connect("ESP32Client", str_mqtt_username.c_str(), str_mqtt_password.c_str()))
    {
      Serial.println("MQTT Bağlantısı Başarılı!");
      if (client.subscribe("esp32/#")) {
        Serial.println("MQTT Subscribe başarılı!");
      } else {
        Serial.println("MQTT Subscribe başarısız!");
      }
    } else {
      Serial.print("Bağlantı hatası, rc=");
      Serial.print(client.state());
      Serial.println(" tekrar denenecek...");
      delay(5000);
    }
  }
}
void requestUuid()
{
  JsonDocument uuid_doc;
  uuid_doc["type"] = "request";
  uuid_doc["client_id"] = WiFi.macAddress();
  uuid_doc["description"] = str_description.c_str();

  char buffer[128];
  serializeJson(uuid_doc, buffer);

  client.publish("esp32/uuid_exchange", buffer);
  Serial.printf("\nUUID isteği gönderildi");
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


void setup() {
  Serial.begin(115200);
  loadConfig();

  if (str_ssid == "" || str_password == "" || str_mqtt_server == "" || mqtt_port == 0) {
    Serial.println("Config dosyası hatalı ya da eksik.");
    return;
  }

  connectToWiFi();
  Serial.printf("\nMqtt Server: %s", str_mqtt_server.c_str());
  Serial.printf("\nMqtt Port: %d", mqtt_port);
  client.setServer(str_mqtt_server.c_str(), mqtt_port);
  client.setCallback(callBack);
  client.setKeepAlive(60);
  connectToMQTT();
  delay(interval);
  Serial.printf("\nUUID: %s\n", str_uuid.c_str());
  if( str_uuid.c_str()[0] == '\0')
  {
    requestUuid();
  }

}

void loop() {
  delay(interval);
  client.loop();
}



