#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <string.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int PIN_ALARME_LED = 2;
const int PIN_BUZZER = 4;
const int PIN_BOTAO = 15;
const int PIN_PIR = 27;

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_PREFIX = "caixa-medicamentos/nicoly-a4";

const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

String mqttBase;
String mqttTopico;

unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaMQTT = 0;

int numeroCompartimento = 3;
String horarioLembrete = "23:00";
String ultimaDataDisparo = "";

bool alertaAtivo = false;
bool alarmeSilenciadoPorPresenca = false;
bool estadoPisca = false;
bool presencaDetectada = false;

unsigned long ultimoPisca = 0;
unsigned long ultimaAtualizacaoDisplay = 0;

String mensagemStatus1 = "";
String mensagemStatus2 = "";

unsigned long statusAte = 0;
bool statusConfirmacao = false;

const unsigned long DEBOUNCE_PIR_MS = 150;
const unsigned long DEBOUNCE_BOTAO_MS = 35;

bool pirLeituraBruta = LOW;
bool pirEstadoEstavel = LOW;
unsigned long pirMudancaBruta = 0;

bool botaoLeituraBruta = HIGH;
bool botaoEstadoEstavel = HIGH;
unsigned long botaoMudancaBruta = 0;
unsigned long inicioPressionamentoBotao = 0;

struct EstatisticaTempo {
  unsigned long long somaUs = 0;
  unsigned long amostras = 0;
};

EstatisticaTempo tempoLED;
EstatisticaTempo tempoBuzzer;
EstatisticaTempo tempoOLED;
EstatisticaTempo tempoPIRMQTT;
EstatisticaTempo tempoBotaoMQTT;

struct MensagemPendente {
  bool ativa = false;
  unsigned long sequencia = 0;
  unsigned long inicioUs = 0;
  String evento = "";
};

const int MAX_PENDENTES = 16;

MensagemPendente pendentes[MAX_PENDENTES];

unsigned long sequenciaMQTT = 0;

String bufferSerial = "";

String obterHoraReal() {
  time_t agora = time(nullptr);

  if (agora < 1700000000) {
    return "--:--";
  }

  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);

  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);

  return String(buffer);
}

String obterDataReal() {
  time_t agora = time(nullptr);

  if (agora < 1700000000) {
    return "";
  }

  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);

  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);

  return String(buffer);
}

void registrarTempo(
  const char* nome,
  unsigned long duracao,
  EstatisticaTempo &estatistica
) {
  if (estatistica.amostras >= 4) {
    return;
  }

  estatistica.somaUs += duracao;
  estatistica.amostras++;

  double media =
    (double)estatistica.somaUs / estatistica.amostras;

  Serial.println();
  Serial.println("========== DESEMPENHO ==========");

  Serial.printf(
    "[MEDICAO] %s | Amostra %lu/4\n",
    nome,
    estatistica.amostras
  );

  Serial.printf(
    "[TEMPO] %lu us | %.3f ms\n",
    duracao,
    duracao / 1000.0
  );

  Serial.printf(
    "[MEDIA] %.2f us | %.3f ms\n",
    media,
    media / 1000.0
  );

  if (estatistica.amostras == 4) {
    Serial.println("[RESULTADO] QUATRO MEDICOES CONCLUIDAS.");
  }

  Serial.println("===============================");
}

void textoCentralizado(String texto, int y, int tamanho) {
  display.setTextSize(tamanho);

  int16_t x1, y1;
  uint16_t largura, altura;

  display.getTextBounds(
    texto.c_str(),
    0,
    0,
    &x1,
    &y1,
    &largura,
    &altura
  );

  display.setCursor((SCREEN_WIDTH - largura) / 2, y);
  display.print(texto);
}

void atualizarDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  if (alertaAtivo) {
    textoCentralizado("Tomar medicamento", 0, 1);
    textoCentralizado("A" + String(numeroCompartimento), 15, 2);
    textoCentralizado(horarioLembrete, 34, 2);
    textoCentralizado("Pressione para", 49, 1);
    textoCentralizado("confirmar", 57, 1);
  }
  else if ((int32_t)(statusAte - millis()) > 0) {
    textoCentralizado(mensagemStatus1, 5, 1);
    textoCentralizado(mensagemStatus2, 23, 1);

    if (!statusConfirmacao) {
      textoCentralizado(horarioLembrete, 42, 2);
    }
  }
  else {
    textoCentralizado(obterHoraReal(), 21, 3);
  }

  display.display();
}

