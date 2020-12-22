#include <SPIFFS.h>
#include <ArduinoJson.h>

void setup() {
  Serial.begin(115200);
  // Initialize the SPIFFS object
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){}
  }

  const char* filename = "/config.json";
  writeDataToFile(filename);
  listAllFiles();
  // removeFile(filename);
  readDataFromFile(filename);
}

void loop() {}

void listAllFiles() {
  Serial.println("List all files");
  // List all available files (if any) in the SPI Flash File System
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
  root.close();
  file.close();  
}

void readDataFromFile(const char* filename) {
  // Read JSON data from a file
  File file = SPIFFS.open(filename);
  if(file) {
    // Deserialize the JSON data
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    String wifi_ssid = doc["wifi_ssid"];
    String wifi_passw = doc["wifi_passw"];
    String mqtt_server = doc["mqtt_server"];
    String mqtt_user = doc["mqtt_user"];
    String mqtt_passw = doc["mqtt_passw"];
    String mqtt_topic_wind = doc["mqtt_topic_wind"];
    String mqtt_topic_laser = doc["mqtt_topic_laser"];
    float thr_temp = doc["thr_temp"];
    float thr_hum = doc["thr_hum"];
    float thr_press = doc["thr_press"];
    Serial.println("Wifi SSID: " + wifi_ssid);
    Serial.println("Wifi Password: " + wifi_passw);
    Serial.println("MQTT Server: " + mqtt_server);
    Serial.println("MQTT User: " + mqtt_user);
    Serial.println("MQTT Password: " + mqtt_passw);
    Serial.println("MQTT Topic Wind: " + mqtt_topic_wind);
    Serial.println("MQTT Topic Laser: " + mqtt_topic_laser);
    Serial.print("Threshold Temperature: ");
    Serial.println(thr_temp);
    Serial.print("Threshold Humidity: ");
    Serial.println(thr_hum);
    Serial.print("Threshold Pression: ");
    Serial.println(thr_press);
  }
  file.close();  
}

void writeDataToFile(const char* filename) {
  File outfile = SPIFFS.open(filename,"w");
  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = "my-long-ssid";
  doc["wifi_passw"] = "my-long-password";
  doc["mqtt_server"] = "mqtt_server-address";
  doc["mqtt_user"] = "mqtt-username";
  doc["mqtt_passw"] = "mqtt-password-very-long";
  doc["mqtt_topic_wind"] = "pixel/wind/more/stuff";
  doc["mqtt_topic_laser"] = "pixel/laser/more/stuff";
  doc["thr_temp"] = 1.5;
  doc["thr_hum"] = 5;
  doc["thr_press"] = 10.1;
  if(serializeJson(doc, outfile)==0) {
    Serial.println("Failed to write to SPIFFS file");
  } else {
    Serial.println("Success!");
  }
  outfile.close();  
}

void removeFile(const char* filename) {
  SPIFFS.remove(filename);
}
