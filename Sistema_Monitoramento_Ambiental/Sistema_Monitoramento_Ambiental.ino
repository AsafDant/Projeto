// ============================================================
// INCLUSÃO DE BIBLIOTECAS
// ============================================================
#include <WiFi.h>          // Biblioteca para funcionalidades Wi-Fi do ESP32
#include <HTTPClient.h>    // Biblioteca para fazer requisições HTTP (para enviar dados ao Firebase)
#include <DHT.h>           // Biblioteca para o sensor DHT22 (temperatura e umidade)
#include <WebServer.h>     // Biblioteca para criar um servidor web no ESP32
#include <esp_sleep.h>     // Biblioteca para funcionalidades de deep sleep (modo de economia de energia)
#include <NTPClient.h>     // Biblioteca para obter hora da internet via protocolo NTP
#include <WiFiUdp.h>       // Biblioteca UDP necessária para o funcionamento do NTPClient
#include <TimeLib.h>       // Biblioteca para manipulação avançada de data e hora
#include <ArduinoJson.h>   // Biblioteca para trabalhar com formato JSON (usada para formatar dados)

// ============================================================
// CONFIGURAÇÃO DO SENSOR DHT22
// ============================================================
// Cria uma instância do sensor DHT22 conectado ao pino 21 do ESP32
// DHT22 é o tipo de sensor (poderia ser DHT11 também)
DHT dht(21, DHT22);

// ============================================================
// CONFIGURAÇÕES NTP (DATA E HORA)
// ============================================================
WiFiUDP ntpUDP;  // Cria uma instância UDP para comunicação com servidores NTP
// Cria um cliente NTP que se conecta ao pool.ntp.org, com fuso horário UTC-4 (horário de Manaus)
// O último parâmetro (60000) é o intervalo de atualização em milissegundos
NTPClient timeClient(ntpUDP, "pool.ntp.org", -4 * 3600, 60000);
bool timeConfigured = false;  // Flag que indica se o tempo foi sincronizado com sucesso
unsigned long lastTimeSync = 0;  // Armazena o último momento em que o tempo foi sincronizado
const unsigned long TIME_SYNC_INTERVAL = 3600000;  // Intervalo de 1 hora para sincronizar novamente

// Variáveis para armazenar data e hora formatadas
String currentDateTime = "";  // Armazena data e hora completas (ex: "01/01/2023 14:30:45")
String currentDate = "";      // Armazena apenas a data (ex: "01/01/2023")
String currentTime = "";      // Armazena apenas a hora (ex: "14:30:45")
// ============================================================

// ============================================================
// CONFIGURAÇÃO DO SENSOR DE CHAMAS
// ============================================================
int chama = 19;  // Define o pino 19 do ESP32 para conectar o sensor de chamas
int leituraChama = 0;  // Variável para armazenar o valor lido do sensor (0 ou 1)
bool alertaChama = false;  // Flag que indica se há alerta de chama ativo
bool ultimoEstadoChama = false;  // Armazena o último estado do sensor para detectar mudanças

// ============================================================
// CONFIGURAÇÃO DO SENSOR DE GÁS MQ-2
// ============================================================
int gas = 18;  // Define o pino 18 do ESP32 para conectar o sensor de gás MQ-2
int leituraGas = 0;  // Variável para armazenar o valor lido do sensor (0 ou 1)
bool alertaGas = false;  // Flag que indica se há alerta de gás ativo
bool ultimoEstadoGas = false;  // Armazena o último estado do sensor para detectar mudanças

// ============================================================
// CREDENCIAIS DE REDE WI-FI
// ============================================================
const char* ssid = "Nome";  // Nome da rede Wi-Fi (SSID) a ser conectada
const char* password = "Senha";  // Senha da rede Wi-Fi

// ============================================================
// CONFIGURAÇÃO DO FIREBASE REALTIME DATABASE
// ============================================================
// URL do banco de dados Firebase onde os dados serão enviados
const char* firebaseHost = "link de acesso ao firebase";

// ============================================================
// CONFIGURAÇÃO DO SERVIDOR WEB
// ============================================================
// Cria uma instância do servidor web na porta 80 (porta padrão HTTP)
WebServer server(80);

// ============================================================
// CONSTANTES PARA OS LIMITES DO SENSOR DHT22
// ============================================================
// Estas constantes definem os valores limites para temperatura e umidade
// Quando os valores medidos ultrapassarem esses limites, será gerado um alerta
const float TEMP_ALTA = 70.0;    // Temperatura máxima em graus Celsius - valor pode ser ajustado
const float TEMP_BAIXA = 15.0;   // Temperatura mínima em graus Celsius - valor pode ser ajustado
const float UMID_ALTA = 90.0;    // Umidade máxima em porcentagem - valor pode ser ajustado
const float UMID_BAIXA = 20.0;   // Umidade mínima em porcentagem - valor pode ser ajustado
// ============================================================

// ============================================================
// VARIÁVEIS PARA CONTROLE DO FIREBASE
// ============================================================
unsigned long lastFirebaseUpdate = 0;  // Armazena o último momento em que os dados foram enviados ao Firebase
const unsigned long FIREBASE_INTERVAL = 30000;  // Intervalo de 30 segundos para envio de dados normais
bool firebaseStatus = false;  // Flag que indica se o último envio ao Firebase foi bem-sucedido

// ============================================================
// VARIÁVEIS PARA CONTROLE DE ALERTAS
// ============================================================
bool alertaTemperatura = false;  // Flag que indica alerta de temperatura fora dos limites
bool alertaUmidade = false;  // Flag que indica alerta de umidade fora dos limites
bool ultimoAlertaTemperatura = false;  // Último estado do alerta de temperatura (para detectar mudanças)
bool ultimoAlertaUmidade = false;  // Último estado do alerta de umidade (para detectar mudanças)
String mensagemAlerta = "";  // String que armazena a mensagem descritiva do alerta

// ============================================================
// CONTADORES PARA ORGANIZAÇÃO DOS DADOS NO FIREBASE
// ============================================================
int contadorDados = 0;  // Contador para dados normais enviados periodicamente
int contadorAlertas = 0;  // Contador para alertas enviados
int contadorAlertasImmediate = 0;  // Contador para alertas imediatos (envio instantâneo)

// ============================================================
// VARIÁVEIS PARA CONTROLE DO DEEP SLEEP
// ============================================================
// Define os tempos para o ciclo de operação do ESP32:
const unsigned long TEMPO_ATIVO = 300000;      // 5 minutos ativo (300000 milissegundos)
const unsigned long TEMPO_DEEP_SLEEP = 900000; // 15 minutos em deep sleep (900000 milissegundos)
// RTC_DATA_ATTR faz com que a variável seja mantida na memória RTC durante o deep sleep
RTC_DATA_ATTR int bootCount = 0;  // Contador de inicializações que persiste durante deep sleep
unsigned long inicioCiclo = 0;  // Marca o início do ciclo ativo (quando o ESP32 acorda)

// ============================================================
// DECLARAÇÕES DE FUNÇÕES (PROTOTYPES)
// ============================================================
// Estas são as assinaturas das funções que serão implementadas posteriormente
// É uma boa prática declarar todas as funções no início do código
void handleRoot();  // Função para tratar requisições à página principal
void handleTemperatura();  // Função para retornar o valor da temperatura via HTTP
void handleUmidade();  // Função para retornar o valor da umidade via HTTP
void handleChama();  // Função para retornar o status do sensor de chamas via HTTP
void handleGas();  // Função para retornar o status do sensor de gás via HTTP
void handleServidor();  // Função para retornar o status do servidor/Firebase
void handleAlertas();  // Função para retornar informações sobre alertas ativos
void handleDateTime();  // Função para retornar data e hora atual
void sendToFirebase();  // Função para enviar dados normais ao Firebase
void sendAlertToFirebaseImmediately();  // Função para enviar alertas imediatos ao Firebase
void verificarAlertas(float temperatura, float umidade);  // Função que verifica se os valores estão fora dos limites definidos
void organizarFirebase();  // Função para criar estrutura inicial no Firebase
bool checkSensorChanges();  // Função para verificar mudanças nos sensores
void entrarDeepSleep();  // Função para colocar o ESP32 em modo deep sleep
void configurarWakeup();  // Função para configurar o wakeup do deep sleep

