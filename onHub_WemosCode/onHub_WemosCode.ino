#include "SERIAL_COMMUNICATION.h"
#include "PROCESS_DATA.h"
#include "WIFI_PROCESS.h"
#include "COMMON.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include "onHubConfig.h"

SERIAL_COMMUNICATION serial;
WIFI_PROCESS WiFiProcess;
PROCESS_DATA procesamiento;
COMMON codigoComun;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String Data = "";
int jsonIndex = 1, tiempoEspera;

static const int count_mqtt_server = 3;
static char* mqtt_server[count_mqtt_server] = { "157.230.174.83", "test.mosquitto.org", "157.230.174.83"};
char* __mqttServerConnected;
const int serverPort = 1883;
int serverConnectedIndex = 0;

unsigned long lastPublished = 0;
unsigned long lastGetPetition = 0;


void setup() {

  codigoComun.inicializarSalida(wifiPin);
  codigoComun.inicializarSalida(relayPin);
  codigoComun.inicializarSalida(LED_BUILTIN);
  
  codigoComun.escribirSalidaDigital(relayPin, EEPROM.read(relayEEPROMAdressState)); //Cargo en el rele el ultimo estado
  codigoComun.escribirSalidaDigital(wifiPin, LOW); //Apago el bombillo mientras se configura
  
  codigoComun.escribirSalidaDigital(LED_BUILTIN, LOW); //Apago el led
  delay(1000);
  codigoComun.escribirSalidaDigital(LED_BUILTIN, HIGH); //Enciendo el led
  delay(500);
  codigoComun.escribirSalidaDigital(LED_BUILTIN, LOW); //Apago el led
  
  serial.inicializar();
  WiFiProcess.inicializar();

  procesamiento.resetIndex();
  codigoComun.escribirSalidaDigital(wifiPin, HIGH); //Enciendo el led
  setMQTTServer();
  procesamiento.inicializar();
  
  //Test
  String JSON = procesamiento.makeTest();
  Serial.println("Test: " + String(JSON));
}

void loop() {

  if(!WiFiProcess.wifiIsConnected()) setup(); //Reinicio si no hay wifi
  if (!mqttIsConnected()) reconnect(); //Reconectar mqtt si perdio conexion  
  if(Data = "") Data = serial.leerArduino();
  if(Data != "")  {
    if(procesamiento.procesarData(Data))
    {
      jsonIndex = procesamiento.getIndex();
      if(serDebug) Serial.println("Subiendo al sistema  " + String(jsonIndex) + " Mediciones individuales");
      for(int i = 1; i<= jsonIndex; i++)
      {
        publishData(procesamiento.getJson(i));
      }
      procesamiento.resetIndex();
      tiempoEspera = procesamiento.generateRandom();
      procesamiento.setTimeToWait(tiempoEspera);      
      if(serDebug) Serial.println("Nuevo tiempo de espera: " + String(tiempoEspera) + " Segundos");
    }
  }
}

//Funciones MQTT

bool publishData(String strJson){

    int len = 260;
    char Json[len];

    if(strJson != "")
    {
      strJson.toCharArray(Json, 260);
      if(serDebug) Serial.println("Publicando (" + String(__mqttServerConnected) + ") mensaje al topic " +  String(outTopic) + "  Json: " + String(Json));
      
      if (mqttClient.publish(outTopic, Json) == true) {
        return 1;
      } else {
        return 0;
      }
      return 0;
    }
    else
    {
      return 0; //json vacio
    }
}


void setMQTTServer()
{   
  if(serverConnectedIndex < count_mqtt_server-1)
  {
    serverConnectedIndex++;
  }
  else
  {
    serverConnectedIndex = 0;
  }
  __mqttServerConnected = mqtt_server[serverConnectedIndex];//"broker.mqtt-dashboard.com";
  if(serDebug) Serial.println("MQTT Server: " + String(__mqttServerConnected));
  mqttClient.setServer(__mqttServerConnected, serverPort);
  mqttClient.setCallback(callback);
}
bool mqttIsConnected()
{
  return mqttClient.connected();
}

void reconnect() {
  int attemps = 0;
  // Loop until we're reconnected
  while (!mqttIsConnected() && attemps<10) {
    attemps ++;
    if(serDebug) Serial.print("reconectando MQTT...");
    // Create a random client ID
    String clientId = obtenerIdCliente();
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      if(serDebug) Serial.println("Conectado con exito");
      // Once connected, publish an announcement...
      mqttClient.publish("testMQTT", "probando sistema de supervision");
      // ... and resubscribe
      mqttClient.subscribe(inTopic);
    } else {
      if(serDebug) Serial.print("fallo, rc=");
      if(serDebug) Serial.print(mqttClient.state());
      if(serDebug) Serial.println(" intentando nuevamente en 5 segundos");
      // Wait 5 seconds before retrying
      setMQTTServer();
      delay(5000);
    }
  }
}
void callback(char* topic, byte* payload, unsigned int length) {
  if(topic == inTopic)
  {
    String mensaje = "";
    if(serDebug) Serial.println("Message arrived [");
    if(serDebug) Serial.println(topic);
    if(serDebug) Serial.println("] ");
    for (int i = 0; i < length; i++) {
            if(serDebug) Serial.print((char)payload[i]);  
            mensaje +=  (char)payload[i];
    }

   bool state = false;
   if(mensaje.indexOf("ON") > 0) state = true;
   if(mensaje.indexOf("on") > 0) state = true;
   if(mensaje.indexOf("encender") > 0) state = true;            
   if(mensaje.indexOf("prender") > 0) state = true;            
   if(serDebug) Serial.println("");
   if(serDebug) Serial.println("Nuevo estado del rele -- " + String(state));
   
   EEPROM.write(relayEEPROMAdressState, state);  
            
  }
}
String obtenerIdCliente()
{
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  return clientId;
}
