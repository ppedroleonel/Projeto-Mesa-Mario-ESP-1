/**
 *  Nome: Pedro Leonel de Lorena, Leonardo Ferrarese Correa, Lais Rodrigues Sevilhano & Luigi Arnosti Reginato
 *  Descrição: Neste projeto haverão dois displays onde serão exibidos os funcionários e se estes estão trabalhando, permitindo que os mesmos registrem seus inícios e finais de turno.
 *  Projeto: Bater ponto - Sistema de presença de funcionários
 *  Data: 06/05/2026
 *  Versão: 0.8
 */

 
#include <Arduino.h>
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include <Bounce2.h>
#include <HTTPCLIENT.H>
#include "LED.h"
#include "secrets.h"

// * PINOS

const int PINO_LED_RGB = 48;
const int QUANTIDADE_LEDS = 1;
const int PINO_LAMPADA = 20;

const int BotaoUP = 15;
const int BotaoDOWN = 16;
const int BotaoSELECT = 17;

//* TOPICO

// TODO: COLOCAR A URL_API* no secrets
const char TOPICO_COMANDO[] = "senai134/pedroleonel/esp32/comando";




// * VARIAVEIS DO atualizarDisplay()
int coordenada = 0;

bool LaisisTrabalhando = false;
bool LeonardoisTrabalhando = false;
bool LuigiisTrabalhando = false;
bool PedroisTrabalhando = false;

// variavel pra controlar a lampada
bool lampada = false;

// * VARIAVEIS PARA DA coletarHorra()
String tempoLocal;
String tempoTraduzido;

// * CRIAÇÃO DE OBJETOS

LiquidCrystal_I2C lcd(0x27, 20, 4);

Led LEDA(20);

Adafruit_NeoPixel ledRGB(
    QUANTIDADE_LEDS,
    PINO_LED_RGB,
    NEO_GRB + NEO_KHZ800 // constante de configuração
);

Bounce DOWN = Bounce();
Bounce UP = Bounce();
Bounce SELECT = Bounce();

// * PROTÓTIPOS DAS FUNÇÕES

void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void alterarCorLedRGB(int vermelho, int verde, int azul);
void tratarJsonComando(const String &mensagem);
void acenderLampada(bool lampadaParametro);
String coletarHora();
void AtualizarDisplay();
void FuncaoSELECT();
void AtualizarLED();

void setup()
{

  lcd.init();
  lcd.backlight();

  pinMode(20, OUTPUT);

  DOWN.attach(BotaoDOWN, INPUT_PULLUP);
  UP.attach(BotaoUP, INPUT_PULLUP);
  SELECT.attach(BotaoSELECT, INPUT_PULLUP);

  configurarDebug();
  configurarLedRGB();
  conectarWiFi();
  configurarMQTT();
  registrarCallBackMensagem(tratarMensagemRecebida);
  conectarMQTT();
  AtualizarDisplay();

  
}

void loop()
{
  LEDA.update();

  garantirWiFiConectado();
  garantirMQTTConectado();
  loopMQTT();

  DOWN.update();
  SELECT.update();
  UP.update();

  if (DOWN.fell())
  {
    coordenada--;
    AtualizarDisplay();
  }
  if (UP.fell())
  {
    coordenada++;
    AtualizarDisplay();
  }
  if (SELECT.fell())
  {
    FuncaoSELECT();
    AtualizarDisplay();
    LEDA.piscar(0.5, 1);
  }
}

void tratarMensagemRecebida(const char *topico, const String &mensagem)
{
  debugInfo("==================================");
  debugInfo("Mensagem recebida na aplicação");
  debugInfo("==================================");

  if (topico == nullptr)
  {
    debugErro("Tópico MQTT inválido");
    return;
  }

  debugInfo("Tópico: " + String(topico));
  debugInfo("Mensagem " + mensagem);

  debugInfo(String(strcmp(topico, TOPICO_COMANDO)));

  if (strcmp(topico, TOPICO_COMANDO) == 0)
  {
    tratarJsonComando(mensagem);
    return;
  }

  debugErro("Tópico näo tratado: " + String(topico));
}

void configurarLedRGB()
{
  ledRGB.begin();
  ledRGB.setBrightness(80); // IGUAL O ANALOG WRITE (0 a 255)
  ledRGB.clear();
  ledRGB.show();

  debugInfo("LED RGB configurado no GPIO " + String(PINO_LED_RGB));
}

void alterarCorLedRGB(int vermelho, int verde, int azul)
{
  // essa constrain faz a mesma coisa que um if(vermelho < 0) vermelho = 0 e if(vermelho > 255) vermelho = 255
  vermelho = constrain(vermelho, 0, 255);
  verde = constrain(verde, 0, 255);
  azul = constrain(azul, 0, 255);

  // passar qual é o endereço do led (tipo pensa na fita q tenha 5 leds ele vai falar q começa no primeiro que é o 0)
  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();

  debugInfo("Cor aplicada no LED RGB");
  debugInfo("R: " + String(vermelho));
  debugInfo("G: " + String(verde));
  debugInfo("B: " + String(azul));
}

