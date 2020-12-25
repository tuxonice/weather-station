#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

/* Put your SSID */
const char* ssid = "ESP32-PIXEL";  // Enter SSID here

/* Put IP Address details */
IPAddress local_ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

struct Configuration {
   String wifi_ssid = "";
   String wifi_passw = "";
   String mqtt_server = "";
   String mqtt_user = "";
   String mqtt_passw = "";
   String mqtt_topic_wind = "";
   String mqtt_topic_laser = "";
   float thr_temp = 0;
   float thr_hum = 0;
   float thr_press = 0;
};

struct Configuration systemConfiguration;

// Set web server port number to 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){}
  }
  // 1. Read config file
  Serial.println("Reading configuration file");
  readConfigFile();
  // 2. Check if config has valid SSID and PSW
  if(systemConfiguration.wifi_ssid != "" && systemConfiguration.wifi_passw != "" ) {
      const char* ssid = systemConfiguration.wifi_ssid.c_str();
      const char* password = systemConfiguration.wifi_passw.c_str();
      if(!wifiConnect(ssid, password)) {
        apConnect();
      }
  } else {
      apConnect();
  }
  server.on("/", HTTP_GET, handle_OnConnect);
  server.on("/", HTTP_POST, handle_Update);
  server.onNotFound(handle_NotFound);
  
  server.begin();
  Serial.println("HTTP server started");
}

bool wifiConnect(const char * ssid, const char * password)
{
    // Connect to WI-FI
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // connect to your local wi-fi network
    WiFi.begin(ssid, password);
    // check wi-fi is connected to wi-fi network
    for(byte i = 1; i <= 20; i++) {
      if(WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected..!");
        Serial.print("Got IP: ");
        Serial.println(WiFi.localIP());
        return true;
      }
      delay(1000);
      Serial.print(".");  
    }
    WiFi.disconnect(); //Not sure if we can do this here
    return false;
}

void apConnect()
{
    // Start the AP mode
    Serial.println("Setting AP (Access Point)…");
    WiFi.softAP(ssid);
    delay(2000); // VERY IMPORTANT
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.persistent(false);
    delay(1000);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
}

