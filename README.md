# AquaSense IoT (ESP32)

Firmware do dispositivo: conta os pulsos do sensor de fluxo YF-S201 e envia as
leituras para a API (`POST /sensor-readings`).

## Antes de gravar
Edite `include/device_config.h`:
- `API_URL`: endereço da API acessível pelo ESP32 (em teste local, o IP do PC
  na rede, não `localhost`).
- `CALIBRATION_K`: fator de calibração medido do sensor (pulsos por litro).
- `DEVICE_KEY`: chave do dispositivo (RNF03). Depois de ler o identificador na
  serial, rode `npm run device-key -- <MAC>` em `aquasense-api` (precisa de
  `DEVICE_KEY_SECRET` no `.env`), copie a chave e grave-a passando
  `-DDEVICE_KEY=\"<chave>\"` em `build_flags` do `platformio.ini`
  (sem commitar a chave) ou na variável `PLATFORMIO_BUILD_FLAGS`. Sem a chave certa a API responde 401 e a
  leitura é descartada.

## Gravar e ver o identificador
```
pio run -t upload
pio device monitor
```
Na serial aparece `Identificador: AA:BB:CC:DD:EE:FF`. Use esse MAC para
gerar a chave do dispositivo (`npm run device-key -- <MAC>` em `aquasense-api`).
O mesmo identificador e o K aparecem na página de configuração do Wi-Fi
(`AquaSense-Setup`) e são digitados no aplicativo ao cadastrar o dispositivo.

## Ligação do sensor
| YF-S201 | ESP32 |
|---|---|
| Vermelho (5 V) | VIN / 5 V |
| Preto (GND) | GND |
| Amarelo (sinal) | GPIO 27, **através de divisor de tensão 5 V → 3,3 V** |

## Configuração do Wi-Fi (usuário)
1. Ligar o dispositivo. LED piscando devagar = portal aberto.
2. No celular, conectar em `AquaSense-Setup` (sem senha). A página abre sozinha
   ou em `192.168.4.1`. Ela mostra o identificador e o K.
3. **Configure WiFi**, escolher a rede de 2,4 GHz, informar a senha e salvar.
4. LED aceso fixo = conectado. Piscando rápido = sem Wi-Fi (as leituras ficam
   guardadas na flash e são reenviadas em ordem ao reconectar).
5. Para trocar de rede: segurar o botão BOOT por 5 s.

## Comportamento
- Uma leitura a cada 10 s (`SEND_INTERVAL_MS`), mesmo com zero pulsos.
- Sem conexão, as leituras vão para a fila na flash (`reading_queue`) e são
  reenviadas da mais antiga para a mais nova.
- Resposta 4xx da API (ex.: dispositivo ainda não cadastrado no app) descarta a
  leitura; erro de rede ou 5xx mantém na fila.
- Cada leitura leva `streamId` (sorteado a cada boot), `seq`, `durationMs` e
  `epoch` (fim da janela, UTC, via NTP). A API descarta reenvios repetidos
  (RNF04) e data a leitura pelo `epoch`, mesmo que ela tenha ficado horas na
  flash. Sem NTP sincronizado (ex.: ligou sem internet), `epoch` vai 0 e a API
  usa o horário de chegada.

