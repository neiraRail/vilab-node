#include "Nodo.h"
#include "SPIFFS.h"
#include <HTTPClient.h>
#include "WiFi.h"

//Objetos task
TaskHandle_t Task2;

Nodo nodo;
void setup(){
  //nodo.iniciarOnline("railxalkan", "familiarailxalkan", "192.168.18.3");
  nodo.iniciarOnline("RAILWIFI", "", "192.168.43.113");
  Serial.print("setup ejecutandose en core: ");
  Serial.println(xPortGetCoreID());
  
  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
       loop2,                 /* Task function. */
       "Enviar archivos",     /* name of task. */
       10000,                 /* Stack size of task */
       NULL,                  /* parameter of the task */
       1,                     /* priority of the task */
       &Task2,                /* Task handle to keep track of created task */
       !ARDUINO_RUNNING_CORE);/* pin task to core 1 */
    delay(500); 
}


int timestep = 100;
unsigned long eventDuration = 20000;
int nvectores = eventDuration/timestep;
//int event_id = 0; //despues sacar de config.json

void loop2(void * _){
  Serial.print("loop2 ejecutandose en core: ");
  Serial.println(xPortGetCoreID());
  delay(2000);
  while(true){
    //bool test = nodo.buffer[nodo.nroEnviados%50];
    //Serial.println(test.toJson());
    if(nodo.isEvent){
      nodo.enviarVectores2(nvectores);
//DESCOMENTAR PARA TESTEAR CON 1 SOLO 
//      while(){
//        delay(2000);
//      }
    }
    //nodo.verVectores();
    delay(timestep/2);
  }
}


unsigned long previousMillis = 0;

void loop(){
  nodo.alDetectarEvento( []() {
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
  delay(2000); //Esperar 1 segundo para evitar detectar otro altiro
}