void tratarJsonComando(const String &mensagem)
{
  JsonDocument doc;

  deserializeJson(doc, mensagem);

  DeserializationError erro = deserializeJson(doc, mensagem);

  if (erro)
  {
    debugErro("Erro ao interpretar o JSON.");
    debugErro(erro.c_str());
    return;
  }

  if (!doc["led"].is<JsonObject>())
  {
    if (!doc["led"]["r"].is<int>() || !doc["led"]["g"].is<int>() || !doc["led"]["b"].is<int>())
    {
      debugErro("JSON Inválido. Use led.r, led.g e led.b");
      return;
    }
  }
  else
  {
    int vermelho = doc["led"]["r"].as<int>();
    int verde = doc["led"]["g"].as<int>();
    int azul = doc["led"]["b"].as<int>();

    debugInfo("entrou no else para chamar alterarCorLedRGB");

    alterarCorLedRGB(vermelho, verde, azul);
  }
  if (doc["lampada"].is<bool>())
  {
    bool lampada = doc["lampada"].as<bool>();

    acenderLampada(lampada);
  }
  else
  {
    debugErro("Checar a conexão da lampamda com o LED");
    debugErro("Checar erro no JSON");
  }
}

void acenderLampada(bool lampadaParametro)
{
  debugInfo("entrou aqui");

  lampada = lampadaParametro;
  digitalWrite(PINO_LAMPADA, lampada);
  debugInfo("Estado da Lampada: " + String(lampada));
}

void FuncaoSELECT()
{
  String hora = coletarHora();

  if (hora == "")
  {
    debugErro("Falha ao obter hora");
    return;
  }

  JsonDocument doc;

  switch (coordenada)
  {
  case 0:
    LaisisTrabalhando = !LaisisTrabalhando;
    doc["Funcionario"] = "Lais";
    doc["Status"] = LaisisTrabalhando ? "Trabalhando" : "Finalizado";
    break;

  case 1:
    LeonardoisTrabalhando = !LeonardoisTrabalhando;
    doc["Funcionario"] = "Leonardo";
    doc["Status"] = LeonardoisTrabalhando ? "Trabalhando" : "Finalizado";
    break;

  case 2:
    LuigiisTrabalhando = !LuigiisTrabalhando;
    doc["Funcionario"] = "Luigi";
    doc["Status"] = LuigiisTrabalhando ? "Trabalhando" : "Finalizado";
    break;

  case 3:
    PedroisTrabalhando = !PedroisTrabalhando;
    doc["Funcionario"] = "Pedro";
    doc["Status"] = PedroisTrabalhando ? "Trabalhando" : "Finalizado";
    break;
  }

  doc["Hora"] = hora;

  String texto;
  serializeJson(doc, texto);
  publicarMensagem(TOPICO_COMANDO, texto.c_str());
}

void AtualizarLED()
{
  switch (coordenada)
  {
  case 0:
    if (LaisisTrabalhando)
    {
      alterarCorLedRGB(11, 222, 18);
    }
    else if (!LaisisTrabalhando)
    {
      alterarCorLedRGB(255, 0, 0);
    }
    break;
  case 1:
    if (LeonardoisTrabalhando)
    {
      alterarCorLedRGB(11, 222, 18);
    }
    else if (!LeonardoisTrabalhando)
    {
      alterarCorLedRGB(255, 0, 0);
    }
    break;
  case 2:
    if (LuigiisTrabalhando)
    {
      alterarCorLedRGB(11, 222, 18);
    }
    else if (!LuigiisTrabalhando)
    {
      alterarCorLedRGB(255, 0, 0);
    }
    break;
  case 3:
    if (PedroisTrabalhando)
    {
      alterarCorLedRGB(11, 222, 18);
    }
    else if (!PedroisTrabalhando)
    {
      alterarCorLedRGB(255, 0, 0);
    }
    break;
  }
}

void AtualizarDisplay()
{
  AtualizarLED();
  lcd.setCursor(1, 0);
  lcd.print("Lais");
  lcd.setCursor(1, 1);
  lcd.print("Leonardo");
  lcd.setCursor(1, 2);
  lcd.print("Luigi");
  lcd.setCursor(1, 3);
  lcd.print("Pedro");

  switch (coordenada)
  {
  case 0:
    lcd.setCursor(0, 0);
    lcd.print(">");
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print(" ");
    break;
  case 1:
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print(">");
    lcd.setCursor(0, 2);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print(" ");
    break;
  case 2:
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print(">");
    lcd.setCursor(0, 3);
    lcd.print(" ");
    break;
  case 3:
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print(">");
    break;
  default:
    lcd.setCursor(0, 0);
    lcd.print(">");
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print(" ");
    coordenada = 0;
    break;
  }
}

String coletarHora()
{
  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, URL_API))
  {
    debugInfo("");
    debugErro("Falha ao iniciar a conexão HTTP");
    return "";
  }

  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode > 0)
  {
    debugInfo("Codigo HTTP: " + String(httpCode));
    if (httpCode == HTTP_CODE_OK)
    {
      String resposta = http.getString();
      // USAR SE DER ERRO NA API
      // Serial.println("resposta bruta da API: ");
      // Serial.print(resposta);
      JsonDocument doc;
      DeserializationError erro = deserializeJson(doc, resposta);

      if (!erro)
      {
        if (doc["local_time"].is<JsonVariant>())
        {
          tempoLocal = doc["local_time"].as<String>();

          tempoTraduzido = tempoLocal.substring(11, 16);

          return tempoTraduzido;
        }
      }
    }
    else
    {
      debugErro("A API respondeu, mas com codigo de erro: " + String(httpCode)); // se for diferente de 200(Esse 200 foi obtido la no http.GET()) mostra isso
    }
  }
  else // se httpCode <= 0
  {
    debugErro("Erro na requisiçao HTTP: " + http.errorToString(httpCode));
  }
  http.end();
  return "";
}