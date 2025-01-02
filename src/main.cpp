#include <WiFi.h>
#include <Preferences.h>
#include <BluetoothSerial.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define BUTTON_ENTER 15
#define BUTTON_UP    16
#define BUTTON_DOWN  4
#define OLED_SDA 23
#define OLED_SCL 22
#define SS_PIN 5     // SDA
#define RST_PIN 17   // RST (TX2)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 rfid(SS_PIN, RST_PIN);
int delayTime = 0; 
unsigned long startMillis = 0;
std::vector<String> helpTypes =  {"independente", "ajuda_visual_media", "ajuda_visual_alta", "ajuda_gestual", "fisica_parcial", "ajuda_fisica_total"};
String selectedHelpType = ""; 
String rfidTag = ""; 
BluetoothSerial ESP_BT; 
Preferences preferences; 

#define MAX_TRIES 10

String storedSSID;
String storedPassword;

void setupDisplay() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("Falha ao alocar SSD1306"));
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);
    display.println("");
    display.display();
}

void setupButtons() {
    pinMode(BUTTON_ENTER, INPUT_PULLUP);
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
}

void setupRFID() {
    SPI.begin(18, 21, 19);  // SCK, MISO, MOSI
    rfid.PCD_Init();        // Inicializa o RC522
    Serial.println("Aproxime o cartão RFID ao leitor...");
}

void readRFID() {
    // Verifica se há um cartão RFID
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        rfidTag = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            rfidTag += String(rfid.uid.uidByte[i], HEX);
        }

        // Exibe o número do cartão no display e no serial
        display.clearDisplay();
        display.setCursor(0, 2);
        display.print("Cartão RFID: ");
        display.println(rfidTag);
        display.display();

        // Exibe no Serial Monitor
        Serial.print("Cartão RFID detectado: ");
        Serial.println(rfidTag);

        // Finaliza a leitura do cartão
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
    }
}

String navigateList(const std::vector<String>& itemList) {
    int selectedItem = 0;
    int topLineIndex = 0;
    int itemsPerPage = 6;  // Número máximo de itens por página

    while (true) {
        // Exibir a lista no OLED
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        int yPosition = 0;
        for (int i = topLineIndex; i < topLineIndex + itemsPerPage && i < itemList.size(); i++) {
            if (i == selectedItem) {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverte as cores para destacar o item selecionado
            } else {
                display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(0, yPosition);
            display.println(itemList[i]);
            yPosition += 10;
        }
        display.display();

        // Leitura dos botões
        bool currentButtonUpState = digitalRead(BUTTON_UP);
        bool currentButtonDownState = digitalRead(BUTTON_DOWN);
        bool currentButtonEnterState = digitalRead(BUTTON_ENTER);

        // Navegar para cima
        if (currentButtonUpState == LOW) {
            selectedItem--;
            if (selectedItem < 0) {
                selectedItem = itemList.size() - 1;
                topLineIndex = std::max(0, static_cast<int>(itemList.size()) - itemsPerPage);
            }
            if (selectedItem < topLineIndex) {
                topLineIndex = selectedItem;
            }
        }

        // Navegar para baixo
        if (currentButtonDownState == LOW) {
            selectedItem++;
            if (selectedItem >= itemList.size()) {
                selectedItem = 0;
                topLineIndex = 0;
            }
            if (selectedItem >= topLineIndex + itemsPerPage) {
                topLineIndex++;
            }
        }

        // Selecionar item com ENTER
        if (currentButtonEnterState == LOW) {
            return itemList[selectedItem];
        }
    }
}

void setDelayTime() {
    while (true) {
        display.clearDisplay();
        display.setCursor(0, 2);
        display.print("Tempo de atraso: ");
        display.print(delayTime);
        display.print(" segundos");
        display.display();

        // Leitura dos botões para ajuste
        bool currentButtonUpState = digitalRead(BUTTON_UP);
        bool currentButtonDownState = digitalRead(BUTTON_DOWN);
        bool currentButtonEnterState = digitalRead(BUTTON_ENTER);

        // Aumentar o tempo
        if (currentButtonUpState == LOW) {
            delayTime++;
            delay(200);  // Debounce
        }

        // Diminuir o tempo (mínimo 0)
        if (currentButtonDownState == LOW && delayTime > 0) {
            delayTime--;
            delay(200);  // Debounce
        }

        // Avançar para o próximo passo
        if (currentButtonEnterState == LOW) {
            break;
        }
    }
}


void selectHelpType() {
    String selectedType = navigateList(helpTypes);
    Serial.print("Tipo de ajuda selecionado: ");
    Serial.println(selectedType);
    selectedHelpType = selectedType;  // Salva a escolha do tipo de ajuda
}



void startDelayProcess() {
    delayTime = 10; 
    unsigned long startMillis = millis();  
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);
    display.print("Tempo de espera: ");
    display.print(delayTime);
    display.println("s");
    display.print("Tipo de ajuda: ");
    //display.println(helpTypes[selectedHelpType]);
    display.display();
}

