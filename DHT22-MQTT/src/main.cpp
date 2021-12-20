#include "Arduino.h"
#include "DHT.h" //DHT library
#include <ESP8266WiFi.h>      //Library for WiFi management on an ESP8266.
#include <ESP8266WiFiMulti.h> //Additional library for WiFi management on an ESP8266.
#include <PubSubClient.h>     //Library for MQTT management.

//WIFI
const char *ssid = "Usine";         //The network's SSID.
const char *password = "nainporte"; //The network's Password.
#define durationSleep 10            //Secondes.
#define NB_TRYWIFI 10               //Number of tests for the wifi connection.
//MQTT
const char *mqtt_server = "192.20.2.112"; //Broker Mqtt's IP Adress.
const int mqttPort = 1883;                //Broker use port.
long tps = 0;

ESP8266WiFiMulti WiFiMulti;
WiFiClient espClient;
PubSubClient client(espClient);

#define DHTPIN 5 //Pin for sensor information.

//Uncomment the sensor uses
//#define DHTTYPE DHT11 // DHT 11
#define DHTTYPE DHT22 // DHT 22 (AM2302)
//#define DHTTYPE DHT21 // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);//Sensor declaration.

//Callback must be present to subscribe to a topic and to plan an action
void callback(char *topic, byte *payload, unsigned int length)
{
   Serial.println("-------Nouveau message du broker mqtt-----");
   Serial.print("Canal:");
   Serial.println(topic);
   Serial.print("donnee:");
   Serial.write(payload, length);
   Serial.println();
   if ((char)payload[0] == '1')
   {
      Serial.println("LED ON");
      digitalWrite(2, HIGH);
   }
   else
   {
      Serial.println("LED OFF");
      digitalWrite(2, LOW);
   }
}

void reconnect()
{
   //Connection to the MQTT Broker.
   while (!client.connected())
   {
      Serial.println("Connection au serveur MQTT ...");
      //Serial display of connection status if established.
      if (client.connect("ESP32Client"))
      {
         Serial.println("MQTT connecté");
      }
      //If the connection failed, serial display of the error.
      else
      {
         Serial.print("echec, code erreur= ");
         Serial.println(client.state());
         Serial.println("nouvel essai dans 2s");
         delay(2000);
      }
   }
}

void setup_wifi()
{
   //Connexion at the wifi.
   WiFiMulti.addAP(ssid, password);

   //Initialization of the number of trials.
   int _try = 0;
   //If the connection is not established and as long as the maximum number of attempts is not reached, retry a connection every 2 seconds.
   while (WiFiMulti.run() != WL_CONNECTED)
   {
      Serial.print("...");
      delay(2000);
      _try++;
      //If the maximum number of tries is reached, send esp in sleep mode for 1h.
      if (_try >= NB_TRYWIFI)
      {
         Serial.println("Impossible to connect WiFi network, go to deep sleep");
         ESP.deepSleep(durationSleep * 360);
      }
   }

   //serial display of wifi connection information.
   Serial.println("");
   Serial.println("WiFi connecté");
   Serial.print("MAC : ");
   Serial.println(WiFi.macAddress());
   Serial.print("Adresse IP : ");
   Serial.println(WiFi.localIP());
}

void setup_mqtt()
{
   client.setServer(mqtt_server, mqttPort); //Definition of the connection path to the MQTT broker.
   client.setCallback(callback);            //Declaration of the subscription function
   reconnect();
}

//Function to publish a float on a topic.
void mqtt_publish(String topic, float t)
{
   char top[topic.length() + 1];
   topic.toCharArray(top, topic.length() + 1);
   char t_char[50];
   String t_str = String(t);
   t_str.toCharArray(t_char, t_str.length() + 1);
   client.publish(top, t_char);
}

void setup()
{
   Serial.begin(115200);
   //Call the method of the wifi setup.
   setup_wifi();
   //Call the method of the MQTT setup.
   setup_mqtt();
   //Send a test publication.
   client.publish("esp82/test", "Hello from ESP8266");
   Serial.println("DHTxx test!"); //Message test
   dht.begin();
}


void loop()
{
   client.loop();
   //We do not use a delay so as not to block the reception of messages
   if (millis() - tps > 60000)
   {
      tps = millis();

      reconnect();

      //The sensor reading takes 250ms
      //The read values ​​can be dated from 2 seconds (slow sensor)
      float temperature = dht.readTemperature();//Temperature reading in celsius (by default).
      float humidity = dht.readHumidity();//Hygrometry reading.
      
      //Checking the reading, in case of failure we repeat a reading.
      if (isnan(temperature) || isnan(humidity))
      {
         Serial.println("Failed to read from DHT sensor!");
         return;
      }

      // Publication of the temperature of the BOC@L to the MQTT broker via the topic "esp82/temperature/bocal"
      mqtt_publish("esp82/bocal/temperature", temperature);
      // Publication of the humidity of the BOC@L to the MQTT broker via the topic "esp82/humidity/bocal"
      mqtt_publish("esp82/bocal/humidity", humidity);

      // debug message for temperature values
      Serial.print("temperature : ");
      Serial.print(temperature);
      Serial.print(" degrees C\t");

      // debug message for hygrometry values
      Serial.print("Humidite: ");
      Serial.print(humidity);
      Serial.println(" %");
   }
}