void loop(){
  server.handleClient();
  delay(1000);
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void handle_Update(){
    
  if (server.hasArg("wifi-ssid")) {
      systemConfiguration.wifi_ssid = server.arg("wifi-ssid");
  }
  
  if (server.hasArg("wifi-password")) {
      systemConfiguration.wifi_passw = server.arg("wifi-password");
  }
  
  if (server.hasArg("mqtt-server")) {
      systemConfiguration.mqtt_server = server.arg("mqtt-server");
  }
  
  if (server.hasArg("mqtt-user")) {
      systemConfiguration.mqtt_user = server.arg("mqtt-user");
  }
  
  if (server.hasArg("mqtt-password")) {
      systemConfiguration.mqtt_passw = server.arg("mqtt-password");
  }
  
  if (server.hasArg("mqtt-topic-wind")) {
      systemConfiguration.mqtt_topic_wind = server.arg("mqtt-topic-wind");
  }
  
  if (server.hasArg("mqtt-topic-laser")) {
      systemConfiguration.mqtt_topic_laser = server.arg("mqtt-topic-laser");
  }
  
  if (server.hasArg("threshold-temperature")) {
      systemConfiguration.thr_temp = server.arg("threshold-temperature").toFloat();
  }
  
  if (server.hasArg("threshold-humidity")) {
      systemConfiguration.thr_hum = server.arg("threshold-humidity").toFloat();
  }
  
  if (server.hasArg("threshold-pressure")) {
      systemConfiguration.thr_press = server.arg("threshold-pressure").toFloat();
  }

  writeConfigFile("/config.json");
  server.send(200, "text/html", SendHTML());
}

void readConfigFile() {
    
  // Read JSON data from a file
  File file = SPIFFS.open("/config.json");
  if(file) {
    // Deserialize the JSON data
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    systemConfiguration.wifi_ssid = doc["wifi_ssid"].as<String>();
    systemConfiguration.wifi_passw = doc["wifi_passw"].as<String>();
    systemConfiguration.mqtt_server = doc["mqtt_server"].as<String>();
    systemConfiguration.mqtt_user = doc["mqtt_user"].as<String>();
    systemConfiguration.mqtt_passw = doc["mqtt_passw"].as<String>();
    systemConfiguration.mqtt_topic_wind = doc["mqtt_topic_wind"].as<String>();
    systemConfiguration.mqtt_topic_laser = doc["mqtt_topic_laser"].as<String>();
    systemConfiguration.thr_temp = doc["thr_temp"];
    systemConfiguration.thr_hum = doc["thr_hum"];
    systemConfiguration.thr_press = doc["thr_press"]; 
    
  file.close();  
  } else {
    Serial.println("Failed to open SPIFFS file to read");
  }
}

void writeConfigFile(const char* filename) {
  File outfile = SPIFFS.open(filename,"w");
  if(outfile) {
    StaticJsonDocument<512> doc;
    doc["wifi_ssid"] = systemConfiguration.wifi_ssid;
    doc["wifi_passw"] = systemConfiguration.wifi_passw;
    doc["mqtt_server"] = systemConfiguration.mqtt_server;
    doc["mqtt_user"] = systemConfiguration.mqtt_user;
    doc["mqtt_passw"] = systemConfiguration.mqtt_passw;
    doc["mqtt_topic_wind"] = systemConfiguration.mqtt_topic_wind;
    doc["mqtt_topic_laser"] = systemConfiguration.mqtt_topic_laser;
    doc["thr_temp"] = systemConfiguration.thr_temp;
    doc["thr_hum"] = systemConfiguration.thr_hum;
    doc["thr_press"] = systemConfiguration.thr_press;
    if(serializeJson(doc, outfile)==0) {
      Serial.println("Failed to write to SPIFFS file");
    } else {
      Serial.println("Success!");
    }
    outfile.close();
    return;
  }
  Serial.println("Failed to open SPIFFS file to write");
}

String SendHTML(){
  String ptr = "<!doctype html> <html lang=\"en\">\n";
  ptr +="<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\n";
  ptr +="<title>Weather Station | Pixel</title>\n";
  ptr +="<style>\n";
  ptr +="body{margin:0;font-family:Roboto,\"Helvetica Neue\",Arial,\"Noto Sans\",sans-serif;font-size:1rem;font-weight:400;line-height:1.5;color:#212529;text-align:left;background-color:#f8f9fa}input[type=password],input[type=text],select{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}input[type=submit]{width:100%;background-color:#4caf50;color:#fff;padding:14px 20px;margin:8px 0;border:none;border-radius:4px;cursor:pointer;font-size:1.2rem}.section-header{text-align:center;padding-top:1.5rem;padding-bottom:1.5rem}.container{max-width:960px;width:100%;padding-right:15px;padding-left:15px;margin-right:auto;margin-left:auto}h2{font-size:1.5rem;margin-top:0;margin-bottom:1rem;font-weight:500;line-height:1.2}hr{margin-top:1rem;margin-bottom:1rem;border:0;border-top:1px solid rgba(0,0,0,.1)}input[type=submit]:hover{background-color:#45a049}footer{color:#6c757d;text-align:center;padding-top:1rem;margin-bottom:1rem;margin-top:1.5rem}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body><div class=\"container\"><div class=\"section-header\"><h1>Pixel Weather Station 3</h1></div><form action=\"/\" method=\"POST\"><h2>Wifi</h2> <label for=\"wifi-ssid\">SSID</label> <input type=\"text\" id=\"wifi-ssid\" name=\"wifi-ssid\" value=\"" + systemConfiguration.wifi_ssid + "\" required><label for=\"wifi-password\">Password</label> <input type=\"password\" id=\"wifi-password\" name=\"wifi-password\" value=\"" + systemConfiguration.wifi_passw + "\" required><hr/><h2>MQTT</h2> <label for=\"mqtt-server\">Server</label> <input type=\"text\" id=\"mqtt-server\" name=\"mqtt-server\" value=\"" + systemConfiguration.mqtt_server + "\" required><label for=\"mqtt-user\">Username</label> <input type=\"text\" id=\"mqtt-user\" name=\"mqtt-user\" value=\"" + systemConfiguration.mqtt_user + "\" required><label for=\"mqtt-password\">Password</label> <input type=\"password\" id=\"mqtt-password\" name=\"mqtt-password\" value=\"" + systemConfiguration.mqtt_passw + "\" required><hr/><h2>MQTT Topics</h2> <label for=\"mqtt-topic-wind\">Wind</label> <input type=\"text\" id=\"mqtt-topic-wind\" name=\"mqtt-topic-wind\" value=\"" + systemConfiguration.mqtt_topic_wind + "\" required> <label for=\"mqtt-topic-laser\">laser</label> <input type=\"text\" id=\"mqtt-topic-laser\" name=\"mqtt-topic-laser\" value=\"" + systemConfiguration.mqtt_topic_laser + "\" required><hr/><h2>Threshold</h2> <label for=\"threshold-temperature\">Temperature</label> <select id=\"threshold-temperature\" name=\"threshold-temperature\"><option value=\"1\">1</option><option value=\"1.5\">1.5</option><option value=\"2\">2</option> </select> <label for=\"threshold-humidity\">Humidity</label> <select id=\"threshold-humidity\" name=\"threshold-humidity\"><option value=\"1\">1</option><option value=\"3\">3</option><option value=\"5\">5</option> </select> <label for=\"threshold-pressure\">Pressure</label> <select id=\"threshold-pressure\" name=\"threshold-pressure\"><option value=\"10\">10</option><option value=\"20\">20</option><option value=\"30\">30</option> </select><input type=\"submit\" value=\"Save\"></form> <footer><p>&copy; 2020 TLab</p> </footer></div></body>\n";
  ptr +="</html>\n";
  return ptr;
}
