// Lib WIFI
#include <ESP8266WiFi.h>
// Lib Thérmomètre
#include "DHT.h"
// Lib parsing Json
#include <ArduinoJson.h>
// OLED Library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// Lib i2c
#include <Wire.h>

#define OLED_RESET 16
Adafruit_SSD1306 display(OLED_RESET);

// Définition pin thérmomètre
#define DHTPIN 14   
// Définition type thérmomètre
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE,15);

// Variables :
const char* ssid       = "ESTIAM Wifi";
const char* password   = "r8d8c3p0r2d2";
const char* host = "www.dweet.io";
const char* thing = "dht_estiam_florian";       // node sur dweet.io
const char* thing2 = "radctrl_estiam_florian";  // node sur dweet.io
const char* content1 = "T1";                    // température
const char* content2 = "H1";                    // température
const int httpPort = 80;
String line;                                    // Retour du call (avec header)
String url;
int timeout;
String json;
boolean httpBody;

// Setup
void setup() {

  // Vitesse du bus
  Serial.begin(57600); 

  // Secure delay
  delay(100);

  // Affectation de la boardled
  pinMode(LED_BUILTIN, OUTPUT);
  // et allumage
  digitalWrite(LED_BUILTIN, LOW);


  display.begin(SSD1306_SWITCHCAPVCC, 0x78>>1);
  display.display();
  delay(2000);
  display.clearDisplay();

  // Récupération des données sur le bus i2c
  dht.begin();

  // Affichage du texte avant connexion 
  //[non fonctionnel pour cause de conflit avec la lib Adafruit_SSD1306]
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting to ");
  display.print(ssid);
  display.display();
  delay(1000);
  
  // Initiation de la connexion
  WiFi.begin(ssid, password);

  // ménager l'utilisateur
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    display.print(".");
    display.display();
  }

  // Effacer l'ancien affichage puis afficher l'IP
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi connected");  
  display.println("IP address: ");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
  display.clearDisplay();

}

// fonction d'affichage de la température sur le lcd
void showTemp(float temp,float hud) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0); // température
  display.print("T=");
  display.print(temp);
  display.println("C"); // saut de ligne
  display.print("H="); // humidité
  display.print(hud);
  display.println("%");
  display.display();
  display.clearDisplay();
}

// fonction d'envoi de la température sur dweet.io
void dweetTemp(float temp,float hud) {

  Serial.print("connecting to ");
  Serial.println(host);

  // Ouverture d'une connexion http
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // formatage du payload  
  url = "/dweet/for/";
  url += thing;
  url += "?";
  url += content1;
  url += "=";
  url += temp;
  url += "&";
  url += content2;
  url += "=";
  url += hud;  

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // Requète
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "User-Agent:Mozilla/5.0(Windows NT 6.3; WOW64; rv:48.0) Gecko/20100101 Firefox/48.0" +
               "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" +
               "Connection: close\r\n\r\n");
  // Implémentation d'un timeout
  timeout = millis() + 5000;
  while (client.available() == 0) {
    if (timeout - millis() < 0) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // Récupération de la réponse
  while(client.available()){
    line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
}

// Fonction de récupération de l'état voulu du SSR (relai)
void dweetRad() {

  Serial.print("connecting to ");
  Serial.println(host);

  // Ouverture d'une connexion http
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // Formatage du payload
  url = "/get/latest/dweet/for/";
  url += thing2;
  json = "";
  httpBody = false;

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // Requète 
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "User-Agent:Mozilla/5.0(Windows NT 6.3; WOW64; rv:48.0) Gecko/20100101 Firefox/48.0" +
               "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" +
               "Connection: close\r\n\r\n");
  // Timeout
  timeout = millis() + 5000;
  while (client.available() == 0) {
    if (timeout - millis() < 0) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // réponse (avec extraction du corp JSON)
  while (client.available()) {
    line = client.readStringUntil('\r');
    if (!httpBody && line.charAt(1) == '{') {
      httpBody = true;
    }
    if (httpBody) {
      json += line;
    }
  }
  Serial.println("Got data:");
  Serial.println(json);
  
  //##############################################
  // Grosse prise de tête sur le parsing du json #
  //##############################################
  // définition de la taille du buffer
  const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 140;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parsing
  JsonObject& root = jsonBuffer.parseObject(json);

  // Récupération de l'état de la requète
  const char* state = root["this"]; // "succeeded"

  // Récupération des datas
  JsonObject& with0 = root["with"][0];          // Le node contenant les Datas
  const char* with0_thing = with0["thing"];     // "radctrl_estiam_florian"
  const char* with0_created = with0["created"]; // "2017-09-28T11:30:19.045Z"

  // Notre fameux booléen qui annonce l'allumage ou l'extiction du radiateur
  int with0_content_radctrl = with0["content"]["radctrl"]; // dépend du retour de l'api

  // verdict :
  Serial.print("Le radiateur doit être allumé : ");
  Serial.println(with0_content_radctrl);  

  // Allumage ou extinction du radiateur (simuler par une led : il 
  // faisait 26°c en moyenne dans la pièce durant toute la journée) :
  with0_content_radctrl ? digitalWrite(LED_BUILTIN, LOW) : digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.println();
  Serial.println("closing connection");
}

// Nous entrons dans la boucle principale du programme :
void loop() {
  // Qui doit s'executer toutes les secondes
  delay(1000);

  // lecture des variables
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // fallback
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Affichage sur la console
  Serial.print("Humidity: "); 
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: "); 
  Serial.print(t);
  Serial.print(" *C \n");

  // Execution des fonction
  showTemp(t,h);
  dweetTemp(t,h);
  dweetRad();
}
