#include <Arduino.h>
#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>
#include <HttpClient.h>
#include <ArduinoUniqueID.h>
#include <ArduinoJson.h>
// ======== Wi-Fi ========
// const char* ssid = "Alicio";
// const char* password = "1512231718";
const char *ssid = "PEDRO HENRIQUE";
const char *password = "20240204";

// ======== Motor AZIMUTE ========
#define DIR_AZ 17
#define STEP_AZ 16
AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);

// ======== Motor ALTITUDE ========
#define DIR_ALT 19
#define STEP_ALT 18
AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// ======== Web Server ========
WebServer server(80);

// ======== Variáveis globais ========
const float passosPorGrau = 200.0 / 360.0; // Corrigido
long posicaoAtualAz = 0;
long posicaoAtualAlt = 0;

String deviceIp = "";
String uniqueId = "";

void executarMovimento(float az, float alt)
{
  Serial.println("Iniciando movimentação!");
  Serial.println("az:" + String(az));
  Serial.println("alt:" + String(alt));

  float grausAz = az;
  float grausAlt = alt;

  long novaPosAz = grausAz * passosPorGrau;
  long novaPosAlt = grausAlt * passosPorGrau;

  motorAz.moveTo(novaPosAz);
  motorAlt.moveTo(novaPosAlt);

  posicaoAtualAz = novaPosAz;
  posicaoAtualAlt = novaPosAlt;

  server.send(200, "text/plain", "Movendo para AZ: " + String(grausAz) + "°, ALT: " + String(grausAlt) + "°");
  Serial.println("Movimentação realizada!");
}

void handleMover()
{
  Serial.print("Tentando mover...");
  if (server.hasArg("az") && server.hasArg("alt"))
  {
    float grausAz = server.arg("az").toFloat();
    float grausAlt = server.arg("alt").toFloat();

    long novaPosAz = grausAz * passosPorGrau;
    long novaPosAlt = grausAlt * passosPorGrau;

    motorAz.moveTo(novaPosAz);
    motorAlt.moveTo(novaPosAlt);

    posicaoAtualAz = novaPosAz;
    posicaoAtualAlt = novaPosAlt;

    server.send(200, "text/plain", "Movendo para AZ: " + String(grausAz) + "°, ALT: " + String(grausAlt) + "°");
  }
  else
  {
    server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
  }
}

String getUniqueIDDevice()
{
  Serial.begin(115200);
  UniqueIDdump(Serial);
  if (uniqueId == "")
  {
    for (size_t i = 0; i < UniqueIDsize; i++)
    {
      uniqueId += String(UniqueID[i], HEX);
    }
  }
  return uniqueId;
}

void registerDevice(String uniqueId, String deviceIp)
{

  Serial.println("Registrando dispositivo...");

  HTTPClient http;
  String url = "https://boout-n8n.sifxgd.easypanel.host/webhook/c98a1d88-d1c6-4521-89f0-25ad255457a2?uid=" + uniqueId + "&device_ip=" + deviceIp;

  Serial.println("URL: " + url);

  http.begin(url.c_str());
  int httpCode = http.GET();

  if (httpCode > 0)
  {
    Serial.println("Enviando requisição...");
    String payload = http.getString();
    Serial.println("Resposta: " + payload);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    else
    {
      Serial.println("JSON convertido com sucesso.");
      /*
      {
        "id": 3,
        "device_id": "1",
        "nome_astro": "sun",
        "lat": "-20.6531238",
        "lng": "-40.4911397",
        "az": "185.44",
        "alt": "-75.29",
        "movido": 0
      }
      */
      // long time = doc["time"];
      //  float latitude = doc["data"][0];
      int movido = doc["movido"].as<int>();

      if (movido == 0)
      {
        float az = 0.0;
        float alt = 0.0;

        if (doc.containsKey("az"))
        {
          const char *azStr = doc["az"];
          if (azStr != nullptr)
            az = atof(azStr);
        }

        if (doc.containsKey("alt"))
        {
          const char *altStr = doc["alt"];
          if (altStr != nullptr)
            alt = atof(altStr);
        }

        Serial.printf("az: %.2f, alt: %.2f\n", az, alt);
        executarMovimento(az, alt);
      }
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpCode);
  }
  http.end();
}

void setup()
{

  Serial.begin(115200);

  Serial.print("Tentando conexão com o Wi-Fi");

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  WiFi.begin(ssid, password);

  motorAz.setMaxSpeed(100);
  motorAz.setAcceleration(10);
  motorAlt.setMaxSpeed(100);
  motorAlt.setAcceleration(10);

  server.on("/mover", handleMover);
  server.begin();
  Serial.println("[✓] Servidor HTTP iniciado!");
}

void loop()
{

  Serial.println("Obtendo Endereço de IP do dispositivo...");
  deviceIp = WiFi.localIP().toString();

  Serial.println("DeviceIp:" + deviceIp);
  delay(500);

  Serial.println("Obtendo UniqueID do dispositivo");
  uniqueId = getUniqueIDDevice();

  while (uniqueId == "")
  {
    delay(500);
    Serial.print(".");
  }

  delay(500);

  Serial.println("Registrando dispositivo...");
  registerDevice(uniqueId, deviceIp);
  Serial.println("Dispositivo registrado!");

  server.handleClient();
  motorAz.run();
  motorAlt.run();
}

// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // ======== Wi-Fi ========
// const char* ssid = "PEDRO HENRIQUE";
// const char* password = "20240204";

// // ======== Motor AZIMUTE ========
// #define DIR_AZ 17
// #define STEP_AZ 16
// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);

// // ======== Motor ALTITUDE ========
// #define DIR_ALT 19
// #define STEP_ALT 18
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// // ======== Web Server ========
// WebServer server(80);

// // ======== Variáveis globais ========
// const float passosPorGrau = 1.0 / 1.8; // 1 passo = 1.8 graus
// float atualAz = 0.0;
// float atualAlt = 0.0;

// void handleMover() {
//   if (server.hasArg("az") && server.hasArg("alt")) {
//     float az = server.arg("az").toFloat();
//     float alt = server.arg("alt").toFloat();

//     long passosAz = az * passosPorGrau;
//     long passosAlt = alt * passosPorGrau;

//     motorAz.moveTo(passosAz);
//     motorAlt.moveTo(passosAlt);

//     atualAz = az;
//     atualAlt = alt;

//     server.send(200, "text/plain", "Movendo para AZ: " + String(az) + ", ALT: " + String(alt));
//   } else {
//     server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//   }
// }

// void setup() {
//   Serial.begin(115200);

//   WiFi.begin(ssid, password);
//   Serial.print("Conectando ao Wi-Fi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.print("\n[✓] Wi-Fi conectado! IP: " + WiFi.localIP().toString());

//   motorAz.setMaxSpeed(100);
//   motorAz.setAcceleration(10);
//   motorAlt.setMaxSpeed(100);
//   motorAlt.setAcceleration(10);

//   server.on("/mover", handleMover);
//   server.begin();
//   Serial.print("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();
//   motorAz.run();
//   motorAlt.run();
// }
