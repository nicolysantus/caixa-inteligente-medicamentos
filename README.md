# Caixa Inteligente de Medicamentos

Projeto desenvolvido com ESP32 no Wokwi, utilizando display OLED, sensor PIR, LED, buzzer, botão de confirmação e comunicação MQTT.

## Arquivos do projeto

- `diagram.json` — montagem e conexões dos componentes no Wokwi.
- `sketch.ino` — código-fonte do ESP32.
- `libraries.txt` — bibliotecas externas necessárias.

## Bibliotecas

O Wokwi deve instalar as seguintes bibliotecas:

- Adafruit GFX Library
- Adafruit SSD1306
- PubSubClient

As bibliotecas `Wire`, `WiFi`, `time` e `string` são fornecidas pelo ambiente ESP32/Arduino.

## Componentes e pinos

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

## Configuração de rede

O código está configurado para a rede de simulação do Wokwi:

- SSID: `Wokwi-GUEST`
- Senha: vazia
- Broker MQTT: `broker.hivemq.com`
- Porta: `1883`

Para utilizar outra rede, altere `WIFI_SSID` e `WIFI_PASS` no início de `sketch.ino`.

## Como executar no Wokwi

1. Crie ou abra um projeto ESP32 no Wokwi.
2. Substitua o conteúdo de `diagram.json` pelo arquivo deste repositório.
3. Substitua o código do projeto pelo conteúdo de `sketch.ino`.
4. Adicione as bibliotecas indicadas em `libraries.txt`.
5. Inicie a simulação e abra o Serial Monitor em `115200` baud.

## Comandos pelo Monitor Serial

Envie os comandos com quebra de linha:

- `TESTE` — aciona manualmente o alerta.
- `STATUS` — exibe o estado atual do sistema.
- `A2 22:30` — programa o compartimento A2 para 22:30.

O comando de agendamento aceita compartimentos A1 a A10 e horários no formato de 24 horas (`HH:MM`).

## Funcionamento

No horário programado, o sistema ativa o LED e o buzzer e apresenta o lembrete no display OLED. O alerta pode ser confirmado pelo botão ou por detecção de presença do sensor PIR. O dispositivo publica os eventos no tópico MQTT configurado no código.

O firmware contém lógica para evitar que o mesmo agendamento seja disparado mais de uma vez na mesma data, utilizando a data obtida por NTP.

## Observações

- O sensor PIR detecta movimento/presença; ele não comprova que o medicamento foi ingerido.
- A confirmação pelo botão registra o pressionamento, não a ingestão do medicamento.
- O broker HiveMQ utilizado no código é público. Não publique dados pessoais ou informações sensíveis.
- Este projeto é um protótipo acadêmico e não deve ser utilizado como dispositivo médico real sem validação técnica apropriada.
