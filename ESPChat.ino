#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#define LED_PIN 2
#define MAX_FILE_SIZE (200 * 1024)

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

File uploadFile;
String uploadedFileName;
String uploadedDisplayName;
size_t uploadedFileSize = 0;
bool uploadFailed = false;
String uploadError;

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

String sanitizeFileName(String fileName)
{
    fileName.replace("\\", "/");

    int slash = fileName.lastIndexOf('/');
    if (slash >= 0)
        fileName = fileName.substring(slash + 1);

    String cleanName;

    for (size_t i = 0; i < fileName.length() && cleanName.length() < 48; i++)
    {
        char c = fileName.charAt(i);

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_')
        {
            cleanName += c;
        }
        else
        {
            cleanName += '_';
        }
    }

    if (cleanName == "" || cleanName == "." || cleanName == "..")
        cleanName = "file";

    return cleanName;
}

void handleFileUpload()
{
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadedDisplayName = sanitizeFileName(upload.filename);
        uploadedFileName = String(millis()) + "_" + uploadedDisplayName;
        uploadedFileSize = 0;
        uploadFailed = false;
        uploadError = "";

        String path = "/uploads/" + uploadedFileName;
        uploadFile = LittleFS.open(path, FILE_WRITE);

        if (!uploadFile)
        {
            uploadFailed = true;
            uploadError = "Could not create file";
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFailed)
            return;

        if (uploadedFileSize + upload.currentSize > MAX_FILE_SIZE)
        {
            uploadFailed = true;
            uploadError = "File exceeds 200 KB limit";
            uploadFile.close();
            LittleFS.remove("/uploads/" + uploadedFileName);
            return;
        }

        size_t written = uploadFile.write(upload.buf, upload.currentSize);

        if (written != upload.currentSize)
        {
            uploadFailed = true;
            uploadError = "LittleFS is full";
            uploadFile.close();
            LittleFS.remove("/uploads/" + uploadedFileName);
            return;
        }

        uploadedFileSize += written;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
            uploadFile.close();
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        uploadFailed = true;
        uploadError = "Upload cancelled";

        if (uploadFile)
            uploadFile.close();

        LittleFS.remove("/uploads/" + uploadedFileName);
    }
}

void handleUploadComplete()
{
    JsonDocument doc;

    if (uploadFailed)
    {
        doc["ok"] = false;
        doc["error"] = uploadError;

        String json;
        serializeJson(doc, json);
        server.send(uploadError.indexOf("200 KB") >= 0 ? 413 : 500,
                    "application/json", json);
        return;
    }

    doc["ok"] = true;
    doc["name"] = uploadedDisplayName;
    doc["size"] = uploadedFileSize;
    doc["url"] = "/files?name=" + uploadedFileName;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void handleFileDownload()
{
    if (!server.hasArg("name"))
    {
        server.send(400, "text/plain", "Missing file name");
        return;
    }

    String storedName = sanitizeFileName(server.arg("name"));
    String path = "/uploads/" + storedName;

    if (!LittleFS.exists(path))
    {
        server.send(404, "text/plain", "File not found");
        return;
    }

    String downloadName = storedName;

    if (server.hasArg("download"))
        downloadName = sanitizeFileName(server.arg("download"));

    File file = LittleFS.open(path, "r");

    server.sendHeader("Content-Disposition",
                      "attachment; filename=\"" + downloadName + "\"");
    server.streamFile(file, "application/octet-stream");
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

            if (type == "message" || type == "file")
            {
                Serial.println(type == "message" ? "Message" : "File");
        
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

    if(!LittleFS.exists("/uploads"))
        LittleFS.mkdir("/uploads");

    WiFi.softAP(ssid,password);

    Serial.print("IP Address : ");
    Serial.println(WiFi.softAPIP());

    server.on("/",handleRoot);

    server.on("/style.css",handleCSS);

    server.on("/script.js",handleJS);

    server.on("/upload", HTTP_POST, handleUploadComplete, handleFileUpload);

    server.on("/files", HTTP_GET, handleFileDownload);

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
