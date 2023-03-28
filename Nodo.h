#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <ESP32_multipart.h>
#include <Adafruit_MPU6050.h>
#include <HTTPClient.h>

class Lectura : public Printable
    {
        public:
            Lectura();
            Lectura(float acc[3], float gyro[3], float temp);
            float* getAcc();
            float* getGyro();
            float getTemp();
            void setValues(float* acc, float* gyro, float temp);
            void setMeta(int node, int event);
            void setTime(unsigned long time_lap);
            char* toJson();
            size_t printTo(Print& p) const;
            bool isUsed = false;
        private:
            float acc[3];
            float gyro[3];
            float temp;
            int node;
            int event;
            unsigned long time_lap;
    };

class Nodo
{
  public:
    Nodo();
    void iniciar();
    void iniciarOnline(const char* _ssid, const char* _password);
    String pedirConfig(const char* _server, int node);
    void iniciarOffline();
    bool conectarServer(const char*_server);
    void capturarEvento(int tiempo, float frequencia);
    Lectura capturarVector();
    void verVectores();
    void enviarEventos();
    bool enviarVector(int m);
    void enviarVectores();
    void enviarVectores2(int nvectores);
    void eliminarEventos();
    void alDetectarEvento(std::function<void()> fn);
    int event =-1;
    int isEvent = false;
    int nroEnviados = 0;
    Lectura buffer[50];
  private:
    Adafruit_MPU6050 mpu; //Dependencia
    void _iniciarSPIFFS();
    void _iniciarWIFI(const char* _ssid, const char* _password);
    void _iniciarMPU();
    const char* _server   = "129.151.100.69"; //IP del servidor de envio de archivos
    bool conectado = false;
    int node = 2;
    
};