// ============================================================
// FUNÇÕES DE DATA/HORA - DECLARAÇÕES
// ============================================================
void initTimeClient();  // Função para inicializar o cliente NTP
void updateDateTime();  // Função para atualizar as variáveis de data e hora
String getFormattedDateTime();  // Função para retornar data e hora formatadas
String getFormattedDate();  // Função para retornar apenas a data formatada
String getFormattedTime();  // Função para retornar apenas a hora formatada
String getTimestamp();  // Função para retornar timestamp no formato ISO 8601
// ============================================================

// ============================================================
// PÁGINA WEB COMPLETA EM HTML
// ============================================================
// A constante htmlPage contém todo o código HTML, CSS e JavaScript
// que será servido quando alguém acessar o ESP32 via navegador
// R"rawliteral(...) é uma string literal raw que permite escrever
// múltiplas linhas sem precisar escapar caracteres especiais
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Monitor DHT22 + Sensor de Chamas + Sensor de Gás</title>
        <style>
            body {
                background-color: #f5f5f5;
                margin: 0;
                padding: 20px;
                font-family: Arial, Helvetica, sans-serif;
            }
            .container {
                max-width: 400px;
                margin: 0 auto;
            }
            div{
                scale: 0.95;
            }
            
            /* Estilo para o painel de data/hora */
            #DateTimeBoard {
                height: 80px;
                width: 375px;
                background-color: #2196F3;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: white;
                border-radius: 2em;
                margin-bottom: 20px;
                display: flex;
                flex-direction: column;
                justify-content: center;
                align-items: center;
            }
            #DateTimeVal {
                font-size: 20px;
                margin-bottom: 5px;
            }
            #DateVal {
                font-size: 16px;
                opacity: 0.9;
            }
            
            #TempBoard{
                height: 225px;
                width: 375px;
                background-color: white;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: rgb(175, 175, 175);
                border-radius: 2em;
                transition: all 0.3s ease;
            }
            #TempBoard.alerta {
                background-color: #fff0f0;
                border: 2px solid #ff4444;
            }
            #TempType{
                position: relative;
                top: 15%;
            }
            #TempVal{
                position: relative;
                top: 12%;
                font-size: 25px;
            }
            #HumBoard{
                height: 225px;
                width: 375px;
                background-color: white;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: rgb(175, 175, 175);
                border-radius: 2em;
                transition: all 0.3s ease;
            }
            #HumBoard.alerta {
                background-color: #fff0f0;
                border: 2px solid #ff4444;
            }
            #HumType{
                position: relative;
                top: 15%;
            }
            #HumVal{
                position: relative;
                top: 12%;
                font-size: 25px;
            }
            #ChamaBoard{
                height: 225px;
                width: 375px;
                background-color: white;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: rgb(175, 175, 175);
                border-radius: 2em;
                transition: all 0.3s ease;
            }
            #ChamaBoard.alerta {
                background-color: #ffebee;
                border: 2px solid #f44336;
                animation: pulse 1s infinite;
            }
            #GasBoard{
                height: 225px;
                width: 375px;
                background-color: white;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: rgb(175, 175, 175);
                border-radius: 2em;
                transition: all 0.3s ease;
            }
            #GasBoard.alerta {
                background-color: #fff3e0;
                border: 2px solid #ff9800;
                animation: pulse 1s infinite;
            }
            @keyframes pulse {
                0% { background-color: #ffebee; }
                50% { background-color: #ffcdd2; }
                100% { background-color: #ffebee; }
            }
            @keyframes pulse-orange {
                0% { background-color: #fff3e0; }
                50% { background-color: #ffe0b2; }
                100% { background-color: #fff3e0; }
            }
            #ChamaType, #GasType {
                position: relative;
                top: 15%;
            }
            #ChamaVal, #GasVal {
                position: relative;
                top: 12%;
                font-size: 25px;
            }
            #DbBoard{
                height: 280px;
                width: 375px;
                background-color: white;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                color: rgb(175, 175, 175);
                border-radius: 2em;
            }
            #ServType{
                position: relative;
                top: 10%;
            }
            #FireBaseLogo{
                width: 60px;
                height: 55px;
                margin: 10px 0;
            }
            #ErroVal{
                position: relative;
                top: 5%;
                font-size: 25px;
                color: rgb(175, 175, 175);
            }
            #OkVal{
                position: relative;
                top: 5%;
                font-size: 25px;
                color: rgb(175, 175, 175);
            }
            .firebase-button {
                background-color: #ffa000;
                color: white;
                border: none;
                padding: 12px 24px;
                border-radius: 25px;
                font-size: 16px;
                font-weight: bold;
                cursor: pointer;
                margin: 15px 0;
                filter: drop-shadow(4px 2px 3px rgb(207, 207, 207));
                transition: all 0.3s ease;
                text-decoration: none;
                display: inline-block;
            }
            .firebase-button:hover {
                background-color: #ff8f00;
                transform: scale(1.05);
            }
            #alertaPanel {
                height: auto;
                min-height: 80px;
                width: 375px;
                background-color: #fff8e1;
                filter: drop-shadow(8px 3px 5px rgb(207, 207, 207));
                margin-left: auto;
                margin-right: auto;
                text-align: center;
                font-family: Arial, Helvetica, sans-serif;
                font-weight: bold;
                border-radius: 2em;
                margin-bottom: 20px;
                padding: 10px;
                display: none;
                border: 2px solid #ffa000;
            }
            #alertaPanel.visible {
                display: block;
            }
            #alertaPanel.emergencia {
                background-color: #ffebee;
                border: 2px solid #f44336;
                animation: pulse 1s infinite;
            }
            #alertaPanel.emergencia-gas {
                background-color: #fff3e0;
                border: 2px solid #ff9800;
                animation: pulse-orange 1s infinite;
            }
            #alertaMensagem {
                color: #e65100;
                font-size: 16px;
                margin: 10px 0;
            }
            .emergencia-text {
                color: #d32f2f !important;
                font-weight: bold;
            }
            .emergencia-gas-text {
                color: #ef6c00 !important;
                font-weight: bold;
            }
            .limites {
                text-align: center;
                margin: 10px 0;
                font-size: 12px;
                color: #666;
            }
            .sleep-info {
                text-align: center;
                margin: 10px 0;
                font-size: 12px;
                color: #2196F3;
                background-color: #E3F2FD;
                padding: 5px;
                border-radius: 10px;
            }
            .config-info {
                text-align: center;
                margin: 10px 0;
                font-size: 11px;
                color: #4CAF50;
                background-color: #E8F5E9;
                padding: 5px;
                border-radius: 10px;
                border: 1px dashed #4CAF50;
            }
            .time-sync-info {
                text-align: center;
                margin: 5px 0;
                font-size: 11px;
                color: #9C27B0;
                background-color: #F3E5F5;
                padding: 3px;
                border-radius: 8px;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <!-- Painel de data e hora -->
            <div id="DateTimeBoard">
                <div id="DateTimeVal">00:00:00</div>
                <div id="DateVal">01/01/2025</div>
            </div>
            
            <!-- Informações sobre o modo deep sleep -->
            <div class="sleep-info">
                ⏰ Modo Deep Sleep Ativo: 5min ON / 15min OFF
            </div>
            
            <!-- Informações sobre sincronização de tempo -->
            <div class="time-sync-info">
                🕐 Sincronizado com NTP: <span id="TimeSyncStatus">Conectando...</span>
            </div>
            
            <!-- Informações sobre configurações dos limites -->
            <div class="config-info">
                ⚙️ Configurações DHT22: Temp: 15-70°C | Umidade: 20-90%
            </div>
            
            <!-- Painel de alertas (inicialmente oculto) -->
            <div id="alertaPanel">
                <h3>⚠ ALERTA</h3>
                <div id="alertaMensagem"></div>
            </div>
            
            <!-- Informações sobre limites dos sensores -->
            <div class="limites">
                Limites: Temp: 15-70°C | Umidade: 20-90% | Chama/Gás: Monitoramento Ativo
            </div>
            
            <!-- Painel de temperatura -->
            <div id="TempBoard">
                <h2 id="TempType">TEMPERATURA (ºC)</h2><br><br>
                <label id="TempVal"></label>
            </div><br>
            
            <!-- Painel de umidade -->
            <div id="HumBoard">
                <h2 id="HumType">UMIDADE (%)</h2><br><br>
                <label id="HumVal"></label>
            </div>
            
            <!-- Painel de sensor de chamas -->
            <div id="ChamaBoard">
                <h2 id="ChamaType">SENSOR DE CHAMAS</h2><br><br>
                <label id="ChamaVal"></label>
            </div>
            
            <!-- Painel de sensor de gás -->
            <div id="GasBoard">
                <h2 id="GasType">SENSOR DE GÁS</h2><br><br>
                <label id="GasVal"></label>
            </div><br>
            
            <!-- Painel de status do servidor e Firebase -->
            <div id="DbBoard">
                <h2 id="ServType">STATUS DO SERVIDOR</h2>
                <img id="FireBaseLogo" src="https://www.gstatic.com/devrel-devsite/prod/v5ab6fd0ad9c02b131b4d387b5751ac2c3616478c6dd65b5e931f0805efa1009c/firebase/images/touchicon-180.png"><br>
                <label id="ErroVal">ERRO</label><label id="OkVal"> OK</label>
                <br>
                
                <!-- Botão para acessar o Firebase -->
                <a href="Link de acesso ao Firebase" target="_blank" class="firebase-button">
                    🔍 VER BANCO DE DADOS
                </a>
            </div>
        </div>
        
        <script>
        // JavaScript para atualização dinâmica da página web
        // A cada intervalo de tempo, faz requisições AJAX para obter os dados mais recentes
        
        // Atualiza data e hora a cada segundo
        setInterval(function() {
            const reqDateTime = new XMLHttpRequest();
            reqDateTime.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    const data = JSON.parse(reqDateTime.responseText);
                    document.getElementById("DateTimeVal").innerHTML = data.time;
                    document.getElementById("DateVal").innerHTML = data.date;
                    document.getElementById("TimeSyncStatus").innerHTML = data.synced ? "Sincronizado" : "Não sincronizado";
                }
            }
            reqDateTime.open('GET','/datetime',true);
            reqDateTime.send();
        }, 1000);

        // Atualiza temperatura a cada 500ms
        setInterval(function() {
            const reqTemp = new XMLHttpRequest();
            reqTemp.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    document.getElementById("TempVal").innerHTML = reqTemp.responseText + "ºC";
                }
            }
            reqTemp.open('GET','/temperatura',true);
            reqTemp.send();
        }, 500);

        // Atualiza umidade a cada 500ms
        setInterval(function() {
            const reqHum = new XMLHttpRequest();
            reqHum.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    document.getElementById("HumVal").innerHTML = reqHum.responseText + "%";
                }
            }
            reqHum.open('GET','/umidade',true);
            reqHum.send();
        }, 500);

        // Atualiza status do sensor de chamas a cada 500ms
        setInterval(function() {
            const reqChama = new XMLHttpRequest();
            reqChama.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    document.getElementById("ChamaVal").innerHTML = reqChama.responseText;
                }
            }
            reqChama.open('GET','/chama',true);
            reqChama.send();
        }, 500);

        // Atualiza status do sensor de gás a cada 500ms
        setInterval(function() {
            const reqGas = new XMLHttpRequest();
            reqGas.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    document.getElementById("GasVal").innerHTML = reqGas.responseText;
                }
            }
            reqGas.open('GET','/gas',true);
            reqGas.send();
        }, 500);

        // Atualiza status do servidor/Firebase a cada 500ms
        setInterval(function() {
            const reqServ = new XMLHttpRequest();
            reqServ.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    if(reqServ.responseText == "o"){
                        document.getElementById("OkVal").style.color = "rgb(17, 245, 17)";
                        document.getElementById("ErroVal").style.color = "rgb(175, 175, 175)";
                    }
                    if(reqServ.responseText == "e"){
                        document.getElementById("OkVal").style.color = "rgb(175, 175, 175)";
                        document.getElementById("ErroVal").style.color = "rgb(235, 43, 43)";
                    }
                }
            }
            reqServ.open('GET','/servidor',true);
            reqServ.send();
        }, 500);

        // Atualiza alertas a cada segundo
        setInterval(function() {
            const reqAlerta = new XMLHttpRequest();
            reqAlerta.onreadystatechange = function(){
                if(this.readyState == 4 && this.status == 200){
                    const alertas = JSON.parse(reqAlerta.responseText);
                    
                    const alertaPanel = document.getElementById('alertaPanel');
                    const alertaMensagem = document.getElementById('alertaMensagem');
                    
                    // Se houver alerta ativo, mostra o painel
                    if (alertas.alertaAtivo) {
                        alertaPanel.classList.add('visible');
                        alertaMensagem.innerHTML = alertas.mensagem;
                        
                        // Adiciona classes CSS diferentes baseadas no tipo de alerta
                        if (alertas.alertaChama) {
                            alertaPanel.classList.add('emergencia');
                            alertaPanel.classList.remove('emergencia-gas');
                            alertaMensagem.classList.add('emergencia-text');
                            alertaMensagem.classList.remove('emergencia-gas-text');
                        } else if (alertas.alertaGas) {
                            alertaPanel.classList.add('emergencia-gas');
                            alertaPanel.classList.remove('emergencia');
                            alertaMensagem.classList.add('emergencia-gas-text');
                            alertaMensagem.classList.remove('emergencia-text');
                        } else {
                            alertaPanel.classList.remove('emergencia');
                            alertaPanel.classList.remove('emergencia-gas');
                            alertaMensagem.classList.remove('emergencia-text');
                            alertaMensagem.classList.remove('emergencia-gas-text');
                        }
                    } else {
                        // Se não houver alerta, esconde o painel
                        alertaPanel.classList.remove('visible');
                        alertaPanel.classList.remove('emergencia');
                        alertaPanel.classList.remove('emergencia-gas');
                        alertaMensagem.classList.remove('emergencia-text');
                        alertaMensagem.classList.remove('emergencia-gas-text');
                    }
                    
                    // Atualiza as cores dos painéis baseadas nos alertas
                    const tempBoard = document.getElementById('TempBoard');
                    const humBoard = document.getElementById('HumBoard');
                    const chamaBoard = document.getElementById('ChamaBoard');
                    const gasBoard = document.getElementById('GasBoard');
                    
                    if (alertas.alertaTemperatura) {
                        tempBoard.classList.add('alerta');
                    } else {
                        tempBoard.classList.remove('alerta');
                    }
                    
                    if (alertas.alertaUmidade) {
                        humBoard.classList.add('alerta');
                    } else {
                        humBoard.classList.remove('alerta');
                    }
                    
                    if (alertas.alertaChama) {
                        chamaBoard.classList.add('alerta');
                    } else {
                        chamaBoard.classList.remove('alerta');
                    }
                    
                    if (alertas.alertaGas) {
                        gasBoard.classList.add('alerta');
                    } else {
                        gasBoard.classList.remove('alerta');
                    }
                }
            }
            reqAlerta.open('GET','/alertas',true);
            reqAlerta.send();
        }, 1000);
        </script>
    </body>
