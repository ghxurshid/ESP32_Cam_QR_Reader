#include <WiFi.h>

#define LED1_PIN 12
#define LED2_PIN 13

NetworkServer server(5764);
SemaphoreHandle_t shared_var_mutex = NULL;

void TaskServerHandler(void *pvParameters);
void TaskQRCodeReader1(void *pvParameters);

std::vector<NetworkClient> clients;
 
void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  delay(10);
  
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  server.begin();

  shared_var_mutex = xSemaphoreCreateMutex();
  xTaskCreate(TaskServerHandler, "Server handler", 2048, NULL, 1, NULL);
  xTaskCreate(TaskQRCodeReader1, "QRCode reader", 2048, NULL, 1, NULL);  
}

void loop() {
  // put your main code here, to run repeatedly: 
}

void TaskServerHandler(void *pvParameters) {
  while(1) {
    NetworkClient client = server.accept();  // listen for incoming clients
    if (client) {
      int m = 0;
      do {
        m = xSemaphoreTake(shared_var_mutex, portMAX_DELAY);
        if (m == pdTRUE) {
          clients.push_back(client);
          xSemaphoreGive(shared_var_mutex);
        }
      }
      while( m == pdFALSE );      
    }
    delay(10);
  }
}

void TaskQRCodeReader1(void *pvParameters) {
  
}

