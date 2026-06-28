#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#define LED_PIN 2

const char* ssid = "ESP32 Chat";
const char* password = "12345678";

WebServer server(80);
WebSocketsServer webSocket(81);

void handleRoot() {
    File file = LittleFS.open("/index.html", "r");

    if (!file) {
        server.send(404, "text/plain", "index.html not found");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
}

void handleCSS() {
    File file = LittleFS.open("/style.css", "r");

    if (!file) {
        server.send(404, "text/plain", "style.css not found");
        return;
    }

    server.streamFile(file, "text/css");
    file.close();
}

void handleJS() {
    File file = LittleFS.open("/script.js", "r");

    if (!file) {
        server.send(404, "text/plain", "script.js not found");
        return;
    }

    server.streamFile(file, "application/javascript");
    file.close();
}
void handlePacket(uint8_t client, String payload);
void webSocketEvent(uint8_t num,
                    WStype_t type,
                    uint8_t * payload,
                    size_t length)
{

    switch(type)
    {

        case WStype_CONNECTED:
        {
        
            Serial.printf("Client %u Connected\n", num);
        
            File file = LittleFS.open("/chat.log","r");
        
            if(file)
            {
                while(file.available())
                {
                    String line = file.readStringUntil('\n');
        
                    line.trim();
        
                    if(line.length())
                        webSocket.sendTXT(num,line);
                }
        
                file.close();
            }
        
        }
        break;

        case WStype_DISCONNECTED:

            Serial.printf("Client %u Disconnected\n", num);

            break;

        case WStype_TEXT:
        {
            handlePacket(num, (char*)payload);
        }
        break;

        default:
            break;

    }

}
void handlePacket(uint8_t client, String payload)
        {
            JsonDocument doc;
        
            DeserializationError err = deserializeJson(doc, payload);
        
            if (err)
            {
                Serial.println("Invalid JSON");
                return;
            }
        
            String type = doc["type"];
        
            if (type == "message")
            {
                Serial.println("Message");
        
                webSocket.broadcastTXT(payload);
        
                digitalWrite(LED_PIN, HIGH);
                delay(100);
                digitalWrite(LED_PIN, LOW);
        
                File file = LittleFS.open("/chat.log", FILE_APPEND);
        
                if (file)
                {
                    file.println(payload);
                    file.close();
                }
        
                return;
            }
        
            if (type == "join")
            {
                Serial.println("Join packet");
        
                return;
            }
        }

void createChatLog()
{
    if(!LittleFS.exists("/chat.log"))
    {
        File file = LittleFS.open("/chat.log", FILE_WRITE);

        if(file)
            file.close();
    }
}
void setup()
{

    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);


    if(!LittleFS.begin(true))
    {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    createChatLog();

    WiFi.softAP(ssid,password);

    Serial.print("IP Address : ");
    Serial.println(WiFi.softAPIP());

    server.on("/",handleRoot);

    server.on("/style.css",handleCSS);

    server.on("/script.js",handleJS);

    server.begin();

    webSocket.begin();

    webSocket.onEvent(webSocketEvent);

    Serial.println("Server Ready");

}

void loop()
{

    server.handleClient();

    webSocket.loop();

}