#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <AdafruitIO.h>

#include <time.h>

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com" // io.adafruit.com
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "Uororiz"
#define AIO_KEY         "aio_Ijtz32qBi85yNplegsV1izjHhKf1"

#define FIREBASE_HOST "airpurifier-uororiz.firebaseio.com"
#define FIREBASE_AUTH "GKcusIaEqbmrZhN7lifpFMpxU2XAYcPnC8CpWxhX"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish calidadAire = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/calidadaire");
Adafruit_MQTT_Publish purificador = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/purificadorencendido");
Adafruit_MQTT_Publish ventilador = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/ventiladorencendido");

Adafruit_MQTT_Subscribe purificadorSubscribe = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/purificadorencendido");
Adafruit_MQTT_Subscribe ventiladorSubscribe = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/ventiladorencendido");


/*************************** Sketch Code ************************************/

void digitalCallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a onoff callback, the button value is: ");
  Serial.println(data);

  String message = String(data);
  message.trim();
  if (message == "ON") {
    digitalWrite(12, HIGH);
  }
  if (message == "OFF") {
    digitalWrite(12, LOW);
  }
}

void analogicaCallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a onoff callback, the button value is: ");
  Serial.println(data);

  String message = String(data);
  message.trim();
  analogWrite(13, message.toInt());
}


//-------------------VARIABLES GLOBALES--------------------------
int contconexion = 0;

const char *ssid = "Uororiz";
const char *password = "casa12345";

unsigned long previousMillis = 0;

boolean ventiladorEncendido = false;
boolean ventiladorEncendidoUltimo = false;

boolean purificadorEncendido = false;
boolean purificadorEncendidoUltimo = false;


const int a0 = A0;
const int VENTILADOR = 5;
const int SENSOR = 4;

// time set variables para obtener la hora por internet
int timezone = -5 * 3600;
int dst = 0;

String pathCalidadAire = "/calidadAire";

String pathEstado = "/estado";
String keyPath;
//objetos de la libreria de firebaseesp8266
FirebaseData firebaseData;
FirebaseJson json;
FirebaseJson json2;

//-------------------------------------------------------------------------

// Callback function header
void setup() {

  // Inicia Serial
  Serial.begin(115200);
  Serial.println("");
  pinMode(A0, INPUT);
  pinMode(VENTILADOR, OUTPUT);
  pinMode(SENSOR, OUTPUT);

  digitalWrite(VENTILADOR, HIGH);
  digitalWrite(SENSOR, HIGH);

  // Conexión WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED and contconexion < 50) { //Cuenta hasta 50 si no se puede conectar lo cancela
    ++contconexion;
    delay(500);
    Serial.print(".");
  }
  if (contconexion < 50) {
    //para usar con ip fija
    IPAddress ip(192, 168, 1, 156);

    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);

    Serial.println("");
    Serial.println("WiFi conectado");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Error de conexion");
  }


  mqtt.subscribe(&purificadorSubscribe);
  mqtt.subscribe(&ventiladorSubscribe);

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  //Establezca el tiempo de espera de lectura de la base de datos en 1 minuto (máximo 15 minutos)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);

  //Tamaño y  tiempo de espera de escritura, tiny (1s), small (10s), medium (30s) and large (60s).
  //tiny, small, medium, large and unlimited.
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for Internet time");

  while (!time(nullptr)) {
    Serial.print("*");
    delay(1000);
  }
  Serial.println("Time response....OK!");
}

bool activado;

