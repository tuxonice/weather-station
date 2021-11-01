#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "SparkFunBME280.h"

void statusToBlink(int status, int times);
String SendHTML(String alertMessage, String currentValues);

#define INPUT_PIN_WIND 15
#define INPUT_PIN_LASER 18
#define STATUS_LED  5
#define ERROR_LED 13
#define WIFI_CONNECTING_BLINK_COUNT 2
#define MQTT_ERROR_BLINK_COUNT 3
#define BME280_ERROR_BLINK_COUNT 6
#define SPIFFS_ERROR_BLINK_COUNT 8

volatile int count = 0;
volatile bool laser = false;
volatile unsigned long lastEntryWind;
volatile unsigned long lastEntryLaser;
unsigned long lastSlice = 0;
unsigned long lastEntryBME280;
bool weatherSensorIsActive = false;
bool apMode;

struct Configuration {
   String wifi_ssid = "";
   String wifi_password = "";
   String mqtt_server = "";
   String mqtt_port = "1883";
   String mqtt_user = "";
   String mqtt_password = "";
   float thr_temp = 1;
   float thr_hum = 5;
   float thr_press = 1;
   int slice_time = 30;
   int time_update = 60;
};

struct Readings {
   float temperature = 0.0;
   float humidity = 0.0;
   float pressure = 0.0;
   float wind = 0.0;
};


struct Configuration systemConfiguration;
struct Readings currentReadings;
BME280 weatherSensor;

portMUX_TYPE synch_wind = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE synch_laser = portMUX_INITIALIZER_UNLOCKED;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WebServer webServer(80);

// reed sensor interrupt routine (500 ms debouce)
void IRAM_ATTR wind_isr() 
{
  portENTER_CRITICAL(&synch_wind);
  if (millis() > lastEntryWind + 500) {
    count++;
    lastEntryWind = millis();
  }
  portEXIT_CRITICAL(&synch_wind);
}

// laser sensor interrupt routine (500 ms debouce)
void IRAM_ATTR laser_isr() 
{
  portENTER_CRITICAL(&synch_laser);
  if (millis() > lastEntryLaser + 500) {
    laser = true;
    lastEntryLaser = millis();
  }
  portEXIT_CRITICAL(&synch_laser);
}

void setup() 
{
  Serial.begin(115200);
  apMode = false;
  pinMode(STATUS_LED, OUTPUT);
  pinMode(ERROR_LED, OUTPUT);
  digitalWrite(ERROR_LED, HIGH);

  // 1. I2C Initialization
  Wire.begin();
  if (weatherSensor.beginI2C() == false)
  {
    Serial.println("The sensor did not respond. Please check wiring.");
    weatherSensorIsActive = false;
  } else {
    weatherSensorIsActive = true;
  }

  // 2. Initialize SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){
        statusToBlink(SPIFFS_ERROR_BLINK_COUNT, 1);
    }
  }

  // 3. Read config file
  Serial.println("Reading configuration file");
  readConfigFile();

  // 4. Connect to wifi STA or AP mode
  if(systemConfiguration.wifi_ssid == "" || systemConfiguration.wifi_password == "" || !wifiConnect(15000)) {
      apConnect();
      apMode = true;
  }

  webServer.on("/", HTTP_GET, handle_OnConnect);
  webServer.on("/", HTTP_POST, handle_Update);
  webServer.onNotFound(handle_NotFound);
  
  webServer.begin();
  Serial.println("HTTP server started");

  // -- INTERRUPT WIND --
  pinMode(INPUT_PIN_WIND, INPUT_PULLUP);
  attachInterrupt(INPUT_PIN_WIND, wind_isr, RISING);
  lastEntryWind = millis();

  // -- INTERRUPT LASER --
  pinMode(INPUT_PIN_LASER, INPUT_PULLUP);
  attachInterrupt(INPUT_PIN_LASER, laser_isr, RISING);
  lastEntryLaser = millis();

  // 5. Initialize MQTT
  mqttClient.setServer(systemConfiguration.mqtt_server.c_str(), systemConfiguration.mqtt_port.toInt());
  mqttReconnect();
  
  lastEntryBME280 = millis();
  digitalWrite(ERROR_LED, LOW);
}

bool mqttReconnect() 
{
  if (!mqttClient.connected()) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (!mqttClient.connect(clientId.c_str(),systemConfiguration.mqtt_user.c_str(),systemConfiguration.mqtt_password.c_str())) {
      Serial.print("Fail MQTT connection...");
      Serial.println("failed, rc=" + mqttClient.state());
      return false;
    }
  }
  return true;
}

bool mqttPublish(char * topic, char * serialData){
  if (!mqttReconnect()) {
      digitalWrite(ERROR_LED, HIGH);
      return false;
  }
  mqttClient.publish(topic, serialData);
  digitalWrite(ERROR_LED, LOW);
  return true;
}