void mostrarStatus(
  String linha1,
  String linha2,
  bool confirmacao = false
) {
  mensagemStatus1 = linha1;
  mensagemStatus2 = linha2;
  statusConfirmacao = confirmacao;
  statusAte = millis() + 3000;

  atualizarDisplay();
}

void callbackMQTT(
  char* topic,
  byte* payload,
  unsigned int length
) {
  String mensagem = "";

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  int posicao = mensagem.indexOf("\"seq\":");

  if (posicao < 0) {
    return;
  }

  unsigned long seq =
    mensagem.substring(posicao + 6).toInt();

  int indice = seq % MAX_PENDENTES;

  if (!pendentes[indice].ativa) {
    return;
  }

  if (pendentes[indice].sequencia != seq) {
    return;
  }

  unsigned long tempoResposta =
    micros() - pendentes[indice].inicioUs;

  String evento = pendentes[indice].evento;

  pendentes[indice].ativa = false;

  Serial.println();
  Serial.println("[MQTT] RETORNO RECEBIDO DO BROKER");
  Serial.println("[MQTT] Evento: " + evento);

  Serial.printf(
    "[MQTT] Tempo de retorno: %lu us (%.3f ms)\n",
    tempoResposta,
    tempoResposta / 1000.0
  );

  if (evento == "presenca_detectada") {
    registrarTempo(
      "PIR_ate_retorno_MQTT",
      tempoResposta,
      tempoPIRMQTT
    );
  }
  else if (evento == "botao_pressionado") {
    registrarTempo(
      "BOTAO_ate_retorno_MQTT",
      tempoResposta,
      tempoBotaoMQTT
    );
  }
}

void publicarEvento(const char* evento) {
  if (!mqtt.connected()) {
    Serial.println("[MQTT] Evento nao enviado: broker desconectado.");
    return;
  }

  unsigned long seq = ++sequenciaMQTT;

  String payload =
    "{\"dispositivo\":\"caixa_medicamentos\","
    "\"seq\":" + String(seq) +
    ",\"evento\":\"" + String(evento) +
    "\",\"compartimento\":\"A" +
    String(numeroCompartimento) +
    "\",\"horario_agendado\":\"" +
    horarioLembrete +
    "\",\"hora_atual\":\"" +
    obterHoraReal() +
    "\",\"presenca_detectada\":" +
    String(presencaDetectada ? "true" : "false") +
    ",\"alarme_ativo\":" +
    String(alertaAtivo ? "true" : "false") +
    "}";

  int indice = seq % MAX_PENDENTES;

  bool medirRetorno =
    strcmp(evento, "presenca_detectada") == 0 ||
    strcmp(evento, "botao_pressionado") == 0;

  if (medirRetorno) {
    pendentes[indice].ativa = true;
    pendentes[indice].sequencia = seq;
    pendentes[indice].evento = String(evento);
    pendentes[indice].inicioUs = micros();
  }

  bool enviado = mqtt.publish(
    mqttTopico.c_str(),
    payload.c_str()
  );

  if (enviado) {
    Serial.println("[MQTT] Publicado: " + payload);
  }
  else {
    if (medirRetorno) {
      pendentes[indice].ativa = false;
    }

    Serial.println("[MQTT] Falha ao publicar.");
  }
}

void iniciarAlerta(unsigned long inicioComando = 0) {
  if (alertaAtivo) {
    return;
  }

  if (inicioComando == 0) {
    inicioComando = micros();
  }

  alertaAtivo = true;
  alarmeSilenciadoPorPresenca = false;
  estadoPisca = true;
  ultimoPisca = millis();

  digitalWrite(PIN_ALARME_LED, HIGH);

  registrarTempo(
    "COMANDO_ate_LED_LIGADO",
    micros() - inicioComando,
    tempoLED
  );

  tone(PIN_BUZZER, 1200);

  registrarTempo(
    "COMANDO_ate_BUZZER_LIGADO",
    micros() - inicioComando,
    tempoBuzzer
  );

  atualizarDisplay();

  registrarTempo(
    "COMANDO_ate_OLED_ATUALIZADO",
    micros() - inicioComando,
    tempoOLED
  );

  Serial.println();
  Serial.println("[ALARME] Lembrete ativado!");

  Serial.println(
    "[ALARME] Compartimento A" +
    String(numeroCompartimento)
  );

  Serial.println("[ALARME] Horario: " + horarioLembrete);

  publicarEvento("lembrete_acionado");
}

