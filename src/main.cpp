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
#define MICROSTEPPING 16  // Ajuste conforme seu driver (1, 2, 4, 8, 16, etc.)
const float passosPorGrau = (200.0 * MICROSTEPPING) / 360.0; 
String deviceIp = "";
String uniqueId = "";

// Controle de movimento
bool movimentoEmExecucao = false;

// ======== Funções ========

// Função de movimento NÃO BLOQUEANTE
void iniciarMovimento(float az, float alt) {
  Serial.print("Iniciando movimento -> AZ: ");
  Serial.print(az);
  Serial.print("°, ALT: ");
  Serial.print(alt);
  Serial.println("°");

  long passosAz = round(az * passosPorGrau);
  long passosAlt = round(alt * passosPorGrau);
  
  Serial.print("Passos calculados: AZ=");
  Serial.print(passosAz);
  Serial.print(", ALT=");
  Serial.println(passosAlt);

  motorAz.moveTo(passosAz);
  motorAlt.moveTo(passosAlt);
  movimentoEmExecucao = true;
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
  String url = "https://boout-n8n.sifxgd.easypanel.host/webhook/c98a1d88-d1c6-4521-89f0-25ad255457a2";
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
  if (WiFi.status() != WL_CONNECTED || movimentoEmExecucao) return;
  
  HTTPClient http;
  String url = "https://boout-n8n.sifxgd.easypanel.host/webhook/2f2dbd00-6dc8-4f02-a336-8a2c02a94196";
  url += "?uid=" + uniqueId + "&device_ip=" + deviceIp;
  
  http.begin(url);
  http.setTimeout(3000);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Resposta backend: " + payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      float az = doc["az"] | 0;
      float alt = doc["alt"] | 0;
      int comandoId = doc["id"] | 0;
      int movido = doc["movido"] | 1;  // Padrão: 1 (já movido)
      
      Serial.print("Comando recebido - ID: ");
      Serial.print(comandoId);
      Serial.print(", Movido: ");
      Serial.println(movido);
      
      // SÓ MOVE SE movido == 0
      if (movido == 0) {
        Serial.println("Iniciando execução do comando...");
        iniciarMovimento(az, alt);
      } else {
        Serial.println("Comando já executado. Ignorando.");
      }
    } else {
      Serial.println("Erro parse JSON: " + String(error.c_str()));
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
  
  Serial.println("\nWi-Fi OK, IP: " + WiFi.localIP().toString());
  deviceIp = WiFi.localIP().toString();
}

// ======== Setup ========
void setup() {
  Serial.begin(115200);
  Serial.println("=== SISTEMA DE CONTROLE DE MOTORES ===");
  
  // Configuração dos motores
  motorAz.setMaxSpeed(1000);        // Velocidade máxima (passos/seg)
  motorAz.setAcceleration(500);     // Aceleração (passos/seg²)
  motorAlt.setMaxSpeed(1000);
  motorAlt.setAcceleration(500);
  
  // Posição inicial
  motorAz.setCurrentPosition(0);
  motorAlt.setCurrentPosition(0);
  
  // Conexão Wi-Fi
  connectWifi();
  
  // Identificação do dispositivo
  uniqueId = getUniqueIDDevice();
  Serial.println("UniqueID: " + uniqueId);
  
  // Registro no backend
  registerDevice();
  
  // Teste inicial dos motores
  Serial.println("Teste inicial dos motores...");
  motorAz.moveTo(1600);  // Meia volta (com microstepping 16)
  motorAlt.moveTo(1600);
}

// ======== Loop ========
void loop() {
  static unsigned long ultimaVerificacao = 0;
  unsigned long agora = millis();
  
  // Processamento contínuo dos motores (ESSENCIAL!)
  motorAz.run();
  motorAlt.run();
  
  // Verifica se o movimento terminou
  if (movimentoEmExecucao && 
      motorAz.distanceToGo() == 0 && 
      motorAlt.distanceToGo() == 0) {
    
    movimentoEmExecucao = false;
    Serial.println("Movimento concluído!");
  }
  
  // Verifica novos comandos a cada 5 segundos
  if (agora - ultimaVerificacao >= 5000) {
    ultimaVerificacao = agora;
    verificarComando();
  }
}