bool wifiConnect(int timeout)
{
    unsigned long startTime = millis();
    // Connect to WI-FI
    Serial.print("Connecting to ");
    Serial.println(systemConfiguration.wifi_ssid.c_str());
    // connect to your local wi-fi network
    WiFi.begin(systemConfiguration.wifi_ssid.c_str(), systemConfiguration.wifi_password.c_str());
    // check wi-fi is connected to wi-fi network
    while(WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      if((millis() - startTime) > timeout) {
        WiFi.disconnect(); //Not sure if we can do this here
        return false;
      }
    }
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");
    Serial.println(WiFi.localIP());
    randomSeed(micros());
    return true;
}

void apConnect()
{
    IPAddress localIP(192,168,4,1);
    IPAddress gateway(192,168,4,1);
    IPAddress subnet(255,255,255,0);
    // Start the AP mode
    Serial.println("Setting AP (Access Point)...");
    WiFi.softAP("ESP32-PIXEL");
    delay(2000); // VERY IMPORTANT
    WiFi.softAPConfig(localIP, gateway, subnet);
    WiFi.persistent(false);
    delay(1000);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
}

void loop()
{

  webServer.handleClient();
  if(apMode == true) {
      delay(500);
      return;
  }
  
  if(millis() >= lastSlice + (systemConfiguration.slice_time * 1000)) {
    readWindSpeed();
  }

  readLaserSensor();
  
  if(weatherSensorIsActive == true && millis() >= lastEntryBME280 + (systemConfiguration.time_update * 1000)) {
    readWeatherSensor();
  }
  
  iddleToBlink(1);
}

void readLaserSensor() 
{
  if(laser == true) {
    mqttPublish("pixel/weather/laser", "ON");
    portENTER_CRITICAL_ISR(&synch_laser);
    laser = false;
    portEXIT_CRITICAL_ISR(&synch_laser);
  }
}

void readWindSpeed() 
{
    char average[10];
    currentReadings.wind = (float)(count * 60000)/(float)(systemConfiguration.slice_time * 1000);
    sprintf(average, "%.02f", currentReadings.wind);
    portENTER_CRITICAL_ISR(&synch_wind);
    count = 0;  
    portEXIT_CRITICAL_ISR(&synch_wind);
    lastSlice = millis();
    mqttPublish("pixel/weather/wind", average);
}

bool readWeatherSensor()
{
  char humidity[10];
  char pressure[10];
  char temperature[10];
  float currentHumidity = 0.0;
  float currentPressure = 0.0;
  float currentTemperature = 0.0;
  
  if(!weatherSensorIsActive) {
      return false;
  }

  currentTemperature = weatherSensor.readTempC();
  currentHumidity = weatherSensor.readFloatHumidity();
  currentPressure = weatherSensor.readFloatPressure() / 100;

  sprintf(humidity, "%.02f", currentHumidity);
  sprintf(pressure, "%.02f", currentPressure);
  sprintf(temperature, "%.02f", currentTemperature);
  
/*
  Serial.print("Humidity: ");
  Serial.println(currentHumidity);
  Serial.print("Pressure: ");
  Serial.println(currentPressure);
  Serial.print("Temp: ");
  Serial.println(currentTemperature);
  Serial.print("Alt: ");
  Serial.println(weatherSensor.readFloatAltitudeMeters(), 1);
*/

  if(abs(currentReadings.humidity - currentHumidity) >= systemConfiguration.thr_hum) {
      mqttPublish("pixel/weather/humidity", humidity);
      currentReadings.humidity = currentHumidity;
  }
  
  if(abs(currentReadings.pressure - currentPressure) >= systemConfiguration.thr_press) {
      mqttPublish("pixel/weather/pressure", pressure);
      currentReadings.pressure = currentPressure;
  }

  if(abs(currentReadings.temperature - currentTemperature) >= systemConfiguration.thr_temp) {
      mqttPublish("pixel/weather/temperature", temperature);
      currentReadings.temperature = currentTemperature;
  }
  
  lastEntryBME280 = millis();
  return true;
}

void handle_OnConnect() 
{
  String currentValues = getCurrentValuesHtml();
  webServer.send(200, "text/html", SendHTML("", currentValues)); 
}

void handle_NotFound()
{
  webServer.send(404, "text/plain", "Not found");
}

