#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

const char* ssid = "ESP32-AP";
const char* password = "123456789";
const int maxClients = 20;
const uint16_t port = 5784;
const char* template_code = "123456789\n";

// Настройка статического IP-адреса
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(port);
std::vector<WiFiClient> clients;

void setup() {
  Serial.begin(115200);

  // Настройка точки доступа
  WiFi.softAP(ssid, password);
  // Установка статического IP-адреса
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("Ошибка при установке статического IP-адреса");
  }

  Serial.print("IP адрес точки доступа: ");
  Serial.println(WiFi.softAPIP());

  // Запуск сервера
  server.begin();
  Serial.println("TCP сервер запущен и слушает порт 5784");
}

void loop() {
  // Проверка новых подключений
  if (server.hasClient()) {
    handleNewClient();
  }

  // Обработка существующих клиентов
  handleExistsClients();
}

void handleNewClient() {
  if (clients.size() < maxClients) {
    WiFiClient client = server.available();
    if (client) {
      clients.push_back(client);
      client.print(template_code);
      Serial.println("Новый клиент подключен");
    }
  } else {
    WiFiClient client = server.available();
    if (client) {
      client.stop();
      Serial.println("Клиент отклонен: превышено максимальное количество подключений");
    }
  }
}

void handleExistsClients() {
  for (auto it = clients.begin(); it != clients.end();) {
    if (!(*it).connected()) {
      it = clients.erase(it);
      Serial.println("Клиент отключен");      
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

void sendTemplate(WiFiClient client) {
  if (client.connected()) {
    client.print(template_code);
  }
}

void printClientsStatus(WiFiClient client) {
  if (clients.empty()) {
    client.println("Нет подключенных клиентов");
  } else {
    client.println("Статус клиентов:");
    for (size_t i = 0; i < clients.size(); i++) {
      client.print("Клиент ");
      client.print(i + 1);
      client.print(": ");
      client.print(clients[i].remoteIP());
      client.print(" - ");
      client.println(clients[i].connected() ? "подключен" : "отключен");
    }
  }
  client.println();
}
