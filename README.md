
# Caixa Inteligente de Medicamentos

Projeto acadêmico desenvolvido com ESP32 e simulado no Wokwi. O sistema permite programar lembretes de medicamentos, emitir alertas visuais e sonoros, detectar presença por meio de um sensor PIR e confirmar a dose utilizando um botão físico. A comunicação dos eventos é realizada pelo protocolo MQTT, utilizando o broker HiveMQ.

## Arquivos do projeto

- `diagram.json` — diagrama eletrônico e conexões dos componentes no Wokwi.
- `sketch.ino` — código-fonte principal do ESP32.
- `libraries.txt` — bibliotecas externas utilizadas no projeto.

## Componentes utilizados

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

Display utilizado: OLED SSD1306, resolução de 128 × 64 pixels e endereço I2C `0x3C`.

## Bibliotecas

As bibliotecas externas utilizadas são:

- Adafruit GFX Library
- Adafruit SSD1306
- PubSubClient

As bibliotecas `Wire`, `WiFi`, `time` e `string` são fornecidas pelo ambiente ESP32/Arduino.

## Configuração de rede

O projeto utiliza a rede de simulação do Wokwi:

- SSID: `Wokwi-GUEST`
- Senha: vazia
- Protocolo de comunicação: MQTT
- Broker: `broker.hivemq.com`
- Porta: `1883`
- Transporte: TCP
- Autenticação: não utilizada.

O horário é sincronizado por meio do serviço NTP, utilizando os servidores `pool.ntp.org` e `time.nist.gov`, com configuração de fuso horário UTC-3.

## Comunicação MQTT

A comunicação entre o ESP32 e o broker HiveMQ é realizada utilizando a biblioteca PubSubClient.

O dispositivo publica os eventos do sistema em formato JSON, permitindo acompanhar as alterações de estado e as interações realizadas durante o funcionamento da caixa.

### Tópico MQTT

O tópico é construído automaticamente utilizando um identificador derivado do endereço MAC do ESP32.

Estrutura:

```text
caixa-medicamentos/nicoly-a4/<identificador-do-dispositivo>/eventos
```

O tópico completo é apresentado no Monitor Serial durante a inicialização.

O ESP32 também realiza a inscrição no próprio tópico de publicação para receber de volta as mensagens enviadas ao broker. Esse mecanismo é utilizado nas medições de retorno MQTT.

Para visualizar as mensagens por meio de um cliente MQTT externo, pode-se utilizar a inscrição:

```text
caixa-medicamentos/nicoly-a4/+/eventos
```

A utilização de clientes externos no computador ou celular é opcional, servindo apenas para acompanhar as mensagens durante os testes e a demonstração.

**Observação:** o broker HiveMQ utilizado é público e compartilhado. Não devem ser transmitidos dados pessoais, senhas ou informações médicas sensíveis. A conexão utilizada pelo ESP32 na porta 1883 não possui criptografia TLS.

## Como executar no Wokwi

1. Acesse o projeto no Wokwi.
2. Configure a placa ESP32.
3. Utilize o arquivo `diagram.json` para montar os componentes.
4. Insira o código disponível em `sketch.ino`.
5. Adicione as bibliotecas indicadas em `libraries.txt`.
6. Inicie a simulação.
7. Abra o Monitor Serial na velocidade de 115200 baud.
8. Aguarde a conexão Wi-Fi, a sincronização NTP e a conexão MQTT.

Após a inicialização, o Monitor Serial apresenta informações sobre as conexões e o tópico MQTT utilizado pelo dispositivo.

## Comandos pelo Monitor Serial

O sistema permite configurar e testar suas funcionalidades por meio dos seguintes comandos:

| Comando | Função |
|---|---|
| `TESTE` | Aciona manualmente o alerta para testar o funcionamento do sistema. |
| `STATUS` | Exibe informações sobre o compartimento, horário, presença, alarme e conexões. |
| `A2 22:30` | Programa o compartimento A2 para o horário 22:30. |

O comando de agendamento aceita compartimentos de A1 a A10 e horários no formato de 24 horas (`HH:MM`).

O agendamento é armazenado em memória durante a execução do programa e não é preservado após a reinicialização do ESP32.

## Funcionamento do sistema

### 1. Agendamento do medicamento

O usuário programa o compartimento e o horário desejado por meio do Monitor Serial.

O ESP32 utiliza o horário sincronizado pelo NTP para acompanhar o agendamento.

Quando o horário programado é alcançado, o sistema inicia automaticamente o lembrete, desde que ele ainda não tenha sido disparado naquela data.

### 2. Ativação do alerta

Quando chega o horário programado:

- O LED de alarme é acionado de forma intermitente.
- O buzzer emite um sinal sonoro intermitente.
- O display OLED apresenta a mensagem "Tomar medicamento".
- O compartimento e o horário programado são apresentados no display.
- O evento `lembrete_acionado` é publicado via MQTT.

O sistema utiliza a data obtida pelo NTP para evitar que o mesmo agendamento seja disparado repetidamente durante o mesmo dia.

O lembrete pode ser repetido diariamente.

### 3. Detecção de presença pelo sensor PIR

Quando o sensor PIR detecta movimento, o ESP32 identifica a presença e publica o evento `presenca_detectada` no broker MQTT.