</html>
)rawliteral";

// ============================================================
// IMPLEMENTAÇÃO DAS FUNÇÕES DE DATA E HORA
// ============================================================

// Função para inicializar o cliente NTP e sincronizar a hora
void initTimeClient() {
    Serial.println("Inicializando cliente NTP...");
    timeClient.begin();  // Inicia o cliente NTP
    timeClient.setTimeOffset(-4 * 3600); // Define o fuso horário para UTC-4 (Horário de Manaus)
    
    // Tenta sincronizar por até 10 segundos (20 tentativas com 500ms de intervalo)
    int maxAttempts = 20;
    for (int i = 0; i < maxAttempts; i++) {
        if (timeClient.update()) {  // Tenta atualizar a hora
            timeConfigured = true;
            updateDateTime();  // Atualiza as variáveis de data e hora
            lastTimeSync = millis();  // Registra o momento da sincronização
            Serial.println("✅ Hora sincronizada via NTP: " + currentDateTime);
            return;
        }
        Serial.print(".");  // Mostra progresso durante a tentativa
        delay(500);
    }
    Serial.println("\n❌ Falha ao sincronizar com NTP");
}

// Função para atualizar as variáveis de data e hora com os valores do NTP
void updateDateTime() {
    if (timeClient.update()) {  // Se conseguiu atualizar a hora
        timeConfigured = true;
        
        // Obtém a hora formatada (HH:MM:SS)
        currentTime = timeClient.getFormattedTime();
        
        // Extrai dia, mês e ano do timestamp epoch
        time_t epochTime = timeClient.getEpochTime();  // Obtém o tempo em segundos desde 01/01/1970
        struct tm *ptm = gmtime((time_t *)&epochTime);  // Converte para estrutura tm
        
        int day = ptm->tm_mday;  // Dia do mês (1-31)
        int month = ptm->tm_mon + 1;  // Mês (0-11, então soma 1)
        int year = ptm->tm_year + 1900;  // Ano (anos desde 1900)
        
        // Formata a data como DD/MM/YYYY com zeros à esquerda quando necessário
        currentDate = String(day < 10 ? "0" : "") + String(day) + "/" +
                     String(month < 10 ? "0" : "") + String(month) + "/" +
                     String(year);
        
        // Combina data e hora em uma única string
        currentDateTime = currentDate + " " + currentTime;
    }
}

