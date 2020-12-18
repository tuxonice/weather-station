#include <WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.
const char* ssid = "ssid";
const char* password = "password";
const char* mqtt_server = "10.10.1.1";
#define mqtt_port 1883
#define MQTT_USER "user"
#define MQTT_PASSWORD "password"
#define MQTT_PUBLISH_TOPIC_WIND "/pixel/wind/sensor"
#define INPUTPIN 15
#define SLICE_SIZE 30000

volatile int count = 0;
volatile unsigned long lastEntry;
int slice = 0;


WiFiClient wifiClient;
PubSubClient client(wifiClient);

portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR isr() {
  portENTER_CRITICAL(&synch);
  if (millis() > lastEntry + 500) {
    count++;
    lastEntry = millis();
  }
  portEXIT_CRITICAL(&synch);
}



void setup() {
  //-- MQTT --  
  Serial.begin(115200);
  Serial.setTimeout(500);// sets the max time to wait for serial data (ms)
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
  
  Serial.print("setup() running on core ");
  Serial.println(xPortGetCoreID());
  pinMode(INPUTPIN, INPUT_PULLUP);
  attachInterrupt(INPUTPIN, isr, RISING);
  lastEntry = millis();
  slice = millis();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 10 seconds before retrying
      delay(10000);
    }
  }
}

void loop() {
  char average[12];
  Serial.println(count);
  if(millis() >= slice + SLICE_SIZE) {
    //calc average
    sprintf(average, "%.02f", (float)count/30.0);
    portENTER_CRITICAL_ISR(&synch); // início da seção crítica
    count = 0;  
    portEXIT_CRITICAL_ISR(&synch); // fim da seção crítica
    slice = millis();
    Serial.println(average);
    publishSerialData(MQTT_PUBLISH_TOPIC_WIND, average);
  }
  delay(1000);
}

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void publishSerialData(char * topic, char * serialData){
  if (!client.connected()) {
    reconnect();
  }
  client.publish(topic, serialData);
}
