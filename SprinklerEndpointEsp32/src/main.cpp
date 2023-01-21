#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#define WIFI_SSID "MikroTik-JJ2Ghz"
#define WIFI_PASSWORD "japko932"

#ifdef __cplusplus

extern "C" {

#endif

uint8_t temprature_sens_read();

#ifdef __cplusplus

}

#endif

uint8_t temprature_sens_read();
int LED_BUILTIN = 2;
const int relay = 26;
WebServer server(80);


StaticJsonDocument<250> jsonDocument;
char buffer[250];

float temperature;
bool sprinklingTimerEnabled = false;

hw_timer_t *My_timer = NULL;

void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();  
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);
}

void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

void read_sensor_data(void * parameter) {
   for (;;) {
     temperature = (temprature_sens_read() - 32) / 1.8;
     Serial.println("Read sensor data");
 
     vTaskDelay(60000 / portTICK_PERIOD_MS);
   }
}

void getTemperature() {
  Serial.println("Get temperature");
  create_json("temperature", temperature, "°C");
  server.send(200, "application/json", buffer);
}

void handlePost() {
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  int relay_value = jsonDocument["relay"];
  if(relay_value == 0){
     digitalWrite(relay, LOW);
     Serial.println("Relay to LOW");
  }
  if(relay_value == 1) {
     digitalWrite(relay, HIGH);
     Serial.println("Relay to HIGH");
  }
  server.send(200, "application/json", "{Cannot}");
}

void IRAM_ATTR onTimer(){
  digitalWrite(relay, HIGH);
  Serial.println("Relay switched off");
  timerAlarmDisable(My_timer);
  timerWrite(My_timer, 0);
  timerDetachInterrupt(My_timer);
  timerEnd(My_timer);
  sprinklingTimerEnabled = false;
}

void setupSprinkling() {
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  if (sprinklingTimerEnabled == false){
    long duration = jsonDocument["duration"];
    digitalWrite(relay, LOW);
    My_timer = timerBegin(0, 80, true);

    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, duration * 1000000, true);
    timerAlarmEnable(My_timer);
    sprinklingTimerEnabled = true;
    server.send(200, "application/json", "{}");
    
  }
  else{
    Serial.println("Error ongoin sprinkling");
    server.send(200, "application/json", "{Can't sprinkle, ongoing sprinkling}");
  }
}

void setup_task() {    
  xTaskCreate(     
  read_sensor_data,      
  "Read sensor data",      
  1000,      
  NULL,      
  1,     
  NULL     
  );     
}

void addItselfToServer() {
   HTTPClient http;
  //  while(mdns_init()!= ESP_OK){
  //   delay(1000);
  //   Serial.println("Starting MDNS...");
  //   }
  //   Serial.println("MDNS started");
  //   IPAddress serverIp;
  // while (serverIp.toString() == "0.0.0.0") {
  //   Serial.println("Resolving host...");
  //   delay(250);
  //   serverIp = MDNS.queryHost("MASTER");
  //   }
  // Serial.println("Host address resolved:");
  // Serial.println(serverIp.toString());
  http.begin("http://192.168.88.254:8080/gpio/add_endpoint");
  Serial.println("test123");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["address"] = WiFi.localIP().toString();
  doc["name"] = "itsmemoron";

  String requestBody;
  serializeJson(doc, requestBody);
  int httpResponseCode = http.POST(requestBody);

  Serial.println(httpResponseCode);
  }


void setup_routing() {
  server.on("/temperature", getTemperature);
  server.on("/relay", HTTP_POST, handlePost);
  server.on("/sprinkle", HTTP_POST, setupSprinkling);
  server.on("/add_to_server", addItselfToServer);
}


  
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(921600);
  Serial.println("Hello from the setup");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  pinMode(relay, OUTPUT);
  Serial.println("Connected. IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);

  setup_routing();
  setup_task();
  addItselfToServer();
  server.begin();    
}

bool isConnected = false;

void loop() {
  server.handleClient();
}