// Retorna a data e hora completas formatadas
String getFormattedDateTime() {
    return currentDateTime;
}

// Retorna apenas a data formatada
String getFormattedDate() {
    return currentDate;
}

// Retorna apenas a hora formatada
String getFormattedTime() {
    return currentTime;
}

// Retorna um timestamp no formato ISO 8601 simplificado (YYYY-MM-DDTHH:MM:SS)
String getTimestamp() {
    if (timeConfigured) {  // Só retorna timestamp válido se a hora estiver sincronizada
        time_t epochTime = timeClient.getEpochTime();
        struct tm *ptm = gmtime((time_t *)&epochTime);
        
        char timestamp[25];  // Buffer para armazenar a string formatada
        // Formata: YYYY-MM-DDTHH:MM:SS
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
                 ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        return String(timestamp);
    }
    // Se não estiver sincronizado, retorna o tempo de execução em milissegundos
    return String(millis());
}

// ============================================================

// Função para configurar o temporizador de wakeup do deep sleep
void configurarWakeup() {
    // Configura o temporizador interno para acordar o ESP32 após TEMPO_DEEP_SLEEP
    // O tempo é em microssegundos, por isso multiplica por 1000
    esp_sleep_enable_timer_wakeup(TEMPO_DEEP_SLEEP * 1000);
    Serial.println("Configurado para acordar em " + String(TEMPO_DEEP_SLEEP / 60000) + " minutos");
}

// Função para colocar o ESP32 em modo deep sleep
void entrarDeepSleep() {
    Serial.println("\n⏰ Preparando para entrar em Deep Sleep...");
    Serial.println("Tempo ativo: " + String(TEMPO_ATIVO / 60000) + " minutos");
    Serial.println("Tempo de deep sleep: " + String(TEMPO_DEEP_SLEEP / 60000) + " minutos");
    Serial.println("Próximo ciclo em " + String(TEMPO_DEEP_SLEEP / 60000) + " minutos");
    Serial.println("===========================================");
    
    // Salva última data/hora conhecida antes de dormir (para logging)
    if (timeConfigured) {
        Serial.println("Última hora conhecida: " + currentDateTime);
    }
    
    // Desconectar WiFi para economizar energia (WiFi consome muita energia)
    WiFi.disconnect(true);
    delay(100);  // Pequeno delay para garantir que a desconexão foi processada
    
    // Configurar o wakeup (temporizador para acordar)
    configurarWakeup();
    
    // Entrar em deep sleep - o ESP32 para completamente até o temporizador acordá-lo
    esp_deep_sleep_start();
    // O código nunca continua a partir deste ponto
}

