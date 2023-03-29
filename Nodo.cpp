#include "Nodo.h"

#define BUFFER_SIZE 50

Nodo::Nodo(){
  
}

void _restart(){
  ESP.restart();
}

void Nodo::_iniciarSPIFFS(){
  Serial.print("Incializando SPIFFS... ");
  if(!SPIFFS.begin(true)){
    Serial.println("No se pudo montar incializar SPIFFS, fijate que no haya algun problema con la placa ESP");
    return;
  }
  Serial.println("SPIFFS OK");
}

void Nodo::_iniciarMPU(){
  Serial.print("Buscando acelerómetro MPU6050... ");
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

bool Nodo::conectarServer(const char* server){
  if(!this->conectado){
    Serial.println("No se está conectado a internet");
    return 0;
  }
  this->_server = server;

  HTTPClient http;
  char url[40];
  sprintf(url, "http://%s:%d%s", server, 8080, "/status");
  Serial.println(url);
  http.begin(url); //GET endpoint
    
  Serial.println("Probando API..."); //Json format
  int httpCode = http.GET();
    
  String payload = http.getString();
  Serial.println(httpCode);
  Serial.println(payload);
  http.end();
  return (httpCode == 200);
}

String Nodo::_pedirConfig(const char* _server, int node){
  Serial.print("Obteniendo configuración remota... ");
  
  char initurl[40];
  sprintf(initurl, "http://%s:%d%s", _server, 8080, "/nodes/init");
  
  HTTPClient http; 
  http.begin(initurl); 
  http.addHeader("Content-Type", "application/json");
  char body[12]; sprintf(body, "{\"node\":%d}",node);
  int httpcode = http.POST(body);
  String confg;
  if(httpcode == 200){
    confg = http.getString();
    Serial.println(confg);
  }else{
    confg = "";
    Serial.println("No se logró obtener la configuración remota");
    //return = _getConfigFromFile();
  }
  http.end();
  return confg;
}

String Nodo::obtenerConfig(){
  //Obtener configuración de archivos locales
  DynamicJsonDocument doc(2048);
  String localconfg;
  File archivo = SPIFFS.open("/config.json");
  if (archivo) {
        while (archivo.available()) {
            localconfg += archivo.readString();
        }
        archivo.close();
        Serial.println(localconfg);
    } else {
        Serial.println("Error al abrir el archivo de configuración. Probablemente no existe");
        for(;;);
    }
  DeserializationError error = deserializeJson(doc, localconfg);
  if (error){
    Serial.print(F("deserializeJson() failed en local: "));
    Serial.println(error.c_str());
    for (;;);
  }
  //Conectar WIFI
  _iniciarWIFI(doc["ssid"], doc["password"]);
  //Pedir configuración internet
  String remoteconf = this->_pedirConfig(doc["serverREST"], doc["node"]);
  if(remoteconf == ""){
    return localconfg;
  }
  else{
    return remoteconf;
  }
}

void Nodo::iniciarOnline(const char* _ssid, const char* password){
  Serial.begin(115200);
  while (!Serial)
    delay(100);
  delay(1000);
  
  _iniciarSPIFFS();
  _iniciarMPU();  
  _iniciarWIFI(_ssid, password);
  
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

void Nodo::enviarVectores(int nvectores){
  int httplen = nvectores*358;
  WiFiClient client;
  if(client.connect(this->_server, 8080)) {
    /*******************
     * Enviar headers
     *******************/
    client.print(F("POST ")); client.print(F("/file3")); client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Host: ")); client.print(F(this->_server)); client.print(F("\r\n"));
    client.print(F("User-Agent: vilab_node/2.0\r\n"));
    client.print(F("Accept: */*\r\n"));
    client.print(F("Content-Type: application/json\r\n"));
    client.print(F("Content-Length: ")); client.print(httplen); client.print(F("\r\n\r\n")); //8970 debe ser calculado dependiendo de la duración y el step

    /***************
     * Enviar body
     **************/
    client.print(F("{\"data\":["));
    int len = 9;
    int enviadosLocal = 0;
    int intentos = 0; // Veces que se intenta buscar un vector, si este no aparece despues de 50 intentos, se termina de enviar el JSON.
    
    /* Dentro del body va una lista JSON con N vectores */
    while(enviadosLocal < nvectores && intentos < 50){
      Serial.println("enviados local es menos a nvectores");
      if(this->buffer[this->nroEnviados%50].isUsed == true){
        client.print(this->buffer[this->nroEnviados%50].toJson());
        len += 174; //LEN MINIMO DE UN JSON
        
        Serial.print("Se envío vector nro: "); Serial.println(this->nroEnviados); Serial.print("modulo: "); Serial.println(this->nroEnviados%50);

        /* Agregar comas de separación menos en el último JSON */
        if(enviadosLocal < nvectores-1){
          len +=1;
          client.print(",");
        }
        
        enviadosLocal++;
        this->nroEnviados++;
        intentos = 0;
      }else{
        intentos++;
      }
      /* Si la medición aun no está lista, tiempo de espera para consultar denuevo */
      delay(10);
    }
    client.print("]}");
    len += 2;
    
    /* Completar body con espacios en blanco para igualar largo del body con largo especificado en headers */
    for(int l = len; l<httplen; l++){ 
      client.print("  ");
    }
    
    client.print("\r\n");
    client.flush();
  }
  else{
    Serial.println("Cliente no pudo conectar con servidor"); 
  }
  
  /* TODO:
   * Podriamos escribir todos los vectores en un archivo en SD o Flash.
  */
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
