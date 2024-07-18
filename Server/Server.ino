#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

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

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  while (!Serial) {
    delay(100);
  }

  // Информация о проекте и авторе
  Serial.println("====================================\n");
  Serial.println("Проект: ESP32Cam Wi-Fi TCP Сервер\n");
  Serial.println("Описание: Этот проект реализует точку доступа Wi-Fi и TCP сервер,\n");
  Serial.println("принимающий подключения клиентов и обрабатывающий данные.\n");
  Serial.println("Автор: Хуршид Хужаматов\n");
  Serial.println("Дата: Июль 2024\n");
  Serial.println("====================================\n");
  Serial.println("\n");

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to \n");
  Serial.print(ssidClient);
  Serial.print(":\n");

  WiFi.begin(ssidClient, passwordClient);

  int attempCount = 0;
  bool wifiSuccess = false;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".\n");
    delay(500);
    if (attempCount ++ > 20) {
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" ERROR\n");
  } else {
    Serial.println(" OK\n");
    Serial.print("IP address: \n");
    Serial.print(WiFi.localIP().toString());
    Serial.println("\n");
    wifiSuccess = true;
  }

  if (!wifiSuccess) {
    Serial.print("Creating soft app \n");
    Serial.print(ssidAPP);
    Serial.print(": ");

    // Настройка точки доступа
    if (WiFi.softAP(ssidAPP, passwordAPP)) {
      Serial.println("OK\n");
      wifiSuccess = true;

      // Установка статического IP-адреса
      Serial.print("IP address: \n");
      if (WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.print(local_IP.toString());
        Serial.println("\n");
      } else {
        Serial.println("Error\n");
      }
    } else {
      Serial.println("ERROR\n");
    }
  }

  digitalWrite(LED1_PIN, wifiSuccess ? HIGH : LOW);

  // Запуск сервера
  server.begin();
  Serial.println("Server is started - OK\n");
}

void loop() {
  // Проверка новых подключений
  if (server.hasClient()) {
    digitalWrite(LED2_PIN, HIGH);
    handleNewClient();
    digitalWrite(LED2_PIN, LOW);
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
      Serial.print("New client connected: \n");
      Serial.print(client.remoteIP().toString());
      Serial.println("\n");
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
      Serial.println("Client erased\n");
    } else {
      // Чтение данных от клиента
      if ((*it).available()) {
        String clientData = (*it).readStringUntil('\n');
        Serial.print(clientData);
        Serial.println("\n");
        (*it).println(clientData);
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
