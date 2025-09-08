#include <Arduino.h>
#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoUniqueID.h>
#include <ArduinoJson.h>

// ======== Wi-Fi ========
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ======== Motor AZIMUTE ========
#define DIR_AZ 17
#define STEP_AZ 16
AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);

// ======== Motor ALTITUDE ========
#define DIR_ALT 19
#define STEP_ALT 18
AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// ======== Variáveis globais ========
const float passosPorGrau = 200.0 / 360.0; 
long posicaoAtualAz = 0;
long posicaoAtualAlt = 0;

String deviceIp = "";
String uniqueId = "";

// ======== Funções ========

// Movimento real dos motores
void executarMovimento(float az, float alt) {
  Serial.print("Movendo motores -> AZ: ");
  Serial.print(az);
  Serial.print(", ALT: ");
  Serial.println(alt);

  motorAz.moveTo(round(az * passosPorGrau));
  motorAlt.moveTo(round(alt * passosPorGrau));

  while (motorAz.distanceToGo() != 0 || motorAlt.distanceToGo() != 0) {
    motorAz.run();
    motorAlt.run();
  }

  Serial.println("Movimento concluído!");
}

// Obter UniqueID
String getUniqueIDDevice() {
  if (uniqueId == "") {
    for (size_t i = 0; i < UniqueIDsize; i++) {
      uniqueId += String(UniqueID[i], HEX);
    }
  }
  return uniqueId;
}

// Registrar dispositivo no backend
void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://boout-n8n.sifxgd.easypanel.host/webhook/c98a1d88-d1c6-4521-89f0-25ad255457a2?uid="+uniqueId+"&device_ip="+deviceIp;
  url += "?uid=" + uniqueId + "&device_ip=" + deviceIp;

  http.begin(url);
  http.setTimeout(3000);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.println("Registro OK -> " + http.getString());
  } else {
    Serial.println("Erro registro: " + String(httpCode));
  }
  http.end();
}

// Consultar comandos do backend
void verificarComando() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://boout-n8n.sifxgd.easypanel.host/webhook/2f2dbd00-6dc8-4f02-a336-8a2c02a94196?uid="+uniqueId+"&device_ip="+deviceIp;
  url += "?uid=" + uniqueId + "&device_ip=" + deviceIp;

  http.begin(url);
  http.setTimeout(3000);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Resposta backend: " + payload);

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float az = doc["az"] | 0;
      float alt = doc["alt"] | 0;
      int comandoId = doc["id"] | 0;
      int movido = doc["movido"] | 1;  // se não vier no JSON, assume 1 (já movido)

      if (movido == 0) {
        Serial.println("Novo comando recebido! Executando movimento...");
        executarMovimento(az, alt);

        Serial.println("Comando concluído ID: " + String(comandoId));
      } else {
        Serial.println("Nenhum comando novo para executar.");
      }
    } else {
      Serial.println("Erro parse JSON");
    }

  } else {
    Serial.println("Erro HTTP comando: " + String(httpCode));
  }
  http.end();
}

// Conectar ao Wi-Fi
void connectWifi() {
  Serial.print("Conectando Wi-Fi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  deviceIp = WiFi.localIP().toString();
  Serial.println("Wi-Fi OK, IP: " + deviceIp);
}

// ======== Setup ========
void setup() {
  Serial.begin(115200);

  Serial.println("ESP32: Iniciando Wi-Fi...");
  connectWifi();

  uniqueId = getUniqueIDDevice();
  Serial.println("UniqueID: " + uniqueId);

  Serial.println("Registrando dispositivo...");
  registerDevice();
}

// ======== Loop ========
void loop() {
  static unsigned long ultimaExecucao = 0;
  unsigned long agora = millis();

  if (agora - ultimaExecucao >= 5000) { // a cada 5s
    ultimaExecucao = agora;
    verificarComando();
  }
}