void silenciarAlarmePorPresenca() {
  if (!alertaAtivo || alarmeSilenciadoPorPresenca) {
    return;
  }

  alarmeSilenciadoPorPresenca = true;
  estadoPisca = false;

  digitalWrite(PIN_ALARME_LED, LOW);
  noTone(PIN_BUZZER);

  Serial.println();
  Serial.println("[PIR] Presenca detectada durante o alarme.");
  Serial.println("[ALARME] Som e LED silenciados.");
  Serial.println("[DOSE] Aguardando confirmacao pelo botao.");

  atualizarDisplay();
}

void confirmarAlerta(unsigned long inicioBotao = 0) {
  if (!alertaAtivo) {
    return;
  }

  alertaAtivo = false;
  alarmeSilenciadoPorPresenca = false;
  estadoPisca = false;

  digitalWrite(PIN_ALARME_LED, LOW);
  noTone(PIN_BUZZER);

  mostrarStatus(
    "Dose confirmada!",
    "Compartimento A" + String(numeroCompartimento),
    true
  );

  unsigned long respostaBotao = 0;

  if (inicioBotao > 0) {
    respostaBotao = micros() - inicioBotao;
  }

  Serial.println();
  Serial.println("[CONFIRMADO] Botao pressionado.");

  if (inicioBotao > 0) {
    Serial.printf(
      "[TEMPO] Botao ate confirmacao: %lu us (%.3f ms)\n",
      respostaBotao,
      respostaBotao / 1000.0
    );
  }

  publicarEvento("dose_confirmada_botao");
}

void verificarPresenca() {
  bool leitura = digitalRead(PIN_PIR);

  if (leitura != pirLeituraBruta) {
    pirLeituraBruta = leitura;
    pirMudancaBruta = millis();
  }

  if (
    pirLeituraBruta != pirEstadoEstavel &&
    millis() - pirMudancaBruta >= DEBOUNCE_PIR_MS
  ) {
    pirEstadoEstavel = pirLeituraBruta;
    presencaDetectada = pirEstadoEstavel;

    if (presencaDetectada) {
      Serial.println("[PIR] Movimento detectado.");

      publicarEvento("presenca_detectada");

      if (alertaAtivo) {
        silenciarAlarmePorPresenca();
      }
    }
    else {
      Serial.println("[PIR] Movimento encerrado.");

      publicarEvento("presenca_encerrada");
    }
  }
}

void verificarBotao() {
  bool leitura = digitalRead(PIN_BOTAO);

  if (leitura != botaoLeituraBruta) {
    botaoLeituraBruta = leitura;
    botaoMudancaBruta = millis();

    if (leitura == LOW) {
      inicioPressionamentoBotao = micros();
    }
  }

  if (
    botaoLeituraBruta != botaoEstadoEstavel &&
    millis() - botaoMudancaBruta >= DEBOUNCE_BOTAO_MS
  ) {
    botaoEstadoEstavel = botaoLeituraBruta;

    if (botaoEstadoEstavel == LOW) {
      Serial.println("[BOTAO] Pressionamento detectado.");

      publicarEvento("botao_pressionado");

      if (alertaAtivo) {
        confirmarAlerta(inicioPressionamentoBotao);
      }
      else {
        Serial.println("[BOTAO] Nenhum lembrete ativo.");
      }
    }
  }
}

