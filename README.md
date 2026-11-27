
# 💊 Caixa Inteligente de Medicamentos

<p>
  Aplicação desenvolvida como projeto acadêmico para a disciplina de
  <strong>Objetos Inteligentes e Conectados</strong>, no curso de
  <strong>Análise e Desenvolvimento de Sistemas</strong> da
  <a href="https://www.mackenzie.br" target="_blank"><strong>Universidade Presbiteriana Mackenzie</strong></a>.
  Tem como objetivo auxiliar no gerenciamento de horários de medicamentos por meio de alertas visuais e sonoros, detecção de presença e confirmação da dose.
</p>

**Desenvolvido por:** Nicoly Santos.

## Equipamentos utilizados

| Componente | Função |
|---|---|
| ESP32 | Unidade principal de processamento e comunicação Wi-Fi. |
| Display OLED SSD1306 | Exibição de horários, compartimentos e mensagens do sistema. |
| Sensor PIR | Detecção de presença e movimento. |
| LED | Sinalização visual dos lembretes. |
| Buzzer | Emissão dos alertas sonoros. |
| Botão físico | Confirmação da dose do medicamento. |

## Conexões dos componentes

| Componente | Pino ESP32 |
|---|---|
| LED de alarme | GPIO 2 |
| Buzzer | GPIO 4 |
| Botão de confirmação | GPIO 15 |
| Sensor PIR (OUT) | GPIO 27 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| OLED VCC | 3V3 |
| GND | GND |

**Display:** OLED SSD1306, resolução de 128 × 64 pixels e comunicação I2C no endereço `0x3C`.

## Tecnologias utilizadas

- **Arduino C++:** desenvolvimento da lógica embarcada.
- **ESP32:** processamento e conectividade Wi-Fi.
- **Wokwi:** simulação do circuito eletrônico e do firmware.
- **MQTT:** protocolo de comunicação entre o dispositivo e o broker.
- **HiveMQ:** broker responsável pelo recebimento das mensagens MQTT.
- **NTP:** sincronização automática do relógio pela internet.
- **I2C:** comunicação entre o ESP32 e o display OLED.

## Bibliotecas

O projeto utiliza as seguintes bibliotecas:

- Adafruit GFX Library
- Adafruit SSD1306
- PubSubClient

## Arquitetura do sistema

O ESP32 é responsável pelo controle dos componentes eletrônicos, gerenciamento dos lembretes e processamento das informações recebidas pelos sensores e pelo botão.

A sincronização do horário é realizada por meio de servidores NTP. A conexão com a internet ocorre pela rede Wi-Fi disponibilizada pelo Wokwi.

Os eventos gerados durante o funcionamento são enviados ao broker HiveMQ utilizando o protocolo MQTT.

### Configuração de rede

- SSID: `Wokwi-GUEST`
- Senha: vazia
- Broker MQTT: `broker.hivemq.com`
- Porta MQTT: `1883`
- Transporte: TCP
- Biblioteca MQTT: PubSubClient

### Configuração MQTT

A comunicação MQTT utiliza um tópico construído automaticamente pelo firmware, com um identificador derivado do endereço MAC do ESP32.

Estrutura do tópico:

```text
caixa-medicamentos/nicoly-a4/<identificador-do-dispositivo>/eventos
```

O tópico completo é apresentado no Monitor Serial durante a inicialização.

As mensagens são transmitidas em formato JSON e contêm informações sobre o dispositivo, evento, compartimento, horário e estado do sistema.

O ESP32 também realiza a inscrição no próprio tópico de publicação para receber as mensagens de retorno do broker, mecanismo utilizado nas medições de desempenho da comunicação MQTT.

## Funcionamento do sistema

### 1. Programação do medicamento

O usuário configura o compartimento e o horário desejado por meio do Monitor Serial.

Exemplo de comando:

```text
A2 22:30
```

O ESP32 registra o compartimento A2 e o horário 22:30, passando a acompanhar o horário programado por meio do relógio sincronizado via NTP.

O sistema aceita compartimentos de A1 a A10 e horários no formato de 24 horas (`HH:MM`).

### 2. Ativação do lembrete

Quando o horário programado é alcançado, o sistema inicia automaticamente o alerta.

Nesse momento:

- O LED começa a piscar.
- O buzzer emite um sinal sonoro intermitente.
- O display OLED apresenta a mensagem "Tomar medicamento".
- O compartimento e o horário programado são exibidos.
- O evento `lembrete_acionado` é publicado via MQTT.

O sistema utiliza a data obtida pelo NTP para evitar que o mesmo agendamento seja disparado repetidamente durante o mesmo dia, permitindo a repetição diária dos lembretes.

