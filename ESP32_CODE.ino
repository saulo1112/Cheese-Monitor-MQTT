#include <DHT.h>
#include <DHT_U.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "credentials.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

IPAddress server(32,196,10,168);

#define dhtpin 15
#define DHTTYPE DHT22
#define led 2

#define temperature_topic "esp32/sensor" //TOPIC MQTT

float h;
float t;

WiFiClient client;
PubSubClient esp32(client);

DHT dht(dhtpin, DHTTYPE);

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["humidity"] = h;
  doc["temperature"] = t;  //Organizar data en un json
  char jsonBuffer[512];    //crear un buffer
  serializeJson(doc, jsonBuffer); // Convertir en un string y almacenar en el buffer

  esp32.publish(temperature_topic, jsonBuffer); //Public data al topic mqtt
}

void reconnect(){

  while (!esp32.connected()){
    Serial.println("Attempting MQTT connection...");
    if (esp32.connect("esp32Client",  mqtt_user, mqtt_password)){
      Serial.println("Connected");

    } else{
      Serial.println("Failed, rc=");
      Serial.print(esp32.state());
      Serial.println(" Try again in 5 seconds");
      delay(5000);
    }

  }
}

void setup()
{
  // put your setup code here, to run once:

  //pinMode(11, INPUT);
  Serial.begin(9600);
  dht.begin();
  pinMode(led, OUTPUT);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.println("WiFi connected");
  Serial.println("Connected to IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connected to ");
  Serial.println(ssid);

  Serial.println("Connecting to MQTT Broker");
  //client.setInsecure();
  esp32.setServer(server, 1883);

  reconnect();

}

void loop()
{
  // put your main code here, to run repeatedly:

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.begin(ssid, password);
  }

  Serial.println("WiFi connected");
  Serial.println("Connected to IP address: ");
  Serial.println(WiFi.localIP());

  if (!esp32.connected()){
    reconnect();
  }

  delay(5000);

  h = dht.readHumidity();
  t = dht.readTemperature();

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.println("%");
  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));

  publishMessage();
  esp32.loop(); //Mantener conexión con el servidor
  //delay(1000);

  if (t >= 33){
    digitalWrite(led, HIGH);
  } else{
    digitalWrite(led, LOW);
  }

}