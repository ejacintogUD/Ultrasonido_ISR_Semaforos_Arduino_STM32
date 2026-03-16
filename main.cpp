#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

//definiciones de pines
#define LED_PIN 13
#define echoPin 6
#define trigPin 7

//semaforo para sincronizar la lectura del echo
SemaphoreHandle_t xSemaphore;

//prototipo de funciones
void initUltrasound(void *pvParameters);
void readUltrasound(void *pvParameters);
void blinkLED(void *pvParameters);
void isrEchoPin();

//variable para almacenar el tiempo del echo
volatile long timer;

void setup() {
  //inicializacion de pines
  pinMode(LED_PIN, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //inicializacion de comunicacion serial
  Serial.begin(9600);
  Serial.println("Ultrasound Sensor with FreeRTOS");
  //creacion del semaforo
  xSemaphore = xSemaphoreCreateBinary();
  //creacion de tareas
  xTaskCreate(initUltrasound, "Read Ultrasound", 128, NULL, 1, NULL);
  xTaskCreate(readUltrasound, "Read Ultrasound", 128, NULL, 1, NULL);
  xTaskCreate(blinkLED, "Blink LED", 128, NULL, 1, NULL);
  //configuracion de interrupcion para el pin echo
  attachInterrupt(digitalPinToInterrupt(echoPin), isrEchoPin, FALLING);
  //iniciar el scheduler de FreeRTOS
  vTaskStartScheduler();
}

void loop() {
  //el loop queda vacio ya que las tareas se ejecutan de manera concurrente  
}


//funcion para parpadear el LED cada 500ms
void blinkLED(void *pvParameters) {
  while (1) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void initUltrasound(void *pvParameters) {
  while (1) {
    //enviar pulso ultrasónico
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    
    // desactivar la interrupcion para evitar que se active mientras se espera el echo
    detachInterrupt(digitalPinToInterrupt(echoPin));

    //esperar mientras se activa el echo
    while (digitalRead(echoPin) == LOW); //esperar a que el echo se active

    //activar la interrupcion para capturar el tiempo del echo
    attachInterrupt(digitalPinToInterrupt(echoPin), isrEchoPin, CHANGE);
    //limpiar el semaforo antes de esperar a que se libere
    xSemaphoreTake(xSemaphore, 0); //limpiar el semaforo sin esperar

    timer = micros(); //capturar el tiempo de inicio del echo
    //esperar a que la tarea de lectura calcule la distancia despues de que se active el echo y se libere el semaforo
      
      vTaskDelay(1000 / portTICK_PERIOD_MS); //esperar 1 segundo antes de la siguiente medicion
    }
   
}


void readUltrasound(void *pvParameters) {
  while (1) {
    //esperar a que la interrupcion capture el tiempo del echo
    // tomando el semafor lanzado por la interrupcion para calcular la distancia
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      //calcular distancia
      long duration = micros() - timer; //calcular la duracion del echo
      float distance = (duration * 0.0343) / 2; //calcular la distancia en cm
      if (distance >= 200 || distance <= 2) {
        Serial.println("Out of range");
      } else {
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
              }
    }
  }
}


void isrEchoPin() {
  //liberar el semaforo para que la tarea de lectura pueda calcular la distancia
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}