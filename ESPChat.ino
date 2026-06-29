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
#define MAX_CLIENTS 8

struct ClientInfo
{
    bool connected;
    String username;
};

ClientInfo clients[MAX_CLIENTS];

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

void broadcastUserList()
{
    JsonDocument doc;

    doc["type"] = "users";

    JsonArray users = doc["users"].to<JsonArray>();

    int online = 0;

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i].connected && clients[i].username != "")
        {
            users.add(clients[i].username);
            online++;
        }
    }

    doc["online"] = online;

    String json;
    serializeJson(doc, json);

    webSocket.broadcastTXT(json);

    Serial.println("Broadcasting user list:");
    Serial.println(json);
}

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
        {
            Serial.printf("Client %u Disconnected\n", num);
        
            String name = clients[num].username;
        
            if (name != "")
            {
                broadcastSystemMessage(name + " left the chat");
            }
        
            clients[num].connected = false;
            clients[num].username = "";
        
            broadcastUserList();
        }
        break;

        case WStype_TEXT:
        {
            handlePacket(num, String((char*)payload));
        }
        break;
    }

}

void broadcastSystemMessage(String text)
{
    JsonDocument doc;

    doc["type"] = "system";
    doc["message"] = text;

    String json;
    serializeJson(doc, json);

    webSocket.broadcastTXT(json);

    Serial.println(json);
}

void handlePacket(uint8_t client, String payload)
        {
            Serial.println(payload);
            JsonDocument doc;
        
            DeserializationError err = deserializeJson(doc, payload);
        
            if (err)
            {
                Serial.println("Invalid JSON");
                return;
            }
        
            String type = doc["type"];
        
            if(type=="typing")
            {
                webSocket.broadcastTXT(payload);
            
                digitalWrite(LED_PIN,HIGH);
                delay(15);
                digitalWrite(LED_PIN,LOW);
            
                return;
            }

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
        
           if(type=="join")
            {
                clients[client].connected = true;
                clients[client].username = doc["username"].as<String>();

                broadcastSystemMessage(clients[client].username + " joined the chat");

                broadcastUserList();

                Serial.print("Registered Client ");
                Serial.print(client);
                Serial.print(": ");
                Serial.println(clients[client].username);

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