// ============================================================
// FUNÇÃO SETUP - EXECUTADA UMA VEZ NA INICIALIZAÇÃO
// ============================================================
void setup() {
    // Inicializa a comunicação serial para debug (115200 baud rate)
    Serial.begin(115200);
    delay(100);  // Pequeno delay para estabilização
    
    // Incrementa o contador de boot (quantas vezes o ESP32 foi iniciado)
    bootCount++;
    Serial.println("\n===========================================");
    Serial.println("🔄 Inicialização #" + String(bootCount));
    Serial.println("===========================================");
    
    // Marca o início do ciclo ativo (quando o ESP32 acordou)
    inicioCiclo = millis();
    
    // Inicializar sensor DHT22
    dht.begin();
    
    // Configuração dos pinos dos sensores digitais como entrada
    pinMode(chama, INPUT);
    pinMode(gas, INPUT);
    
    // Conectar à rede Wi-Fi
    Serial.print("Conectando ao Wi-Fi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);  // Inicia a conexão Wi-Fi

    // Configura um timeout de 15 segundos para tentativa de conexão Wi-Fi
    unsigned long inicioWiFi = millis();
    const unsigned long timeoutWiFi = 15000;
    
    // Aguarda a conexão ou timeout
    while (WiFi.status() != WL_CONNECTED && millis() - inicioWiFi < timeoutWiFi) {
        delay(500);
        Serial.print(".");  // Mostra pontos enquanto tenta conectar
    }
    
    // Verifica se conseguiu conectar
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi-Fi conectado!");
        Serial.print("Endereço IP: ");
        Serial.println(WiFi.localIP());  // Mostra o IP atribuído ao ESP32
        
        // Inicializa cliente NTP após conectar ao Wi-Fi (precisa de internet)
        initTimeClient();
    } else {
        Serial.println("\n❌ Falha na conexão Wi-Fi!");
    }

    // Configuração das rotas do servidor web
    // Associa cada URL a uma função handler
    server.on("/", handleRoot);  // Página principal
    server.on("/temperatura", handleTemperatura);
    server.on("/umidade", handleUmidade);
    server.on("/chama", handleChama);
    server.on("/gas", handleGas);
    server.on("/servidor", handleServidor);
    server.on("/alertas", handleAlertas);
    server.on("/datetime", handleDateTime); // Nova rota para data/hora

    // Inicia o servidor web
    server.begin();
    Serial.println("Servidor web iniciado!");
    
    // Organizar Firebase na inicialização (apenas se WiFi conectado)
    if (WiFi.status() == WL_CONNECTED) {
        organizarFirebase();  // Cria estrutura inicial no Firebase
    }
    
    // Mostrar informações do ciclo na serial
    Serial.println("=== SISTEMA DE MONITORAMENTO COM DEEP SLEEP ===");
    Serial.println("Ciclo ativo: 5 minutos (" + String(TEMPO_ATIVO / 60000) + " minutos)");
    Serial.println("Deep sleep: 15 minutos (" + String(TEMPO_DEEP_SLEEP / 60000) + " minutos)");
    
    // Informações sobre data/hora
    if (timeConfigured) {
        Serial.println("=== SISTEMA DE DATA/HORA ===");
        Serial.println("Servidor NTP: pool.ntp.org");
        Serial.println("Fuso horário: UTC-4 (Manaus)");
        Serial.println("Hora atual: " + currentDateTime);
    } else {
        Serial.println("⚠ Sistema de data/hora não sincronizado");
    }
    
    // ============================================================
    // MOSTRAR INFORMAÇÕES SOBRE OS LIMITES DO DHT22
    // ============================================================
    Serial.println("=== CONFIGURAÇÕES DOS LIMITES DO DHT22 ===");
    Serial.println("FUNÇÃO 'verificarAlertas()' usa as constantes:");
    Serial.println("  TEMP_BAIXA: " + String(TEMP_BAIXA) + "°C (ajuste na linha 36)");
    Serial.println("  TEMP_ALTA: " + String(TEMP_ALTA) + "°C (ajuste na linha 35)");
    Serial.println("  UMID_BAIXA: " + String(UMID_BAIXA) + "% (ajuste na linha 38)");
    Serial.println("  UMID_ALTA: " + String(UMID_ALTA) + "% (ajuste na linha 37)");
    Serial.println("===========================================");
    
    Serial.println("Sensor de Chamas: Monitoramento Ativo");
    Serial.println("Sensor de Gás MQ-2: Monitoramento Ativo");
    Serial.println("URL do Firebase: " + String(firebaseHost));
    Serial.println("ALERTAS: Salvo IMEDIATAMENTE no Firebase quando detectados");
    Serial.println("===========================================");
}

// ============================================================
// FUNÇÃO LOOP - EXECUTADA REPETIDAMENTE
// ============================================================
void loop() {
    // Atualiza data/hora periodicamente (a cada TIME_SYNC_INTERVAL)
    if (WiFi.status() == WL_CONNECTED && millis() - lastTimeSync >= TIME_SYNC_INTERVAL) {
        if (timeClient.update()) {
            updateDateTime();
            lastTimeSync = millis();
            Serial.println("🔄 Hora sincronizada: " + currentDateTime);
        }
    }
    
    // Verifica se o tempo ativo acabou
    // Se já passou TEMPO_ATIVO desde que o ESP32 acordou, entra em deep sleep
    if (millis() - inicioCiclo >= TEMPO_ATIVO) {
        // Envia dados finais antes de dormir
        Serial.println("\n⏰ Tempo ativo finalizado. Preparando para deep sleep...");
        
        // Faz uma última leitura e envio antes de dormir
        if (WiFi.status() == WL_CONNECTED) {
            sendToFirebase();  // Envia dados normais
            
            // Envia uma mensagem de status de deep sleep com timestamp
            HTTPClient http;
            String urlSleep = String(firebaseHost) + "info/ultimo_ciclo.json";
            String jsonSleep = "{";
            jsonSleep += "\"boot_count\": " + String(bootCount) + ",";
            jsonSleep += "\"tempo_ativo_min\": " + String(TEMPO_ATIVO / 60000) + ",";
            jsonSleep += "\"tempo_sleep_min\": " + String(TEMPO_DEEP_SLEEP / 60000) + ",";
            jsonSleep += "\"status\": \"entrando_em_deep_sleep\",";
            jsonSleep += "\"timestamp_millis\": " + String(millis()) + ",";
            jsonSleep += "\"timestamp_iso\": \"" + getTimestamp() + "\",";
            jsonSleep += "\"hora_local\": \"" + currentDateTime + "\",";
            jsonSleep += "\"data\": \"" + currentDate + "\",";
            jsonSleep += "\"hora\": \"" + currentTime + "\"";
            jsonSleep += "}";
            
            http.begin(urlSleep);
            http.addHeader("Content-Type", "application/json");
            http.PUT(jsonSleep);  // Envia via método PUT
            http.end();
            
            Serial.println("✅ Status de deep sleep enviado ao Firebase");
        }
        
        // Aguarda um pouco antes de entrar em deep sleep
        delay(1000);
        
        // Entra em deep sleep
        entrarDeepSleep();
        return; // Nunca chegará aqui, mas por segurança
    }
    
    // Handle dos clientes do servidor web (apenas durante o tempo ativo)
    // Esta função verifica se há requisições HTTP pendentes e as processa
    server.handleClient();

    // Leitura dos sensores digitais
    // Os sensores retornam 0 quando detectam algo e 1 quando não detectam
    leituraChama = digitalRead(chama);
    leituraGas = digitalRead(gas);
    
    // Atualiza data/hora a cada segundo (para manter as variáveis atualizadas)
    static unsigned long lastDateTimeUpdate = 0;
    if (millis() - lastDateTimeUpdate >= 1000) {
        updateDateTime();
        lastDateTimeUpdate = millis();
    }
    
    // Verificação de mudanças nos sensores
    // Esta função retorna true se algum sensor mudou de estado
    bool mudancaDetectada = checkSensorChanges();
    
    // Se houve mudança de estado em algum sensor, salva IMEDIATAMENTE no Firebase
    if (mudancaDetectada && WiFi.status() == WL_CONNECTED) {
        Serial.println("⚡ MUDANÇA DETECTADA - Enviando para Firebase IMEDIATAMENTE");
        sendAlertToFirebaseImmediately();  // Envio prioritário para alertas
    }

    // Envio periódico para Firebase a cada 30 segundos (dados normais)
    // Este envio acontece mesmo sem mudanças, para manter dados históricos
    unsigned long currentTimeMillis = millis();
    if (currentTimeMillis - lastFirebaseUpdate >= FIREBASE_INTERVAL && WiFi.status() == WL_CONNECTED) {
        sendToFirebase();
        lastFirebaseUpdate = currentTimeMillis;
    }
    
    // Mostra tempo restante a cada 30 segundos no monitor serial
    static unsigned long ultimoDisplayTempo = 0;
    if (currentTimeMillis - ultimoDisplayTempo >= 30000) {
        // Calcula quanto tempo resta no ciclo ativo
        unsigned long tempoRestante = TEMPO_ATIVO - (currentTimeMillis - inicioCiclo);
        Serial.println("⏰ Tempo restante ativo: " + String(tempoRestante / 60000) + " minutos e " + 
                       String((tempoRestante % 60000) / 1000) + " segundos");
        Serial.println("🕐 Hora atual: " + currentTime + " | Data: " + currentDate);
        ultimoDisplayTempo = currentTimeMillis;
    }
}

