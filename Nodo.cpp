#include "Nodo.h"
#include <Arduino.h>
#include "SPIFFS.h"
#include <Adafruit_MPU6050.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESP32_multipart.h>

#define BUFFER_SIZE 50

Nodo::Nodo(){
  
}


void Nodo::_iniciarSPIFFS(){
  Serial.println("Incializando SPIFFS...");
  if(!SPIFFS.begin(true)){
    Serial.println("No se pudo montar incializar SPIFFS, fijate que no haya algun problema con la placa ESP");
    return;
  }
  Serial.println("SPIFFS OK");
}

void Nodo::_iniciarMPU(){
  Serial.println("Buscando acelerómetro MPU6050...");
  if (!this->mpu.begin()) {
    Serial.println(F("No se ecnontró el chip MPU6050, porfavor fijate en la conexión con el sensor."));
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 OK");                 

  //Establecer el rango del acelerómetro y del giroscópio
  this->mpu.setAccelerometerRange(MPU6050_RANGE_4_G); //Rango del acelerómetro
  this->mpu.setGyroRange(MPU6050_RANGE_250_DEG);       //Rango del giroscópio

  //Establecer filtros del acelerómetro
  //mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  //mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
  
  //Establecer configuración del motion detection
  this->mpu.setMotionDetectionThreshold(2);   //Threshold para la detección de movimiento (G)
  this->mpu.setMotionDetectionDuration(25); //Duración mínima del evento para la detección (ms)
  
  //Aun no sé para qué se utiliza en.
  this->mpu.setInterruptPinLatch(true);  // Keep it latched.  Will turn off when reinitialized.
  this->mpu.setInterruptPinPolarity(true);
  this->mpu.setMotionInterrupt(true);
}

void _restart(){
  ESP.restart();
}

void Nodo::_iniciarWIFI(const char* _ssid, const char* _password){
  unsigned long mytime;
  
  Serial.print("Intentando conectar a la siguiente red: ");
  Serial.println(_ssid);
  WiFi.begin(_ssid, _password);
  mytime = millis();
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if(millis() - mytime > 5000){
        _restart();
      }
  }
  Serial.println("");
  Serial.print("WiFi conectado en ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  this->conectado = true;
}

void Nodo::iniciarOffline(){
  Serial.begin(115200);
  while (!Serial)
    delay(100);
  delay(1000);
  _iniciarSPIFFS();
  _iniciarMPU();

  Serial.println("Nodo inicializado OFFLINE");
}

void Nodo::iniciar(){
  this->iniciarOnline("railxalkan", "familiarailxalkan", "129.151.100.69");
}

bool Nodo::conectarServer(const char* server){
  if(!this->conectado){
    Serial.println("No se está conectado a internet");
    return 0;
  }
  this->_server = server;
  char url[40];
  sprintf(url, "http://%s:%d%s", server, 8080, "/status");
  Serial.println(url);
  this->http.begin(url); //GET endpoint
    
  Serial.println("Probando API..."); //Json format
  int httpCode = http.GET();
    
  String payload = this->http.getString();
  Serial.println(httpCode);
  Serial.println(payload);
  http.end();
  return (httpCode == 200);
}

void Nodo::iniciarOnline(const char* _ssid, const char* password, const char* _server){
  Serial.begin(115200);
  while (!Serial)
    delay(100);
  delay(1000);
  
  _iniciarSPIFFS();
  _iniciarMPU();
  _iniciarWIFI(_ssid, password);
  if(conectarServer(_server)){
    Serial.println("Server REST OK");
  }
  else{
    Serial.println("Server REST ERROR");
  }
  sprintf(this->enviar_url, "http://%s:%d%s", this->_server, 8080, "/events");
  this->http.begin(this->enviar_url);
  this->http.addHeader("Content-Type", "application/json");
  Serial.println("Nodo incializado ONLINE");
}


Lectura Nodo::capturarVector(){
  Lectura lectura;
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  lectura.setValues(a.acceleration.v, g.gyro.v, temp.temperature);
  lectura.setTime(millis());
  lectura.setMeta(this->node, this->event);
  int i;
  for (i = 0; i < BUFFER_SIZE; i++) {
    // Check if the current element is unused
    if (!this->buffer[i].isUsed) {
        //Nos aseguramos que el siguiente elemento no esté usado. Obligadamente se va a sobreescribir el siguiente.
        //Borrar primero y luego escribir para evitar que el enviador se pase de largo.
        if(i<BUFFER_SIZE-1){
          this->buffer[i+1].isUsed = false;
        }
        //Si estamos alfinal del buffer, se indica el primer elemento como no usado. Se sobreescribirá el primer elemento
        else{
          this->buffer[0].isUsed = false;
        }
        //Escribir
        this->buffer[i] = lectura;
        this->buffer[i].isUsed = true;
        
        
        Serial.print("Vector guardado en elemento ");
        Serial.print(i);
        Serial.println(" del buffer");
        break;
    }
  }
  //Si el buffer está lleno y el primero por alguna razón no se desusó.
  if (i==BUFFER_SIZE){
    this->buffer[0] = lectura;
    this->buffer[1].isUsed = false;    
  }
  return lectura;
}

void Nodo::verVectores(){
  for(int i=0; i<BUFFER_SIZE; i++){
    if(this->buffer[i].isUsed)
      Serial.println(this->buffer[i].toJson());    
  }
}

void Nodo::enviarVectores2(int nvectores){
  WiFiClient client;
  if(client.connect("192.168.43.113", 8080)) {
    client.print(F("POST ")); client.print(F("/file3")); client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Host: ")); client.print(F("192.168.43.113")); client.print(F("\r\n"));
    client.print(F("User-Agent: vilab_node/2.0\r\n"));
    client.print(F("Accept: */*\r\n"));
    client.print(F("Content-Type: application/json\r\n"));
    client.print(F("Content-Length: ")); client.print("35880"); client.print(F("\r\n\r\n")); //8970 debe ser calculado dependiendo de la duración y el step
    //comienza el objeto del body
    client.print(F("{\"data\":["));
    int len = 9;
    while(this->nroEnviados < nvectores){
      //Poner una condición extra.
      if(this->buffer[this->nroEnviados%50].isUsed == true){
        client.print(this->buffer[this->nroEnviados%50].toJson());
        len += 174; //LEN MINIMO DE UN JSON
        Serial.print("Se envío vector nro: ");
        Serial.println(this->nroEnviados);
        Serial.print("modulo: ");
        Serial.println(this->nroEnviados%50);
        if(this->nroEnviados < nvectores-1){
          len +=1;
          client.print(",");
        }
        this->nroEnviados++;
      }
    delay(10);
    }
    client.print("]}");
    len += 2;
    for(int l = len; l<35880; l++){ //8970 debe ser calculado a partir de duración y el step
      client.print("  "); //Considerar doblar o agregar constante
    }
    client.print("\r\n");
    client.flush();
    //        Serial.print("Se terminó el envío, total len:");
    //        Serial.println(len);
    this->nroEnviados = 0;
  }
}



void Nodo::enviarVectores(){
  for(int i=this->nroEnviados%BUFFER_SIZE; i<BUFFER_SIZE; i++){
    if (this->buffer[i].isUsed) {
      if(this->enviarVector(i)){
        Serial.print("Enviado elemento: ");
        Serial.println(this->nroEnviados%BUFFER_SIZE);
        this->nroEnviados++;
      }
      else{
        //Guardar en archivo.
        Serial.println("Problema al enviar");
        break;
      }
    }
    else{
      //Serial.println("Lectura isUsed es falso");
      break;
      }
    }
  }
bool Nodo::enviarVector(int m){
  if(!this->conectado){
    Serial.println("No se está conectado a internet");
    return false;
  }
  //Serial.println(url);
  
  //Enviar a endpoint de vectores.
  int httpCode = this->http.POST(this->buffer[m].toJson());
  String payload = http.getString();
  //Serial.println(httpCode);
  //Serial.println(payload);
  return (httpCode == 200);
}

void Nodo::alDetectarEvento(std::function<void()> fn){
  Serial.println("Esperando evento...");
  while(!this->mpu.getMotionInterruptStatus()){
    
  }
  Serial.println("Evento detectado");
  this->event++;
  this->isEvent = true;
  fn();
  this->isEvent = false;
}



/* CLASE LETURA*/

Lectura::Lectura(){
  for(int i=0; i<3; i++){
    this->acc[i] = 0;
    this->gyro[i] = 0;
  }
  this->temp = 0;
  this->node = 2;
  this->event = -1;
}

Lectura::Lectura(float* newacc, float* newgyro, float newtemp) {
    acc[0] = newacc[0];
    acc[1] = newacc[1];
    acc[2] = newacc[2];
    gyro[0] = newgyro[0];
    gyro[1] = newgyro[1];
    gyro[2] = newgyro[2];
    this->temp = temp;
}

float* Lectura::getAcc(){
    return this->acc;
}

float* Lectura::getGyro(){
    return this->gyro;
}

float Lectura::getTemp(){
    return this->temp;
}

void Lectura::setValues(float* newacc, float* newgyro, float newtemp){
    acc[0] = newacc[0];
    acc[1] = newacc[1];
    acc[2] = newacc[2];
    gyro[0] = newgyro[0];
    gyro[1] = newgyro[1];
    gyro[2] = newgyro[2];
    temp = newtemp;    
}

void Lectura::setMeta(int node, int evento){
  this->node = node;
  this->event = evento;
  
}

void Lectura::setTime(unsigned long time_lap){
  this->time_lap = time_lap;
}

char* Lectura::toJson(){
  static char json[200];
  sprintf(json, "{\"time_lap\":%lu,\"node\":%d,\"event\":%d,\"acc_x\":%.4f,\"acc_y\":%.4f,\"acc_z\":%.4f,\"gyr_x\":%.4f,\"gyr_y\":%.4f,\"gyr_z\":%.4f,\"temp\":%.4f,\"mag_x\":0,\"mag_y\":0,\"mag_z\":0}", 
          this->time_lap, this->node, this->event, this->acc[0], this->acc[1], this->acc[2], this->gyro[0], this->gyro[1], this->gyro[2], this->temp);
  return json;
}

char * float2s(float f, unsigned int digits)
{
  int index = 0;
  static char s[16];                    // buffer to build string representation
  // handle sign
  if (f < 0.0)
  {
    s[index++] = '-';
    f = -f;
  } 
  // handle infinite values
  if (isinf(f))
  {
    strcpy(&s[index], "INF");
    return s;
  }
  // handle Not a Number
  if (isnan(f)) 
  {
    strcpy(&s[index], "NaN");
    return s;
  }

  // max digits
  if (digits > 6) digits = 6;
  long multiplier = pow(10, digits);     // fix int => long

  int exponent = int(log10(f));
  float g = f / pow(10, exponent);
  if ((g < 1.0) && (g != 0.0))      
  {
    g *= 10;
    exponent--;
  }
 
  long whole = long(g);                     // single digit
  long part = long((g-whole)*multiplier);   // # digits
  char format[16];
  sprintf(format, "%%ld.%%0%dld E%%+d", digits);
  sprintf(&s[index], format, whole, part, exponent);
  
  return s;
}

char * float2s(float f)
{
  return float2s(f, 2);
}

size_t Lectura::printTo(Print& p) const
{
    size_t bytes = 0;
    bytes += p.print("{ \"time_lap\":"); bytes += p.print(millis()); bytes += p.write(',');
    bytes += p.print("\"acc_x\":"); bytes += p.print(float2s(this->acc[0])); bytes += p.write(',');
    bytes += p.print("\"acc_y\":"); bytes += p.print(float2s(this->acc[1])); bytes += p.write(',');
    bytes += p.print("\"acc_z\":"); bytes += p.print(float2s(this->acc[2])); bytes += p.write(',');
    bytes += p.print("\"gyr_x\":"); bytes += p.print(float2s(this->gyro[0])); bytes += p.write(',');
    bytes += p.print("\"gyr_y\":"); bytes += p.print(float2s(this->gyro[1])); bytes += p.write(',');
    bytes += p.print("\"gyr_z\":"); bytes += p.print(float2s(this->gyro[2])); bytes += p.write('}');
    return bytes;
}

void Nodo::eliminarEventos(){
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      String fileName = file.name();
      if(fileName.startsWith("evento")){
          SPIFFS.remove("/"+fileName);
      }
      file = root.openNextFile();
    }
}

