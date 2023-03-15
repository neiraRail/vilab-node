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
  WiFiClient client;
  delay(2000);
  while(true){
    //bool test = nodo.buffer[nodo.nroEnviados%50];
    //Serial.println(test.toJson());
    if(nodo.isEvent){
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
        while(nodo.nroEnviados < nvectores){
          if(nodo.buffer[nodo.nroEnviados%50].isUsed == true){
            client.print(nodo.buffer[nodo.nroEnviados%50].toJson());
            len += 174; //LEN MINIMO DE UN JSON
            Serial.print("Se envío vector nro: ");
            Serial.println(nodo.nroEnviados);
            Serial.print("modulo: ");
            Serial.println(nodo.nroEnviados%50);
            if(nodo.nroEnviados < nvectores-1){
              len +=1;
              client.print(",");
            }
            nodo.nroEnviados++;
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
        nodo.nroEnviados = 0;
      }
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
