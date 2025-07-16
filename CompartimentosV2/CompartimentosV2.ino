#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <SerialRelay.h>

/* DESENVOLVIDO POR CHAT GPT E LUCAS CARVALHO
 *  13/07/2025
 *  - caso o esp ligue primeiro que o roteador
 *    ele espera 3 minutos e reseta sozinho
 *  - pisca led azul quando abre o portal
 *  - usa o botão EN para reset e o botão BOOT para abrir todos os compartimentos
 *    se pressionado durante o reset
 */

// === PINOS E CONFIG ===
const int led_placa = 2;         // LED da placa
const byte NumModules = 1;
SerialRelay relays(25, 26, NumModules);

const int tempoPulsoRele = 500;  // Tempo de acionamento de cada relé em ms

// === SERVIDOR ===
String servidor = "https://seuServidor.onrender.com";
String rota = "/consulta-maquina01";

// === FUNÇÃO PARA ABRIR TODOS OS COMPARTIMENTOS ===
void abrirTodosCompartimentos() {
  Serial.println("Abrindo todos os compartimentos...");

  for (int i = 1; i <= 4; i++) {
    Serial.println("Acionando relé: " + String(i));
    relays.SetRelay(i, SERIAL_RELAY_ON, 1);
    delay(tempoPulsoRele);
    relays.SetRelay(i, SERIAL_RELAY_OFF, 1);
    delay(300);
  }

  Serial.println("Todos os compartimentos foram abertos.");
}

void setup()
{
  pinMode(led_placa, OUTPUT);
  pinMode(0, INPUT_PULLUP);  // GPIO 0 = BOOT

  Serial.begin(115200);
  delay(100); // Aguarda estabilização

  // Se botão BOOT (GPIO 0) estiver pressionado no boot, abre todos
  if (digitalRead(0) == LOW) {
    Serial.println("BOTÃO BOOT pressionado durante o reset. Abrindo todos os compartimentos...");
    abrirTodosCompartimentos();
    delay(2000); // Pequena pausa após abrir
  }

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  bool portalAtivo = false;
  unsigned long previousMillis = 0;
  const long blinkInterval = 100;
  unsigned long portalStartTime = millis();

  Serial.println("Iniciando tentativa de conexão WiFi...");

  while (!wm.autoConnect("ESP32", "1234567890"))
  {
    if (!portalAtivo)
    {
      Serial.println("Entrou no modo de configuração (portal AP)");
      portalAtivo = true;
    }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval)
    {
      previousMillis = currentMillis;
      digitalWrite(led_placa, !digitalRead(led_placa));
    }

    if ((currentMillis - portalStartTime) > 180000UL)
    {
      Serial.println("Tempo de portal excedido. Reiniciando ESP...");
      ESP.restart();
    }

    delay(10);
  }

  digitalWrite(led_placa, LOW);
  Serial.println("Conectado ao WiFi!");

  for (int i = 1; i <= 4; i++) {
    relays.SetRelay(i, SERIAL_RELAY_OFF, 1);
  }
  Serial.println("Todos os relés desligados.");
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Conexão perdida. Tentando reconectar...");
    WiFi.reconnect();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient https;
    String url = servidor + rota;
    Serial.println("[HTTPS] begin: " + url);

    if (https.begin(url))
    {
      Serial.print("[HTTPS] GET...\n");
      int httpCode = https.GET();
      if (httpCode > 0)
      {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
          String payload = https.getString();
          Serial.println("payload:");
          Serial.println(payload);

          String valor = payload.substring(12, 16);
          Serial.println("payload formatado: " + valor);

          int releIndex = valor.toInt();

          if (releIndex >= 1 && releIndex <= 4)
          {
            Serial.println("Acionando relé: " + String(releIndex));
            relays.SetRelay(releIndex, SERIAL_RELAY_ON, 1);
            delay(tempoPulsoRele);
            relays.SetRelay(releIndex, SERIAL_RELAY_OFF, 1);
          }
          else
          {
            Serial.println("Código de relé inválido: " + valor);
          }
        }
        digitalWrite(led_placa, HIGH);
      }
      else
      {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        digitalWrite(led_placa, LOW);
      }

      https.end();
    }
    else
    {
      Serial.println("[HTTPS] Unable to connect");
    }
  }

  Serial.println("\nTEMPO DE CONSULTA 5 SEGUNDOS");
  delay(5000);
}