// ============================================================
// FUNÇÃO PARA VERIFICAR MUDANÇAS NOS SENSORES
// ============================================================
bool checkSensorChanges() {
    bool mudanca = false;  // Inicializa como false, será true se houver mudança
  
    // Verifica mudanças no sensor de chamas
    // Nota: leituraChama == 0 significa que detectou fogo
    bool novoEstadoChama = (leituraChama == 0);
    if (novoEstadoChama != ultimoEstadoChama) {
        alertaChama = novoEstadoChama;  // Atualiza flag de alerta
        ultimoEstadoChama = novoEstadoChama;  // Atualiza último estado
        mudanca = true;  // Marca que houve mudança
        
        if (alertaChama) {
            Serial.println("🔥 ALERTA CRÍTICO: FOGO DETECTADO!");
            Serial.println("📅 Data/Hora: " + currentDateTime);
        } else {
            Serial.println("✅ Fogo extinto - Sensor de chamas normalizado");
        }
    }

    // Verifica mudanças no sensor de gás
    // Nota: leituraGas == 0 significa que detectou gás
    bool novoEstadoGas = (leituraGas == 0);
    if (novoEstadoGas != ultimoEstadoGas) {
        alertaGas = novoEstadoGas;  // Atualiza flag de alerta
        ultimoEstadoGas = novoEstadoGas;  // Atualiza último estado
        mudanca = true;  // Marca que houve mudança
        
        if (alertaGas) {
            Serial.println("💨 ALERTA CRÍTICO: GÁS/FUMAÇA DETECTADO!");
            Serial.println("📅 Data/Hora: " + currentDateTime);
        } else {
            Serial.println("✅ Gás dissipado - Sensor de gás normalizado");
        }
    }

    // Verifica mudanças nos sensores DHT22 (temperatura e umidade)
    float temperatura = dht.readTemperature();  // Lê temperatura em °C
    float umidade = dht.readHumidity();  // Lê umidade em %
    
    // Verifica se as leituras são válidas (não são NaN - Not a Number)
    if (!isnan(temperatura) && !isnan(umidade)) {
        // ============================================================
        // AQUI CHAMA A FUNÇÃO QUE USA OS LIMITES DO DHT22
        // Esta função verifica se os valores estão fora dos limites definidos
        // ============================================================
        verificarAlertas(temperatura, umidade);
        
        // Verifica se houve mudança no estado dos alertas DHT
        // Compara com os valores anteriores armazenados
        if (alertaTemperatura != ultimoAlertaTemperatura || alertaUmidade != ultimoAlertaUmidade) {
            mudanca = true;  // Marca que houve mudança
            ultimoAlertaTemperatura = alertaTemperatura;  // Atualiza último estado
            ultimoAlertaUmidade = alertaUmidade;  // Atualiza último estado
            
            if (alertaTemperatura || alertaUmidade) {
                Serial.println("🌡️ ALERTA: Condições ambientais fora dos limites!");
                Serial.println("📅 Data/Hora: " + currentDateTime);
            } else {
                Serial.println("✅ Condições ambientais normalizadas");
            }
        }
    }

    return mudanca;  // Retorna true se houve mudança, false caso contrário
}

// ============================================================
// FUNÇÕES HANDLER PARA O SERVIDOR WEB
// ============================================================

// Handler para a página principal - retorna o HTML completo
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

// Handler para data/hora - retorna JSON com data e hora
void handleDateTime() {
    // Cria uma resposta JSON com todas as informações de data/hora
    String jsonResponse = "{";
    jsonResponse += "\"date\": \"" + currentDate + "\",";
    jsonResponse += "\"time\": \"" + currentTime + "\",";
    jsonResponse += "\"datetime\": \"" + currentDateTime + "\",";
    jsonResponse += "\"timestamp\": \"" + getTimestamp() + "\",";
    jsonResponse += "\"synced\": " + String(timeConfigured ? "true" : "false");
    jsonResponse += "}";
    
    server.send(200, "application/json", jsonResponse);
}

// Handler para temperatura - retorna apenas o valor da temperatura
void handleTemperatura() {
    float temperatura = dht.readTemperature();
    if (!isnan(temperatura)) {  // Se a leitura for válida
        server.send(200, "text/plain", String(temperatura));
    } else {
        server.send(200, "text/plain", "Erro");  // Se houver erro na leitura
    }
}

// Handler para umidade - retorna apenas o valor da umidade
void handleUmidade() {
    float umidade = dht.readHumidity();
    if (!isnan(umidade)) {  // Se a leitura for válida
        server.send(200, "text/plain", String(umidade));
    } else {
        server.send(200, "text/plain", "Erro");  // Se houver erro na leitura
    }
}

// Handler para sensor de chamas - retorna mensagem apropriada
void handleChama() {
    if (leituraChama == 0) {  // 0 = detectou fogo
        server.send(200, "text/plain", "🔥 FOGO DETECTADO!");
    } else {  // 1 = não detectou fogo
        server.send(200, "text/plain", "✅ Nenhum fogo");
    }
}

// Handler para sensor de gás - retorna mensagem apropriada
void handleGas() {
    if (leituraGas == 0) {  // 0 = detectou gás
        server.send(200, "text/plain", "💨 GÁS DETECTADO!");
    } else {  // 1 = não detectou gás
        server.send(200, "text/plain", "✅ Ar limpo");
    }
}

// Handler para status do servidor - retorna "o" para OK ou "e" para erro
void handleServidor() {
    if (firebaseStatus && WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/plain", "o");  // OK
    } else {
        server.send(200, "text/plain", "e");  // Erro
    }
}

// Handler para alertas - retorna JSON com status de todos os alertas
void handleAlertas() {
    // Lê os valores atuais dos sensores DHT
    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();
    
    // ============================================================
    // AQUI CHAMA A FUNÇÃO QUE USA OS LIMITES DO DHT22
    // Atualiza as flags de alerta baseadas nos limites definidos
    // ============================================================
    verificarAlertas(temperatura, umidade);
    
    // Cria JSON com status de todos os alertas
    String jsonResponse = "{";
    jsonResponse += "\"alertaAtivo\":" + String((alertaTemperatura || alertaUmidade || alertaChama || alertaGas) ? "true" : "false") + ",";
    jsonResponse += "\"alertaTemperatura\":" + String(alertaTemperatura ? "true" : "false") + ",";
    jsonResponse += "\"alertaUmidade\":" + String(alertaUmidade ? "true" : "false") + ",";
    jsonResponse += "\"alertaChama\":" + String(alertaChama ? "true" : "false") + ",";
    jsonResponse += "\"alertaGas\":" + String(alertaGas ? "true" : "false") + ",";
    jsonResponse += "\"datetime\":\"" + currentDateTime + "\",";
    jsonResponse += "\"mensagem\":\"" + mensagemAlerta + "\"";
    jsonResponse += "}";
    
    server.send(200, "application/json", jsonResponse);
}

