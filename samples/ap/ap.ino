#include <WiFi.h>
#include <WebServer.h>

/* Put your SSID */
const char* ssid = "ESP32-PIXEL";  // Enter SSID here

/* Put IP Address details */
IPAddress local_ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

// Set web server port number to 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Setting AP (Access Point)…");
  WiFi.softAP(ssid);
  delay(2000); // VERY IMPORTANT
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.persistent(false);

  delay(1000);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  
  server.begin();
  Serial.println("HTTP server started");
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


String SendHTML(){
  String ptr = "<!doctype html> <html lang=\"en\">\n";
  ptr +="<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\n";
  ptr +="<title>Weather Station | Pixel</title>\n";
  ptr +="<style>\n";
  ptr +="body{margin:0;font-family:Roboto,\"Helvetica Neue\",Arial,\"Noto Sans\",sans-serif;font-size:1rem;font-weight:400;line-height:1.5;color:#212529;text-align:left;background-color:#f8f9fa}input[type=password],input[type=text],select{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}input[type=submit]{width:100%;background-color:#4caf50;color:#fff;padding:14px 20px;margin:8px 0;border:none;border-radius:4px;cursor:pointer;font-size:1.2rem}.section-header{text-align:center;padding-top:1.5rem;padding-bottom:1.5rem}.container{max-width:960px;width:100%;padding-right:15px;padding-left:15px;margin-right:auto;margin-left:auto}h2{font-size:1.5rem;margin-top:0;margin-bottom:1rem;font-weight:500;line-height:1.2}hr{margin-top:1rem;margin-bottom:1rem;border:0;border-top:1px solid rgba(0,0,0,.1)}input[type=submit]:hover{background-color:#45a049}footer{color:#6c757d;text-align:center;padding-top:1rem;margin-bottom:1rem;margin-top:1.5rem}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body><div class=\"container\"><div class=\"section-header\"><h1>Pixel Weather Station 3</h1></div><form action=\"#\" method=\"POST\"><h2>Wifi</h2> <label for=\"wifi-ssid\">SSID</label> <input type=\"text\" id=\"wifi-ssid\" name=\"wifi-ssid\" required><label for=\"wifi-password\">Password</label> <input type=\"password\" id=\"wifi-password\" name=\"wifi-password\" required><hr/><h2>MQTT</h2> <label for=\"mqtt-server\">Server</label> <input type=\"text\" id=\"mqtt-server\" name=\"mqtt-server\" required><label for=\"mqtt-user\">Username</label> <input type=\"text\" id=\"mqtt-user\" name=\"mqtt-user\" required><label for=\"mqtt-password\">Password</label> <input type=\"password\" id=\"mqtt-password\" name=\"mqtt-password\" required><hr/><h2>MQTT Topics</h2> <label for=\"mqtt-topic-wind\">Wind</label> <input type=\"text\" id=\"mqtt-topic-wind\" name=\"mqtt-topic-wind\" required> <label for=\"mqtt-topic-laser\">laser</label> <input type=\"text\" id=\"mqtt-topic-laser\" name=\"mqtt-topic-laser\" required><hr/><h2>Threshold</h2> <label for=\"threshold-temperature\">Temperature</label> <select id=\"threshold-temperature\" name=\"threshold-temperature\"><option value=\"1\">1</option><option value=\"1.5\">1.5</option><option value=\"2\">2</option> </select> <label for=\"threshold-humidity\">Humidity</label> <select id=\"threshold-humidity\" name=\"threshold-humidity\"><option value=\"1\">1</option><option value=\"3\">3</option><option value=\"5\">5</option> </select> <label for=\"threshold-pressure\">Pressure</label> <select id=\"threshold-pressure\" name=\"threshold-pressure\"><option value=\"10\">10</option><option value=\"20\">20</option><option value=\"30\">30</option> </select><input type=\"submit\" value=\"Save\"></form> <footer><p>&copy; 2020 TLab</p> </footer></div></body>\n";
  ptr +="</html>\n";
  return ptr;
}
