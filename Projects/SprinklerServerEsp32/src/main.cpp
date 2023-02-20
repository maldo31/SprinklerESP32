#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#define WIFI_SSID "MikroTik-JJ2Ghz"
#define WIFI_PASSWORD "japko932"
#define SENSOR 27
#ifdef __cplusplus

extern "C" {

#endif

uint8_t temprature_sens_read();

#ifdef __cplusplus

}

#endif

uint8_t temprature_sens_read();
int LED_BUILTIN = 2;
int sensor_pin = 35;
const int relay = 26;
WebServer server(80);
volatile byte pulseCount;

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
float calibrationFactor = 8.1;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;


bool takeMoistureIntoAccount = false;
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

void getFlow() {
  Serial.println("Get current flow");
  create_json("flow", flowRate, "L/minute");
  server.send(200, "application/json", buffer);
}

void getTotalFlow() {
  Serial.println("Get total flow");
  create_json("totalFlowed", totalMilliLitres/1000, "L");
  server.send(200, "application/json", buffer);
}

String btoss(bool x)
{
  if(x) return "True";
  return "False";
}

int getMoisturePercentage() {
  int moistureValue = analogRead(sensor_pin);
  if (moistureValue > 3300){
     return 0;
  }
  moistureValue = (moistureValue - 700)*0.04;
  int moisturePercentage = 100 - moistureValue;
  
  if(moisturePercentage > 100){
     return 100;
  } 

  else if (moisturePercentage < 0)
  {
     return 0;
  }
  return moisturePercentage*1;
}

void get_moisture(){
  int moistureValue = analogRead(sensor_pin);
  Serial.println(moistureValue);
  float moisturePrecentage = getMoisturePercentage();
  add_json_object("moisture", moisturePrecentage, "%");
  add_json_object("moistureValue", moistureValue, "units");
  serializeJson(jsonDocument, buffer);
  server.send(200, "application/json", buffer);
  jsonDocument.clear();
}

void setupIfToTakeMoistureIntoAccount(){

   if (server.hasArg("plain") == false) {
   }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  bool moistureSensorEnabled = jsonDocument["moistureSensorEnabled"];
  takeMoistureIntoAccount = moistureSensorEnabled;
  String response = "{Moisture sensor enabled set to:" + btoss(moistureSensorEnabled) + "}";
  Serial.println(response);
  server.send(200, "application/json", response);
   
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
     server.send(200, "application/json", "{Switched on}");
  }
  if(relay_value == 1) {
     digitalWrite(relay, HIGH);
     Serial.println("Relay to HIGH");
     server.send(200, "application/json", "{Switched off}");
  }
  
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

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}



void setupSprinkling() {
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  if (takeMoistureIntoAccount == true){
     if (getMoisturePercentage() > 90)
     {
       Serial.println("Soil is too moist");
       server.send(200, "application/json", "{Soil moisture level is above >90%, sprinkling not executed}");
       return;
    }
  }
  else if (sprinklingTimerEnabled == false){
    long duration = jsonDocument["duration"];
    digitalWrite(relay, LOW);
    My_timer = timerBegin(0, 80, true);

    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, duration * 1000000, true);
    timerAlarmEnable(My_timer);
    sprinklingTimerEnabled = true;
    server.send(200, "application/json", "{}");
    return;
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
   while(mdns_init()!= ESP_OK){
    delay(1000);
    Serial.println("Starting MDNS...");
    }
    Serial.println("MDNS started");
    IPAddress serverIp;
  while (serverIp.toString() == "0.0.0.0") {
    Serial.println("Resolving host...");
    delay(250);
    serverIp = MDNS.queryHost("MASTER");
    }
  Serial.println("Host address resolved:");
  Serial.println(serverIp.toString());
  http.begin("http://" + serverIp.toString() + ":8080/UNSECURED/registerEndpoint");
  Serial.println("test123");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["address"] = WiFi.localIP().toString();

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
  server.on("/moisture", get_moisture);
  server.on("/current_flow",getFlow);
  server.on("/total_flow", getTotalFlow);
  server.on("/sprinkleIfMoist", HTTP_POST, setupIfToTakeMoistureIntoAccount);
}


  
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("Hello from the setup");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  Serial.println("Connected. IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN,HIGH);

  setup_routing();
  setup_task();
  addItselfToServer();
  server.begin(); 
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);   
}

bool isConnected = false;

void loop() {
  server.handleClient();
  currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    
    pulse1Sec = pulseCount;
    pulseCount = 0;

    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();

    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;

    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
    
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t");       // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);
    Serial.print("mL / ");
    Serial.print(totalMilliLitres / 1000);
    Serial.println("L");
  }
}