// ============================================================
// FUNÇÃO PRINCIPAL PARA VERIFICAÇÃO DE ALERTAS DHT22
// Esta função 'verificarAlertas()' usa as constantes definidas
// nas linhas 35-38 para verificar se os valores estão fora dos limites
// ============================================================
void verificarAlertas(float temperatura, float umidade) {
    // Reinicia os alertas DHT (exceto chama e gas que são controlados separadamente)
    alertaTemperatura = false;
    alertaUmidade = false;
    mensagemAlerta = "";  // Limpa a mensagem anterior
  
    // Verifica temperatura usando TEMP_BAIXA e TEMP_ALTA
    if (!isnan(temperatura)) {  // Se a leitura for válida
        if (temperatura >= TEMP_ALTA) {  // Usa TEMP_ALTA (linha 35)
            alertaTemperatura = true;
            mensagemAlerta += "Temperatura ALTA: " + String(temperatura) + "°C (Limite: " + String(TEMP_ALTA) + "°C). ";
        } else if (temperatura <= TEMP_BAIXA) {  // Usa TEMP_BAIXA (linha 36)
            alertaTemperatura = true;
            mensagemAlerta += "Temperatura BAIXA: " + String(temperatura) + "°C (Limite: " + String(TEMP_BAIXA) + "°C). ";
        }
    }
  
    // Verifica umidade usando UMID_BAIXA e UMID_ALTA
    if (!isnan(umidade)) {  // Se a leitura for válida
        if (umidade >= UMID_ALTA) {  // Usa UMID_ALTA (linha 37)
            alertaUmidade = true;
            mensagemAlerta += "Umidade ALTA: " + String(umidade) + "% (Limite: " + String(UMID_ALTA) + "%). ";
        } else if (umidade <= UMID_BAIXA) {  // Usa UMID_BAIXA (linha 38)
            alertaUmidade = true;
            mensagemAlerta += "Umidade BAIXA: " + String(umidade) + "% (Limite: " + String(UMID_BAIXA) + "%). ";
        }
    }
  
    // Verifica chama (alerta crítico) - já está definido pela função checkSensorChanges()
    if (alertaChama) {
        mensagemAlerta += "🔥 ALERTA CRÍTICO: FOGO DETECTADO! ";
    }
  
    // Verifica gás (alerta crítico) - já está definido pela função checkSensorChanges()
    if (alertaGas) {
        mensagemAlerta += "💨 ALERTA CRÍTICO: GÁS/FUMAÇA DETECTADO! ";
    }
  
    // Adiciona data/hora à mensagem de alerta se disponível e se há algum alerta
    if (timeConfigured && (alertaTemperatura || alertaUmidade || alertaChama || alertaGas)) {
        mensagemAlerta += " [" + currentDateTime + "]";
    }
}

// ============================================================
// FUNÇÃO PARA ORGANIZAR ESTRUTURA INICIAL DO FIREBASE
// ============================================================
void organizarFirebase() {
    // Função para organizar a estrutura inicial do Firebase
    // Cria pastas e arquivos iniciais para organização dos dados
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;  // Cria uma instância do cliente HTTP
        
        // Define a URL raiz do Firebase
        String url = String(firebaseHost) + ".json";
        
        // Cria uma estrutura JSON complexa com várias seções
        String estruturaInicial = "{";
        estruturaInicial += "\"dados\": {";  // Seção para dados normais
        estruturaInicial += "\"1\": {\"status\": \"sistema_iniciado\", \"timestamp\": \"" + String(millis()) + "\", \"datetime\": \"" + currentDateTime + "\"}";
        estruturaInicial += "},";
        estruturaInicial += "\"alertas\": {";  // Seção para alertas
        estruturaInicial += "\"1\": {\"status\": \"sistema_monitorando\", \"timestamp\": \"" + String(millis()) + "\", \"datetime\": \"" + currentDateTime + "\"}";
        estruturaInicial += "},";
        estruturaInicial += "\"alertas_immediate\": {";  // Seção para alertas imediatos
        estruturaInicial += "\"1\": {\"status\": \"sistema_alerta_imediato_ativado\", \"timestamp\": \"" + String(millis()) + "\", \"datetime\": \"" + currentDateTime + "\"}";
        estruturaInicial += "},";
        estruturaInicial += "\"chamas\": {";  // Seção específica para dados de chamas
        estruturaInicial += "\"1\": {\"status\": \"monitoramento_incendio_ativado\", \"timestamp\": \"" + String(millis()) + "\", \"datetime\": \"" + currentDateTime + "\"}";
        estruturaInicial += "},";
        estruturaInicial += "\"gas\": {";  // Seção específica para dados de gás
        estruturaInicial += "\"1\": {\"status\": \"monitoramento_gas_ativado\", \"timestamp\": \"" + String(millis()) + "\", \"datetime\": \"" + currentDateTime + "\"}";
        estruturaInicial += "},";
        estruturaInicial += "\"info\": {";  // Seção para informações do sistema
        estruturaInicial += "\"limites\": \"Temp: " + String(TEMP_BAIXA) + "-" + String(TEMP_ALTA) + "°C, Umidade: " + String(UMID_BAIXA) + "-" + String(UMID_ALTA) + "%\",";
        estruturaInicial += "\"deep_sleep\": \"ativo\",";
        estruturaInicial += "\"ciclo_ativo_min\": " + String(TEMPO_ATIVO / 60000) + ",";
        estruturaInicial += "\"ciclo_sleep_min\": " + String(TEMPO_DEEP_SLEEP / 60000) + ",";
        estruturaInicial += "\"boot_count\": " + String(bootCount) + ",";
        estruturaInicial += "\"hora_inicio\": \"" + currentDateTime + "\",";
        estruturaInicial += "\"fuso_horario\": \"UTC-4 (Manaus)\",";
        estruturaInicial += "\"ntp_server\": \"pool.ntp.org\"";
        estruturaInicial += "}";
        estruturaInicial += "}";

        // Inicia a conexão HTTP com a URL do Firebase
        http.begin(url);
        http.addHeader("Content-Type", "application/json");  // Define o tipo de conteúdo como JSON
        
        // Envia os dados via método PUT (cria ou substitui)
        int httpResponseCode = http.PUT(estruturaInicial);

        // Verifica se o envio foi bem-sucedido
        if (httpResponseCode > 0) {
            Serial.println("Firebase organizado com estrutura personalizada");
        } else {
            Serial.println("Erro ao organizar Firebase: " + String(httpResponseCode));
        }

        http.end();  // Fecha a conexão HTTP
    }
}