### 3. Detecção de presença

O sensor PIR monitora a presença de movimento próximo à caixa.

Quando detecta presença durante um lembrete ativo:

- O LED é desligado.
- O buzzer é desligado.
- O display continua apresentando a solicitação de confirmação da dose.
- O evento `presenca_detectada` é publicado via MQTT.

A detecção de presença provoca apenas o silenciamento do alerta visual e sonoro. O lembrete permanece ativo, aguardando a confirmação do usuário.

Quando o sensor deixa de detectar movimento, o sistema publica o evento `presenca_encerrada`.

O alarme permanece silenciado até que o usuário pressione o botão de confirmação.

### 4. Confirmação da dose

A confirmação é realizada exclusivamente pelo botão físico.

Quando pressionado durante um lembrete ativo:

- O alerta é encerrado.
- O LED permanece desligado.
- O buzzer permanece desligado.
- O display apresenta a mensagem "Dose confirmada!".
- O compartimento correspondente é apresentado.
- O evento `dose_confirmada_botao` é publicado via MQTT.

Portanto, o sensor PIR identifica a presença do usuário e silencia o alarme, enquanto o botão é responsável pela confirmação da dose.

### 5. Repetição diária

O ESP32 utiliza a data sincronizada pelo NTP para controlar o disparo dos lembretes.

Após a configuração, o dispositivo verifica continuamente o horário programado e permite a repetição diária do lembrete.

## Comunicação MQTT

O protocolo MQTT é utilizado para transmitir os eventos gerados pelo dispositivo ao broker HiveMQ.

Os principais eventos publicados são:

| Evento | Descrição |
|---|---|
| `dispositivo_conectado` | Indica a conexão do ESP32 ao broker MQTT. |
| `lembrete_acionado` | Indica a ativação de um lembrete. |
| `presenca_detectada` | Indica a detecção de presença pelo sensor PIR. |
| `presenca_encerrada` | Indica o encerramento da detecção de presença. |
| `botao_pressionado` | Indica o acionamento do botão físico. |
| `dose_confirmada_botao` | Indica a confirmação da dose pelo botão. |
| `agendamento_salvo` | Indica o registro de um novo agendamento. |

As mensagens são estruturadas em JSON e incluem:

- Identificação do dispositivo.
- Número sequencial do evento.
- Tipo de evento.
- Compartimento programado.
- Horário agendado.
- Horário atual.
- Estado de presença.
- Estado do alarme.

## Comandos disponíveis

O sistema permite executar comandos pelo Monitor Serial:

| Comando | Funcionamento |
|---|---|
| `TESTE` | Aciona manualmente o alerta para testar o sistema. |
| `STATUS` | Apresenta o estado atual do dispositivo. |
| `A2 22:30` | Programa o compartimento A2 para 22:30. |

Os comandos permitem testar as funcionalidades, consultar o estado do sistema e configurar os lembretes.

## Medições de desempenho

O firmware possui rotinas para registrar até quatro amostras por indicador e calcular automaticamente as médias dos tempos medidos.

São avaliados:

- Tempo entre o comando e o acionamento do LED.
- Tempo entre o comando e o acionamento do buzzer.
- Tempo entre o comando e a atualização do display OLED.
- Tempo de retorno MQTT após a detecção de presença.
- Tempo de retorno MQTT após o pressionamento do botão.

As medições são apresentadas no Monitor Serial, em microssegundos e milissegundos.

Os resultados permitem analisar o tempo de resposta dos componentes e da comunicação MQTT, sendo utilizados na elaboração das tabelas e dos gráficos de desempenho do projeto.

## Como executar no Wokwi

1. Acesse a plataforma Wokwi.
2. Crie ou abra um projeto ESP32.
3. Utilize o arquivo `diagram.json` para montar os componentes.
4. Insira o código-fonte disponível em `sketch.ino`.
5. Adicione as bibliotecas indicadas em `libraries.txt`.
6. Inicie a simulação.
7. Abra o Monitor Serial na velocidade de 115200 baud.
8. Aguarde a conexão Wi-Fi, a sincronização NTP e a conexão MQTT.
9. Utilize os comandos disponíveis para testar o agendamento e as funcionalidades do sistema.

## Estrutura do repositório

```text
caixa-inteligente-medicamentos/
│
├── diagram.json
├── sketch.ino
├── libraries.txt
└── README.md
```

## Objetivo do projeto

Demonstrar a aplicação de conceitos de Internet das Coisas (IoT), sistemas embarcados, sensores, atuadores, sincronização de horário e comunicação MQTT no desenvolvimento de uma caixa inteligente para gerenciamento de lembretes de medicamentos.