void handle_Update()
{
    
  if (webServer.hasArg("wifi-ssid")) {
      systemConfiguration.wifi_ssid = webServer.arg("wifi-ssid");
  }
  
  if (webServer.hasArg("wifi-password") && webServer.arg("wifi-password") != "**********") {
      systemConfiguration.wifi_password = webServer.arg("wifi-password");
  }
  
  if (webServer.hasArg("mqtt-server")) {
      systemConfiguration.mqtt_server = webServer.arg("mqtt-server");
  }
  
  if (webServer.hasArg("mqtt-port")) {
      systemConfiguration.mqtt_port = webServer.arg("mqtt-port");
  }
  
  if (webServer.hasArg("mqtt-user")) {
      systemConfiguration.mqtt_user = webServer.arg("mqtt-user");
  }
  
  if (webServer.hasArg("mqtt-password") && webServer.arg("mqtt-password") != "**********") {
      systemConfiguration.mqtt_password = webServer.arg("mqtt-password");
  }
    
  if (webServer.hasArg("thr-temp")) {
      systemConfiguration.thr_temp = webServer.arg("thr-temp").toFloat();
  }
  
  if (webServer.hasArg("thr-hum")) {
      systemConfiguration.thr_hum = webServer.arg("thr-hum").toFloat();
  }
  
  if (webServer.hasArg("thr-press")) {
      systemConfiguration.thr_press = webServer.arg("thr-press").toFloat();
  }
  
  if (webServer.hasArg("slice-time")) {
      systemConfiguration.slice_time = webServer.arg("slice-time").toInt();
  }
  
  if (webServer.hasArg("time-update")) {
      systemConfiguration.time_update = webServer.arg("time-update").toInt();
  }

  bool configFileSaved = writeConfigFile("/config.json");
  String alertMessage = "";
  if(configFileSaved) {
    alertMessage = getAlertMessageHtml("success", "Configuration saved!");
  } else {
    alertMessage = getAlertMessageHtml("danger", "Configuration not saved!");
  }
  String currentValues = getCurrentValuesHtml();
  webServer.send(200, "text/html", SendHTML(alertMessage, currentValues));
}

bool readConfigFile() 
{
    
  File file = SPIFFS.open("/config.json");
  if(file) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return false;
    }

    JsonObject obj = doc.as<JsonObject>();
    systemConfiguration.wifi_ssid = obj["wifi_ssid"].as<String>();
    systemConfiguration.wifi_password = obj["wifi_password"].as<String>();
    systemConfiguration.mqtt_server = obj["mqtt_server"].as<String>();
    systemConfiguration.mqtt_port = obj["mqtt_port"].as<String>();
    systemConfiguration.mqtt_user = obj["mqtt_user"].as<String>();
    systemConfiguration.mqtt_password = obj["mqtt_password"].as<String>();
    systemConfiguration.thr_temp = obj["thr_temp"];
    systemConfiguration.thr_hum = obj["thr_hum"];
    systemConfiguration.thr_press = obj["thr_press"]; 
    systemConfiguration.time_update = obj["time_update"]; 
    systemConfiguration.slice_time = obj["slice_time"]; 
    file.close();
    return true;
  }
  return false;
}

bool writeConfigFile(const char* filename) 
{
  File outfile = SPIFFS.open(filename,"w");
  if(outfile) {
    StaticJsonDocument<512> doc;
    doc["wifi_ssid"] = systemConfiguration.wifi_ssid;
    doc["wifi_password"] = systemConfiguration.wifi_password;
    doc["mqtt_server"] = systemConfiguration.mqtt_server;
    doc["mqtt_port"] = systemConfiguration.mqtt_port;
    doc["mqtt_user"] = systemConfiguration.mqtt_user;
    doc["mqtt_password"] = systemConfiguration.mqtt_password;
    doc["thr_temp"] = systemConfiguration.thr_temp;
    doc["thr_hum"] = systemConfiguration.thr_hum;
    doc["thr_press"] = systemConfiguration.thr_press;
    doc["time_update"] = systemConfiguration.time_update; 
    doc["slice_time"] = systemConfiguration.slice_time;
    if(serializeJson(doc, outfile)!=0) {
      outfile.close();
      return true;
    }
    outfile.close();
  }
  return false;
}

