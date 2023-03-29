#include "Nodo.h"

// Objetos task
TaskHandle_t Task2;

DynamicJsonDocument doc(2048);
int timestep;
unsigned long eventDuration;
int nvectores;
const char *ssid;
const char *password;
unsigned long time_reset;

Nodo nodo;
void setup()
{
  const char *server;
  nodo.iniciarOffline();
  String confg = nodo.obtenerConfig();

  DeserializationError error = deserializeJson(doc, confg);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    for (;;);
  }
  ssid = (const char *)doc["ssid"];
  password = (const char *)doc["password"];
  server = (const char *)doc["serverREST"];
  eventDuration = (unsigned long)doc["time_event"];
  timestep = (int)doc["delay_sensor"];
  time_reset = (int)doc["time_reset"];
  nvectores = (int)doc["batch_size"];

  nodo.iniciarOnline(ssid, password);
  if (nodo.conectarServer(server))
  {
    Serial.println("Server REST OK");
  }
  else
  {
    Serial.println("Server REST ERROR");
    for(;;);
  }

  xTaskCreatePinnedToCore(
      loop2,                  /* Task function. */
      "Enviar archivos",      /* name of task. */
      10000,                  /* Stack size of task */
      NULL,                   /* parameter of the task */
      1,                      /* priority of the task */
      &Task2,                 /* Task handle to keep track of created task */
      !ARDUINO_RUNNING_CORE); /* pin task to core 1 */
  delay(500);
}

/*****************************************************
 * Loop principal, se encarga de capturar mediciones
 * cada 'timestep' milisegundos y por un periodo de
 * tiempo 'eventDuration'
 ****************************************************/
unsigned long previousMillis = 0;
void loop(){  
  nodo.alDetectarEvento([](){
    unsigned long start = millis();
    previousMillis = millis();
    while (previousMillis - start < eventDuration) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= timestep) {
        nodo.capturarVector();
        previousMillis = currentMillis;
      }
    } 
  });
  delay(2000); // Esperar 1 segundo para evitar detectar otro altiro
}


/*****************************************************
 * Loop secundario, se encarga de enviar los vectores
 * en paquetes de 'nvectores'
 ****************************************************/
void loop2(void *_){
  Serial.print("loop2 ejecutandose en core: ");
  Serial.println(xPortGetCoreID());
  delay(2000);
  while (true) {
    if(millis()>time_reset*3600000){
      ESP.restart();
    }
     
    if(nodo.nroEnviados%(eventDuration/timestep) == 0){
      nodo.isDone = true;
    }
    
    if (nodo.isEvent || !nodo.isDone)
    {
      nodo.enviarVectores(nvectores);
    }
    delay(timestep / 2);
  }
}