Se o alerta estiver ativo, a detecção de presença provoca automaticamente o silenciamento do alarme:

- O LED é desligado.
- O buzzer é desligado.
- O display OLED continua apresentando o lembrete.
- A mensagem de confirmação permanece visível no display.

**A detecção de presença não confirma a dose e não encerra o lembrete.**

O sistema mantém o estado de alerta ativo, aguardando que o usuário pressione o botão físico para confirmar a dose.

Quando o sensor deixa de detectar movimento, o evento `presenca_encerrada` é publicado via MQTT.

O alarme permanece silenciado, mesmo após o encerramento da detecção de presença, até que a dose seja confirmada pelo botão.

### 4. Confirmação da dose pelo botão

A confirmação da dose é realizada exclusivamente pelo botão físico.

Quando o usuário pressiona o botão durante um lembrete ativo:

- O estado do alerta é encerrado.
- O LED permanece desligado.
- O buzzer permanece desligado.
- O display OLED apresenta a mensagem "Dose confirmada!".
- O compartimento correspondente é apresentado no display.
- O evento `dose_confirmada_botao` é publicado via MQTT.

O botão é, portanto, o único mecanismo responsável pela confirmação da dose.

A detecção de presença pelo PIR apenas silencia o alerta, não representando a confirmação de que o medicamento foi ingerido.

### 5. Controle de acionamentos

O firmware utiliza mecanismos de debounce para evitar leituras indevidas dos sensores e do botão.

Tempos configurados:

- Sensor PIR: 150 ms.
- Botão: 35 ms.

## Eventos publicados via MQTT

Durante a execução, o ESP32 publica os seguintes eventos:

| Evento | Descrição |
|---|---|
| `dispositivo_conectado` | Indica que o dispositivo estabeleceu conexão com o broker MQTT. |
| `lembrete_acionado` | Indica que um lembrete foi iniciado. |
| `presenca_detectada` | Indica que o sensor PIR detectou presença. |
| `presenca_encerrada` | Indica que o sensor PIR deixou de detectar presença. |
| `botao_pressionado` | Indica que o botão físico foi pressionado. |
| `dose_confirmada_botao` | Indica que um lembrete ativo foi confirmado pelo botão. |
| `agendamento_salvo` | Indica que um novo agendamento foi configurado. |

As mensagens são transmitidas em formato JSON e incluem informações como:

- Identificação do dispositivo.
- Número sequencial da mensagem.
- Tipo de evento.
- Compartimento programado.
- Horário agendado.
- Horário atual.
- Estado de presença.
- Estado do alarme.

## Medições de desempenho

O firmware possui rotinas para registrar até quatro amostras por indicador e calcular as médias dos tempos medidos.

As medições são apresentadas no Monitor Serial e contemplam:

- Tempo entre o início do comando e o acionamento do LED.
- Tempo entre o início do comando e o acionamento do buzzer.
- Tempo entre o início do comando e a atualização do display OLED.
- Tempo de retorno da mensagem MQTT relacionada à detecção de presença.
- Tempo de retorno da mensagem MQTT relacionada ao pressionamento do botão.

Para as medições de retorno MQTT, o ESP32 publica a mensagem e recebe uma cópia pelo próprio tópico, por estar inscrito nele.

Assim, o tempo registrado corresponde ao intervalo entre o início da publicação e o recebimento da mensagem de retorno pelo callback do ESP32.

Esse procedimento permite avaliar o tempo de retorno da comunicação pelo broker, não representando isoladamente o tempo de recebimento por um computador ou celular.

Os resultados devem ser organizados em tabelas, contendo quatro amostras e suas respectivas médias, além dos gráficos solicitados na atividade.

## Reconexão e manutenção da comunicação

O firmware verifica periodicamente o estado das conexões Wi-Fi e MQTT.

Quando ocorre uma desconexão Wi-Fi, o ESP32 realiza novas tentativas de conexão.

Quando a conexão Wi-Fi está estabelecida, mas a conexão MQTT está indisponível, o dispositivo tenta reconectar ao broker.

Após o restabelecimento da conexão MQTT, o ESP32 realiza novamente a inscrição no tópico de publicação.

Os eventos gerados enquanto o broker está desconectado não são armazenados em uma fila para publicação posterior.

## Observações e limitações

- O sensor PIR detecta movimento e presença, mas não comprova a ingestão do medicamento.
- A presença detectada pelo PIR provoca apenas o silenciamento do LED e do buzzer.
- A confirmação da dose depende exclusivamente do botão físico.
- O agendamento é armazenado em memória e é perdido após a reinicialização do ESP32.
- O broker HiveMQ utilizado é público e compartilhado.
- A comunicação MQTT do ESP32 utiliza a porta 1883, sem criptografia TLS.
- O projeto é um protótipo acadêmico e não deve ser utilizado como dispositivo médico real sem validação técnica apropriada.

## Objetivo do projeto

Desenvolver uma solução IoT utilizando ESP32 capaz de auxiliar no gerenciamento de horários de medicamentos por meio de alertas visuais e sonoros, detecção de presença, confirmação física da dose e comunicação MQTT.

O projeto demonstra a integração entre hardware, programação embarcada, sincronização de horário pela internet e comunicação entre dispositivos utilizando o protocolo MQTT.