String SendHTML(String alertMessage, String currentValues) 
{
    
    String ptr = String("<!doctype html>\n");
    ptr += String("<html lang=\"en\"><head><meta charset=\"utf-8\">\n");
    ptr += String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\"><title>Weather Station | Pixel</title>\n");
    ptr += String("<style>body{margin:0;font-family:Roboto,Arial,sans-serif;font-size:1rem;font-weight:400;line-height:1.5;color:#616264;text-align:left;background-color:#e5e5e5}input[type=number],input[type=password],input[type=text],select{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}input[type=submit]{width:100%;background-color:#4caf50;color:#fff;padding:14px 20px;margin:8px 0;border:none;border-radius:4px;cursor:pointer;font-size:1.2rem}.section-header{text-align:center;padding-top:1.5rem;padding-bottom:1.5rem}.container{max-width:960px;width:100%;padding-right:15px;padding-left:15px;margin-right:auto;margin-left:auto}h2{font-size:1.5rem;margin-top:0;margin-bottom:1rem;font-weight:500;line-height:1.2}hr{margin-top:1rem;margin-bottom:1rem;border:0;border-top:1px solid rgba(0,0,0,.1)}input[type=submit]:hover{background-color:#45a049}footer{color:#6c757d;text-align:center;padding-top:1rem;margin-bottom:1rem;margin-top:1.5rem}.alert{padding:20px;color:#fff;margin-bottom:15px;border:none;border-radius:4px;background-color:#999}.alert.danger{background-color:#f44336}.alert.success{background-color:#4caf50}.closebtn{margin-left:15px;color:#fff;font-weight:700;float:right;font-size:22px;line-height:20px;cursor:pointer;transition:.3s}.closebtn:hover{color:#000}.current-values{background-color:#eee;padding:15px;margin-bottom:20px;border-radius:4px}</style>\n");
    ptr += String("</head><body><div class=\"container\"><div class=\"section-header\"><h1>Pixel Weather Station</h1></div>\n");
    ptr += alertMessage;
    ptr += currentValues;    
    ptr += String("<form action=\"/\" method=\"POST\">\n");
    ptr += String("<h2>Wifi</h2> <label>SSID</label> <input type=\"text\" name=\"wifi-ssid\" value=\"" + systemConfiguration.wifi_ssid + "\" required /> <label>Password</label> <input type=\"password\" name=\"wifi-password\" value=\"**********\" required /><hr/>\n");
    ptr += String("<h2>MQTT</h2> <label>Server</label> <input type=\"text\" name=\"mqtt-server\" value=\"" + systemConfiguration.mqtt_server + "\" required /><label>Port</label> <input type=\"text\" name=\"mqtt-port\" value=\"" + systemConfiguration.mqtt_port + "\" required /><label>Username</label> <input type=\"text\" name=\"mqtt-user\" value=\"" + systemConfiguration.mqtt_user + "\" required /><label>Password</label> <input type=\"password\" name=\"mqtt-password\" value=\"**********\" required /><hr/>\n");
    ptr += String("<h2>Configuration</h2> <label>Temperature Threshold</label> <input type=\"number\" step=\"0.1\" name=\"thr-temp\" value=\"" + String(systemConfiguration.thr_temp) + "\" required /><label>Humidity Threshold</label> <input type=\"number\" step=\"0.1\" name=\"thr-hum\" value=\"" + systemConfiguration.thr_hum + "\" required /><label>Pressure Threshold</label> <input type=\"number\" step=\"0.1\" name=\"thr-press\" value=\"" + systemConfiguration.thr_press + "\" required /><label>Slice size (seconds)</label> <input type=\"number\" name=\"slice-time\" value=\"" + systemConfiguration.slice_time + "\" required /><label>BME280 Time update (seconds)</label> <input type=\"number\" name=\"time-update\" value=\"" + systemConfiguration.time_update + "\" required />\n");
    ptr += String("<input type=\"submit\" value=\"Save\"></form>\n");
    ptr += String("<footer><p>&copy; 2021 TLab</p> </footer></div></body></html>");

    return ptr;
}


String getAlertMessageHtml(String type,String message) 
{
  return String("<div class=\"alert " + type + "\"><span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span>" + message + "</div>");
}

String getCurrentValuesHtml() 
{
  String currentTemperature = String(weatherSensor.readTempC(), 2);
  String currentHumidity = String(weatherSensor.readFloatHumidity(),2);
  String currentPressure = String(weatherSensor.readFloatPressure() / 100,2);
  String currentWindSpeed = String(currentReadings.wind, 2);  
  
  return String("<div class=\"current-values\"><h2>Current Values</h2><label>Temperature (ºC)</label><input type=\"text\" value=\"" + currentTemperature + "\" disabled /><label>Humidity (%)</label><input type=\"text\" value=\"" + currentHumidity + "\" disabled /><label>Pressure (hPA)</label><input type=\"text\" value=\"" + currentPressure + "\" disabled /><label>Wind (rpm)</label><input type=\"text\" value=\"" + currentWindSpeed + "\" disabled /></div>\n");
}


void statusToBlink(int status, int times)
{
  while (--times >= 0) {
    for(int i = 1; i<=status; i++) {
      digitalWrite(STATUS_LED, HIGH);
      delay(400);
      digitalWrite(STATUS_LED, LOW);
      delay(200);  
    }
    delay(2000);
  }
}

void iddleToBlink(int count)
{
    for(int i = 1; i<=count; i++) {
      digitalWrite(STATUS_LED, HIGH);
      delay(1000);
      digitalWrite(STATUS_LED, LOW);
      delay(1000);  
    }
}