// ============================================================
// FUNÇÃO PARA ENVIAR ALERTAS IMEDIATOS AO FIREBASE
// ============================================================
void sendAlertToFirebaseImmediately() {
    // Esta função é chamada quando há mudanças críticas nos sensores
    // Envia os dados IMEDIATAMENTE para o Firebase, sem esperar o intervalo normal
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;  // Cria uma instância do cliente HTTP
        
        // Lê os dados atuais dos sensores
        float temperatura = dht.readTemperature();
        float umidade = dht.readHumidity();

        // Verifica se a leitura foi bem sucedida
        if (!isnan(temperatura) && !isnan(umidade)) {
            // Incrementa contador de alertas imediatos
            contadorAlertasImmediate++;
            
            // Envia alerta IMEDIATO para seção específica "alertas_immediate"
            String urlAlertaImmediate = String(firebaseHost) + "alertas_immediate/" + String(contadorAlertasImmediate) + ".json";
            String jsonAlertaImmediate = "{";
            jsonAlertaImmediate += "\"temperatura\": " + String(temperatura) + ",";
            jsonAlertaImmediate += "\"umidade\": " + String(umidade) + ",";
            jsonAlertaImmediate += "\"chama\": " + String(leituraChama) + ",";
            jsonAlertaImmediate += "\"gas\": " + String(leituraGas) + ",";
            jsonAlertaImmediate += "\"alerta_temperatura\": " + String(alertaTemperatura ? "true" : "false") + ",";
            jsonAlertaImmediate += "\"alerta_umidade\": " + String(alertaUmidade ? "true" : "false") + ",";
            jsonAlertaImmediate += "\"alerta_chama\": " + String(alertaChama ? "true" : "false") + ",";
            jsonAlertaImmediate += "\"alerta_gas\": " + String(alertaGas ? "true" : "false") + ",";
            jsonAlertaImmediate += "\"mensagem\": \"" + mensagemAlerta + "\",";
            jsonAlertaImmediate += "\"tipo\": \"alerta_imediato\",";
            jsonAlertaImmediate += "\"timestamp_millis\": " + String(millis()) + ",";
            jsonAlertaImmediate += "\"timestamp_iso\": \"" + getTimestamp() + "\",";
            jsonAlertaImmediate += "\"data\": \"" + currentDate + "\",";
            jsonAlertaImmediate += "\"hora\": \"" + currentTime + "\",";
            jsonAlertaImmediate += "\"datetime\": \"" + currentDateTime + "\",";
            jsonAlertaImmediate += "\"boot_count\": " + String(bootCount) + ",";
            jsonAlertaImmediate += "\"ciclo_ativo\": " + String(millis() - inicioCiclo) + ",";
            jsonAlertaImmediate += "\"limites\": \"Temp: " + String(TEMP_BAIXA) + "-" + String(TEMP_ALTA) + "°C, Umidade: " + String(UMID_BAIXA) + "-" + String(UMID_ALTA) + "%\"";
            jsonAlertaImmediate += "}";
            
            // Inicia conexão e envia dados
            http.begin(urlAlertaImmediate);
            http.addHeader("Content-Type", "application/json");
            int httpResponseCode = http.PUT(jsonAlertaImmediate);
            
            // Verifica resposta do servidor
            if (httpResponseCode > 0) {
                Serial.println("⚡ ALERTA IMEDIATO " + String(contadorAlertasImmediate) + " enviado: " + mensagemAlerta);
                Serial.println("📅 Data/Hora do alerta: " + currentDateTime);
                firebaseStatus = true;  // Marca que o envio foi bem-sucedido
                
                // Também envia para a seção normal de alertas (para histórico completo)
                contadorAlertas++;
                String urlAlerta = String(firebaseHost) + "alertas/" + String(contadorAlertas) + ".json";
                http.begin(urlAlerta);
                http.addHeader("Content-Type", "application/json");
                http.PUT(jsonAlertaImmediate);
                http.end();
                
            } else {
                Serial.println("❌ Erro ao enviar alerta imediato: " + String(httpResponseCode));
                firebaseStatus = false;  // Marca que o envio falhou
            }
            http.end();  // Fecha a conexão
            
        } else {
            Serial.println("Erro na leitura do sensor DHT22 durante alerta imediato");
            firebaseStatus = false;
        }
    } else {
        Serial.println("Wi-Fi desconectado durante tentativa de alerta imediato.");
        firebaseStatus = false;
    }
}

// ============================================================
// FUNÇÃO PARA ENVIAR DADOS NORMAIS AO FIREBASE
// ============================================================
void sendToFirebase() {
    // Esta função é chamada periodicamente (a cada 30 segundos)
    // para enviar dados normais de monitoramento
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;  // Cria uma instância do cliente HTTP
        
        // Lê os dados atuais dos sensores
        float temperatura = dht.readTemperature();
        float umidade = dht.readHumidity();

        // Verifica se a leitura foi bem sucedida
        if (!isnan(temperatura) && !isnan(umidade)) {
            // Incrementa contador de dados normais
            contadorDados++;
            
            // Envia dados normais (apenas se não houver alertas ativos para evitar duplicação)
            if (!alertaTemperatura && !alertaUmidade && !alertaChama && !alertaGas) {
                String urlDados = String(firebaseHost) + "dados/" + String(contadorDados) + ".json";
                String jsonDados = "{";
                jsonDados += "\"temperatura\": " + String(temperatura) + ",";
                jsonDados += "\"umidade\": " + String(umidade) + ",";
                jsonDados += "\"chama\": " + String(leituraChama) + ",";
                jsonDados += "\"gas\": " + String(leituraGas) + ",";
                jsonDados += "\"timestamp_millis\": " + String(millis()) + ",";
                jsonDados += "\"timestamp_iso\": \"" + getTimestamp() + "\",";
                jsonDados += "\"data\": \"" + currentDate + "\",";
                jsonDados += "\"hora\": \"" + currentTime + "\",";
                jsonDados += "\"datetime\": \"" + currentDateTime + "\",";
                jsonDados += "\"boot_count\": " + String(bootCount) + ",";
                jsonDados += "\"ciclo_ativo\": " + String(millis() - inicioCiclo) + ",";
                jsonDados += "\"limites\": \"Temp: " + String(TEMP_BAIXA) + "-" + String(TEMP_ALTA) + "°C, Umidade: " + String(UMID_BAIXA) + "-" + String(UMID_ALTA) + "%\"";
                jsonDados += "}";

                // Envia dados normais para a seção "dados"
                http.begin(urlDados);
                http.addHeader("Content-Type", "application/json");
                int httpResponseCodeDados = http.PUT(jsonDados);
                
                // Verifica resposta
                if (httpResponseCodeDados > 0) {
                    Serial.println("📊 Dados " + String(contadorDados) + " enviados: " + 
                                  String(temperatura) + "°C, " + 
                                  String(umidade) + "%, " + 
                                  "Chama: " + String(leituraChama) + ", " +
                                  "Gás: " + String(leituraGas));
                    Serial.println("📅 Data/Hora: " + currentDateTime);
                    firebaseStatus = true;  // Marca sucesso
                } else {
                    Serial.println("Erro ao enviar dados: " + String(httpResponseCodeDados));
                    firebaseStatus = false;  // Marca falha
                }
                http.end();  // Fecha conexão
            }
            
            // Envia dados específicos dos sensores digitais com timestamp
            // Mesmo sem alerta, registra o estado atual dos sensores digitais
            
            // Envia dados do sensor de chamas para a seção "chamas"
            String urlChama = String(firebaseHost) + "chamas/" + String(contadorDados) + ".json";
            String jsonChama = "{";
            jsonChama += "\"detectado\": " + String(leituraChama == 0 ? "true" : "false") + ",";
            jsonChama += "\"valor\": " + String(leituraChama) + ",";
            jsonChama += "\"timestamp_millis\": " + String(millis()) + ",";
            jsonChama += "\"timestamp_iso\": \"" + getTimestamp() + "\",";
            jsonChama += "\"data\": \"" + currentDate + "\",";
            jsonChama += "\"hora\": \"" + currentTime + "\",";
            jsonChama += "\"datetime\": \"" + currentDateTime + "\",";
            jsonChama += "\"boot_count\": " + String(bootCount);
            jsonChama += "}";
            
            http.begin(urlChama);
            http.addHeader("Content-Type", "application/json");
            http.PUT(jsonChama);
            http.end();
            
            // Envia dados do sensor de gás para a seção "gas"
            String urlGas = String(firebaseHost) + "gas/" + String(contadorDados) + ".json";
            String jsonGas = "{";
            jsonGas += "\"detectado\": " + String(leituraGas == 0 ? "true" : "false") + ",";
            jsonGas += "\"valor\": " + String(leituraGas) + ",";
            jsonGas += "\"timestamp_millis\": " + String(millis()) + ",";
            jsonGas += "\"timestamp_iso\": \"" + getTimestamp() + "\",";
            jsonGas += "\"data\": \"" + currentDate + "\",";
            jsonGas += "\"hora\": \"" + currentTime + "\",";
            jsonGas += "\"datetime\": \"" + currentDateTime + "\",";
            jsonGas += "\"boot_count\": " + String(bootCount);
            jsonGas += "}";
            
            http.begin(urlGas);
            http.addHeader("Content-Type", "application/json");
            http.PUT(jsonGas);
            http.end();
            
        } else {
            Serial.println("Erro na leitura do sensor DHT22");
            firebaseStatus = false;
        }
    } else {
        Serial.println("Wi-Fi desconectado.");
        firebaseStatus = false;
    }
}