//--------------------------LOOP--------------------------------
void loop() {
  MQTT_connect();

  unsigned long currentMillis = millis();


  if (keyPath != "" ) { //obtiene la ruta del ultimo estado ingresado desde firebase para que se pueda encender y apagar los componentes desde la página web
    delay(1000);

    Firebase.getBool(firebaseData, pathEstado + "/" + keyPath + "/purificadorEncendido");

    activado = firebaseData.boolData();
    digitalWrite(SENSOR, !activado);

    delay(1000);

    Firebase.getBool(firebaseData, pathEstado + "/" + keyPath + "/ventiladorEncendido");

    activado = firebaseData.boolData();
    digitalWrite(VENTILADOR, !activado);

  }
  while ((subscription = mqtt.readSubscription(1000))) {//lee las suscripciones desde adafruit.io que a su vez éste las recibe desde IFTT con el comando de voz de google assistant
    if (subscription == &purificadorSubscribe) {
      Serial.print(F("Got: "));
      Serial.println(((char *)purificadorSubscribe.lastread));


      if ((strcmp((char *)purificadorSubscribe.lastread, "ON") == 0)) {//si IFTT le envia un ON a adafruit.io entonces se enciende, en dado caso de recibir un OFF se apaga el pin
        digitalWrite(SENSOR, LOW);
        Serial.println("PURIFICADOR ENCENDIDO");
        digitalWrite(VENTILADOR, LOW);
        Serial.println("VENTILADOR ENCENDIDO");
      } else {
        digitalWrite(SENSOR, HIGH);
        Serial.println("PURIFICADOR APAGADO");
        digitalWrite(VENTILADOR, HIGH);
        Serial.println("VENTILADOR APAGADO");
      }
    }

    if (subscription == &ventiladorSubscribe) {
      Serial.print(F("Got: "));
      Serial.println(((char *)ventiladorSubscribe.lastread));

      if ((strcmp((char *)ventiladorSubscribe.lastread, "ON") == 0)) {//si IFTT le envia un ON a adafruit.io entonces se enciende, en dado caso de recibir un OFF se apaga el pin
        digitalWrite(VENTILADOR, LOW);
        Serial.println("VENTILADOR ENCENDIDO");
      } else {
        digitalWrite(VENTILADOR, HIGH);
        Serial.println("VENTILADOR APAGADO");
      }
    }
  }

  if ((digitalRead(SENSOR) == 0)) {//lee el estado del pin que controla el relevador del sensor, si éste es 0 entonces está encendido, de lo contrario está apagado
    purificadorEncendido = true;
    ventiladorEncendido = true;
  } else {
    purificadorEncendido = false;
    ventiladorEncendido = false;
  }

  if (digitalRead(VENTILADOR) == 0) {//lee el estado del pin que controla el relevador del ventilador, si éste es 0 entonces está encendido, de lo contrario está apagado
    ventiladorEncendido = true;
  } else {
    ventiladorEncendido = false;
  }


  if ((currentMillis - previousMillis >= 5000) && purificadorEncendido) { //envia la calidad de aire cada 5 segundos y solo si el purificador está encendido
    previousMillis = currentMillis;
    int analog = analogRead(A0);
    float calidad = analog;
    Serial.print(F("\nSending calidad val "));
    Serial.println(calidad);
    Serial.println("...\n");


    if (! calidadAire.publish(calidad)) {//si se logra publicar la calidad en adafruit.io entonces procede a crear el json y hace push a firebase
      Serial.println(F("Failed"));
    } else {

      String fecha = obtenerFecha();//llama la función de fecha

      json.set("calidadNivel", double(calidad));//se castea la variable por errores de firebase
      json.set("fecha", fecha);

      Firebase.pushJSON(firebaseData, pathCalidadAire, json);//se envia el json calidadAire a firebase

      Serial.println(F("OK!"));
    }

  }

  if ((ventiladorEncendido != ventiladorEncendidoUltimo) || (purificadorEncendido != purificadorEncendidoUltimo)) { //envia el estado del purificador y ventilador solamente cuando cambia.
    ventiladorEncendidoUltimo = ventiladorEncendido;
    purificadorEncendidoUltimo = purificadorEncendido;

    Serial.println(F("\nEstado ventilador val "));
    Serial.println(ventiladorEncendido);
    Serial.println("...");

    Serial.print(F("\nEstado purificador val "));
    Serial.println(purificadorEncendido);
    Serial.println("...");

    String fecha = obtenerFecha();//se manda a llamar la función de fecha

    json2.set("purificadorEncendido", purificadorEncendido);
    json2.set("ventiladorEncendido", ventiladorEncendido);
    json2.set("fecha", fecha);

    Firebase.pushJSON(firebaseData, pathEstado, json2);//se hace push del json estado a firebase

    keyPath = firebaseData.pushName();//se obtiene la nueva ruta del ultimo estado ingresado a firebase

    Serial.println(F("OK!"));
    delay(1000);
  }

  // this is our 'wait for incoming subscription packets and callback em' busy subloop
  // try to spend your time here:
  mqtt.processPackets(500);

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds

  // if(! mqtt.ping()) {
  //   mqtt.disconnect();
  // }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
  Adafruit_MQTT_Subscribe *subscription;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 10 seconds...");
    mqtt.disconnect();
    delay(10000);  // wait 10 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}

String obtenerFecha() {//función que obtiene la fecha y hora actual desde internet
  String fecha;
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  //se obtiene cada valor de fecha por separado
  String iday = String(p_tm->tm_mday);
  String imonth = String(p_tm->tm_mon + 1);
  String iyear = String(p_tm->tm_year + 1900);
  String ihour = String(p_tm->tm_hour);
  String imin = String(p_tm->tm_min);
  String isec = String(p_tm->tm_sec);

  fecha = iday + '/' + imonth + '/' + iyear + ' ' + ihour + ':' + imin + ':' + isec;//se aplica el formato deseado para la fecha

  Serial.println(fecha);//se imprime en consola con fines de monitoreo

  return fecha;//regresa el string de fecha formateado
}