void Nodo::enviarEventos(){
  if(!this->conectado){
    Serial.println("No se está conectado a internet");
    return;
  }
  // Abre el directorio raíz del sistema de archivos
  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("Error al abrir el directorio raíz");
    return;
  }

  // Contador para contar el número de archivos encontrados
  int count = 0;

  // Recorre todos los archivos del directorio raíz
  //Cambiar a while(continuar) y poner if(file) dentro para poder manejar los errores
  File file = root.openNextFile();
  while (file) {
    // Verifica si el nombre del archivo comienza con "evento"
    if (String(file.name()).startsWith("evento")) {
      // Envía el archivo utilizando la función multipart.sendFile
      ESP32_multipart multipart("129.151.100.69");
      multipart.setPort(8080);
      int result = multipart.sendFile("/files", file);
      if (result == 0) {
        Serial.println("Error al enviar el archivo " + String(file.name()));
      } else {
        // Elimina el archivo si se envía satisfactoriamente
        Serial.println("Se envió el archivo "+String(file.name()));
        SPIFFS.remove("/"+String(file.name()));
      }
      // Incrementa el contador
      count++;
    }
    file = root.openNextFile();
  }

  // Si no se encontró ningún archivo, notifica por serial
  if (count == 0) {
    Serial.println("No se encontraron archivos de eventos en el sistema de archivos");
  }
  else{
    Serial.println("Ya no hay más archivos");
  }
}