void sendDataToEndpoint(int delayTime, String helpType, String result) {
    // Lê o endpoint diretamente da NVS
    preferences.begin("wifi_data", true);
    String endpoint = preferences.getString("Endpoint");  // Lê o endpoint da NVS
    preferences.end();

    HTTPClient http;
    String payload = "{\"tempo de espera\":" + String(delayTime) + ", \"tipo de ajuda\":\"" + helpType + "\", \"resultado\":\"" + result + "\"}";

    http.begin(endpoint);  // Usa o endpoint lido da NVS
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
        Serial.println("Enviado com sucesso!");
    } else {
        Serial.print("Erro ao enviar: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void sendData(String result) {
    // Lê o endpoint diretamente da NVS
    preferences.begin("wifi_data", true);
    String endpoint = preferences.getString("Endpoint");  // Lê o endpoint da NVS
    preferences.end();

    HTTPClient http;
    String jsonPayload = "{\"tempo de espera\": " + String(delayTime) +
                         ", \"tipo de ajuda\": \"" + helpTypes[selectedHelpType] +
                         "\", \"resultado\": \"" + result + "\"}";

    http.begin(endpoint);  // Usa o endpoint lido da NVS
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
        Serial.println("Dados enviados com sucesso");
    } else {
        Serial.println("Erro ao enviar dados");
    }

    http.end();
}
// Função para verificar a conexão Wi-Fi com os dados salvos
bool tryConnectWiFi(String ssid, String password) {
    WiFi.begin(ssid.c_str(), password.c_str());
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < MAX_TRIES) {
        delay(1000);
        Serial.print(".");
        attempt++;
    }

    return WiFi.status() == WL_CONNECTED;
}

// Função para limpar os dados da NVS
void clearNVSData() {
    preferences.clear();  // Limpa todos os dados salvos na NVS
    Serial.println("Dados NVS limpos.");
}

// Função para iniciar o Bluetooth e receber os dados
void startBluetooth() {
    ESP_BT.begin("ESP32_Provisioning");
    Serial.println("Aguardando conexão Bluetooth...");
    while (!ESP_BT.available()) {
        delay(100);
    }
    String bluetoothData = ESP_BT.readStringUntil('\n');
    Serial.print("Dados recebidos via Bluetooth: ");
    Serial.println(bluetoothData);
    String newSSID = bluetoothData.substring(0, bluetoothData.indexOf(","));
    String newPassword = bluetoothData.substring(bluetoothData.indexOf(",") + 1, bluetoothData.indexOf(",", bluetoothData.indexOf(",") + 1));
    String newEndpoint = bluetoothData.substring(bluetoothData.lastIndexOf(",") + 1);
    preferences.begin("wifi_data", false);
    preferences.putString("SSID", newSSID);
    preferences.putString("Password", newPassword);
    preferences.putString("Endpoint", newEndpoint);
    preferences.end();
    Serial.println("Dados de Wi-Fi e Endpoint armazenados na NVS.");
}

// Função de provisionamento
void provision() {
    preferences.begin("wifi_data", false);

    // Verificar se há dados salvos na NVS
    if (preferences.isKey("SSID") && preferences.isKey("Password")) {
        storedSSID = preferences.getString("SSID");
        storedPassword = preferences.getString("Password");
        Serial.println("Dados encontrados na NVS.");
        
        // Tentar conectar com os dados salvos
        if (tryConnectWiFi(storedSSID, storedPassword)) {
            Serial.println("Conexão Wi-Fi bem-sucedida!");
        } else {
            Serial.println("Falha na conexão Wi-Fi.");
            clearNVSData();  // Limpar NVS após falha de conexão
            startBluetooth(); // Iniciar Bluetooth para receber novos dados
        }
    } else {
        Serial.println("Sem dados na NVS. Iniciando Bluetooth...");
        startBluetooth();  // Iniciar Bluetooth para receber dados
    }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);  // Configurar o ESP32 no modo estação
  provision();  // Chama a função de provisionamento
  setupDisplay();
  setupButtons();
  setupRFID();
  setDelayTime();
  selectHelpType();
  String result = "sem_resposta";  // Inicializa como "sem_resposta"
  unsigned long startMillis = millis();
  while (millis() - startMillis < delayTime * 1000) {
    readRFID();  // Tenta ler o RFID durante o tempo de atraso
    if (rfidTag != "") {  // Se um cartão RFID foi lido
      result = "correto";
      break;
    }
    delay(100);  // Aguarda para evitar sobrecarga do processador
  }
  sendDataToEndpoint(delayTime, helpTypes[selectedHelpType], result);
}

void loop() {
    // Lê o estado dos botões
    bool currentButtonEnterState = digitalRead(BUTTON_ENTER);
    
    static bool lastButtonEnterState = HIGH;
    
    // Verifica se o botão Enter foi pressionado para iniciar ou reiniciar o processo
    if (lastButtonEnterState == HIGH && currentButtonEnterState == LOW) {
        delay(200);  // Debounce para evitar múltiplos cliques
        startDelayProcess();
    }
    
    lastButtonEnterState = currentButtonEnterState;
    
    // Leitura do RFID durante o processo de atraso
    if (millis() - startMillis < delayTime * 1000) {
        if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
            // Lê o valor da tag RFID
            rfidTag = "";
            for (byte i = 0; i < rfid.uid.size; i++) {
                rfidTag += String(rfid.uid.uidByte[i], HEX);
            }
            // Finaliza a leitura, considerando como "correto"
            sendData("correto");
            return;
        }
    } else {
        // Se o tempo de espera terminou e nenhuma tag foi lida
        if (rfidTag == "") {
            sendData("sem_resposta");
        }
    }
}