void processarComandoSerial(String linha) {
  linha.trim();
  linha.toUpperCase();

  if (linha.length() == 0) {
    return;
  }

  if (linha == "TESTE") {
    unsigned long inicio = micros();
    iniciarAlerta(inicio);

    Serial.println("[OK] Teste manual executado.");
    return;
  }

  if (linha == "STATUS") {
    Serial.println();
    Serial.println("======= STATUS DO SISTEMA =======");

    Serial.println(
      "Compartimento: A" +
      String(numeroCompartimento)
    );

    Serial.println("Horario programado: " + horarioLembrete);
    Serial.println("Hora atual: " + obterHoraReal());
    Serial.println("Data atual: " + obterDataReal());

    Serial.println(
      "Alarme: " +
      String(alertaAtivo ? "ATIVO" : "INATIVO")
    );

    Serial.println(
      "Alarme sonoro: " +
      String(
        alertaAtivo && !alarmeSilenciadoPorPresenca
          ? "ATIVO"
          : "SILENCIADO"
      )
    );

    Serial.println(
      "Dose: " +
      String(alertaAtivo ? "PENDENTE" : "CONFIRMADA/SEM ALERTA")
    );

    Serial.println(
      "PIR: " +
      String(presencaDetectada ? "DETECTADO" : "LIVRE")
    );

    Serial.println(
      "Wi-Fi: " +
      String(WiFi.status() == WL_CONNECTED ? "CONECTADO" : "DESCONECTADO")
    );

    Serial.println(
      "MQTT: " +
      String(mqtt.connected() ? "CONECTADO" : "DESCONECTADO")
    );

    Serial.println("=================================");
    return;
  }

  int espaco = linha.indexOf(' ');

  if (espaco > 0) {
    String parteComp = linha.substring(0, espaco);
    String parteHora = linha.substring(espaco + 1);

    parteComp.trim();
    parteHora.trim();

    bool valido = true;

    if (
      !parteComp.startsWith("A") ||
      parteComp.length() < 2 ||
      parteComp.length() > 3 ||
      parteHora.length() != 5 ||
      parteHora.charAt(2) != ':'
    ) {
      valido = false;
    }

    for (int i = 1; valido && i < parteComp.length(); i++) {
      if (!isDigit(parteComp.charAt(i))) {
        valido = false;
      }
    }

    for (int i = 0; i < 5; i++) {
      if (
        i != 2 &&
        !isDigit(parteHora.charAt(i))
      ) {
        valido = false;
      }
    }

    int comp = parteComp.substring(1).toInt();
    int hh = parteHora.substring(0, 2).toInt();
    int mm = parteHora.substring(3, 5).toInt();

    if (
      valido &&
      comp >= 1 && comp <= 10 &&
      hh >= 0 && hh <= 23 &&
      mm >= 0 && mm <= 59
    ) {
      if (alertaAtivo) {
        alertaAtivo = false;
        alarmeSilenciadoPorPresenca = false;
        estadoPisca = false;
        digitalWrite(PIN_ALARME_LED, LOW);
        noTone(PIN_BUZZER);
      }

      numeroCompartimento = comp;
      horarioLembrete = parteHora;
      ultimaDataDisparo = "";

      Serial.println(
        "[SALVO] Compartimento A" +
        String(comp) +
        " | Horario: " +
        horarioLembrete
      );

      mostrarStatus(
        "Horario salvo!",
        "Compartimento A" + String(comp)
      );

      publicarEvento("agendamento_salvo");
      return;
    }
  }

  Serial.println("[ERRO] Comando invalido.");
  Serial.println("Exemplo: A2 22:30");
}

void conectarWiFiEHorario() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[WIFI] Conectando");

  int tentativas = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    tentativas < 30
  ) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[WIFI] Conectado!");

    configTime(
      GMT_OFFSET_SEC,
      DAYLIGHT_OFFSET_SEC,
      "pool.ntp.org",
      "time.nist.gov"
    );

    int tentativasNTP = 0;

    while (
      obterHoraReal() == "--:--" &&
      tentativasNTP < 15
    ) {
      delay(1000);
      tentativasNTP++;
    }

    if (obterHoraReal() != "--:--") {
      Serial.println(
        "[NTP] Horario sincronizado: " +
        obterHoraReal()
      );

      Serial.println(
        "[NTP] Data: " +
        obterDataReal()
      );
    }
    else {
      Serial.println("[NTP] Aguardando sincronizacao.");
    }
  }
  else {
    Serial.println("[WIFI] Falha na conexao.");
  }
}