String generarFilename() {
  // Inicializa el contador de archivos
  int fileCount = 0;

  // Busca los archivos eventoXXX.json en el sistema de archivos
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    // Verifica si el nombre del archivo comienza con "evento"
    if (String(file.name()).startsWith("evento")) {
      // Incrementa el contador de archivos
      fileCount++;
    }
    // Busca el siguiente archivo
    file = root.openNextFile();
  }

  // Genera el nombre del archivo con el siguiente número disponible
  String filename = "/evento";
  filename += String(fileCount + 1);
  filename += ".json";

  // Devuelve el nombre del archivo
  return filename;
}
void Nodo::capturarEvento(int tiempo, float frequencia){
  int sampleRate = 1000 / frequencia; // Calculate the sample rate in milliseconds
  //Comenzamos a contar
  unsigned long start = millis();
  bool capturando = true;
  String nombre = generarFilename();
  //Abrir un archivo para escribir
  File file = SPIFFS.open(nombre, FILE_WRITE);
  if(!file){
    Serial.println("Fallo al abrir archivo para escribir");
    capturando = false;
    return;
  }
  file.println("[");
  //Contador de milisegundos para setear la frecuencia de muestreo
  unsigned long previousMillis = 0;
  while(capturando){
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= sampleRate) {
      //Leer datos
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      
      Lectura lectura;
      lectura.setValues(a.acceleration.v, g.gyro.v, temp.temperature);
      
      //Imprimir en archivo
      //Serial.println(lectura.getAcc()[0]);
      file.println(lectura);
      
      //Si ha pasado el tiempo asignado
      if(millis()-start > tiempo){
        // Finalizar evento
        capturando = false;
        file.println("]");
        file.close();
      }
      else{
        //Si no se finaliza, se pone una coma
        file.println(",\n");
      }
      previousMillis = currentMillis;
    }
    
  }
  Serial.println("Evento guardado en "+nombre);  
}
