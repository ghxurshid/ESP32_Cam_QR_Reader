#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#include <math.h>

#define LED1_PIN 13
#define LED2_PIN 12

const char* ssidAPP = "ESP32-AP";
const char* passwordAPP = "123456789";

const char* ssidClient = "PLUM TECHNOLOGIES";
const char* passwordClient = "455855454";

const int maxClients = 20;
const uint16_t port = 5784;
const char* template_code = "123456789\n";

// Настройка статического IP-адреса
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(port);
std::vector<NetworkClient> clients;

void blinks(int count = 1, int pulse = 200, int delayBefore = 0, int delayAfter = 0)
{
  count = max(1, count);
  
  delay(delayBefore);
  for (int i = 0; i < count; i ++) {
    digitalWrite(LED2_PIN, HIGH);
    delay(pulse);
    digitalWrite(LED2_PIN, LOW);
    if (i != count - 1) delay(pulse);
  }
  delay(delayAfter);
}

void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  if (WiFi.softAP(ssidAPP, passwordAPP)) {
    blinks();
  } else {
    while(1) {}
  }
  
  if (WiFi.softAPConfig(local_IP, gateway, subnet)) {
    blinks(2);
  } else {
    while(1) {}
  }

  digitalWrite(LED1_PIN, HIGH);
  server.begin();
}

void loop() {
  // Проверка новых подключений
  if (server.hasClient()) {    
    handleNewClient();
    blinks();
  }

  // Обработка существующих клиентов
  handleExistsClients();
}

void handleNewClient() {
  if (clients.size() < maxClients) {
    NetworkClient client = server.available();
    if (client) {
      clients.push_back(client);
      client.print(template_code);
    }
  } else {
    NetworkClient client = server.available();
    if (client) {
      client.stop();
    }
  }
}

void handleExistsClients() {
  for (auto it = clients.begin(); it != clients.end();) {
    if (!(*it).connected()) {
      it = clients.erase(it);      
    } else {
      // Чтение данных от клиента
      if ((*it).available()) {
        String clientData = (*it).readStringUntil('\n');        
        if (clientData == "get_template") {
          sendTemplate((*it));
        } else if (clientData == "get_status") {
          printClientsStatus((*it));
        }
      }
      ++it;
    }
  }
}

void sendTemplate(NetworkClient& client) {
  if (client.connected()) {
    client.print(template_code);
  }
}

void printClientsStatus(NetworkClient& client) {
  if (clients.empty()) {
    client.println("There ara not connected clients:(");
  } else {
    client.println("States of clientd:");
    for (size_t i = 0; i < clients.size(); i++) {
      client.print("Client ");
      client.print(i + 1);
      client.print(": ");
      client.print(clients[i].remoteIP());
      client.print(" - ");
      client.println(clients[i].connected() ? "connected" : "disconnected");
    }
  }
  client.println();
}