void manterConexoes() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimaTentativaWiFi >= 10000) {
      ultimaTentativaWiFi = millis();

      Serial.println("[WIFI] Tentando reconectar...");
      WiFi.reconnect();
    }

    return;
  }

  if (
    !mqtt.connected() &&
    millis() - ultimaTentativaMQTT >= 5000
  ) {
    ultimaTentativaMQTT = millis();

    String clientId =
      "caixa-" +
      String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientId.c_str())) {
      Serial.println("[MQTT] Conectado ao HiveMQ.");

      bool inscrito = mqtt.subscribe(mqttTopico.c_str());

      Serial.println(
        inscrito
          ? "[MQTT] Inscricao no topico confirmada."
          : "[MQTT] Falha na inscricao."
      );

      publicarEvento("dispositivo_conectado");
    }
    else {
      Serial.println(
        "[MQTT] Falha na conexao: " +
        String(mqtt.state())
      );
    }
  }

  if (mqtt.connected()) {
    mqtt.loop();
  }
}

void verificarSerial() {
  while (Serial.available() > 0) {
    char caractere = Serial.read();

    if (caractere == '\n' || caractere == '\r') {
      if (bufferSerial.length() > 0) {
        processarComandoSerial(bufferSerial);
        bufferSerial = "";
      }
    }
    else if (bufferSerial.length() < 80) {
      bufferSerial += caractere;
    }
    else {
      bufferSerial = "";
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_ALARME_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
  pinMode(PIN_PIR, INPUT);

  digitalWrite(PIN_ALARME_LED, LOW);
  noTone(PIN_BUZZER);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERRO] Falha no display SSD1306.");

    for (;;) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  textoCentralizado("Iniciando...", 25, 1);
  display.display();

  pirLeituraBruta = digitalRead(PIN_PIR);
  pirEstadoEstavel = pirLeituraBruta;
  presencaDetectada = pirEstadoEstavel;

  botaoLeituraBruta = digitalRead(PIN_BOTAO);
  botaoEstadoEstavel = botaoLeituraBruta;

  mqttBase =
    String(MQTT_PREFIX) +
    "/" +
    String((uint32_t)ESP.getEfuseMac(), HEX);

  mqttTopico = mqttBase + "/eventos";

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(callbackMQTT);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(3);

  conectarWiFiEHorario();

  ultimaTentativaMQTT = millis() - 5000;

  manterConexoes();
  atualizarDisplay();

  Serial.println();
  Serial.println("==================================");
  Serial.println(" CAIXA INTELIGENTE DE MEDICAMENTOS");
  Serial.println("==================================");

  Serial.println("[SISTEMA] Inicializado.");
  Serial.println("[MQTT] Topico: " + mqttTopico);
  Serial.println("[SISTEMA] Medicoes de desempenho habilitadas.");
  Serial.println("[SISTEMA] Quatro amostras por indicador.");
  Serial.println("[SISTEMA] Presenca silencia o alarme.");
  Serial.println("[SISTEMA] Dose confirmada somente pelo botao.");
  Serial.println("[COMANDOS] TESTE | STATUS | A2 22:30");
}

void loop() {
  manterConexoes();

  verificarPresenca();
  verificarBotao();
  verificarSerial();

  String horaReal = obterHoraReal();
  String dataReal = obterDataReal();

  if (
    horaReal != "--:--" &&
    dataReal.length() > 0
  ) {
    if (
      horaReal == horarioLembrete &&
      dataReal != ultimaDataDisparo
    ) {
      ultimaDataDisparo = dataReal;

      unsigned long inicioComando = micros();

      iniciarAlerta(inicioComando);
    }
  }

  if (
    alertaAtivo &&
    !alarmeSilenciadoPorPresenca &&
    millis() - ultimoPisca >= 500
  ) {
    ultimoPisca = millis();
    estadoPisca = !estadoPisca;

    digitalWrite(
      PIN_ALARME_LED,
      estadoPisca ? HIGH : LOW
    );

    if (estadoPisca) {
      tone(PIN_BUZZER, 1200);
    }
    else {
      noTone(PIN_BUZZER);
    }
  }

  if (alarmeSilenciadoPorPresenca) {
    digitalWrite(PIN_ALARME_LED, LOW);
    noTone(PIN_BUZZER);
  }

  if (
    millis() - ultimaAtualizacaoDisplay >= 250
  ) {
    ultimaAtualizacaoDisplay = millis();
    atualizarDisplay();
  }

  delay(2);
}
