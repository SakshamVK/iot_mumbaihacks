#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Atlas5";
const char* password = "FutureisTech";
const char* apiEndpoint = "https://getpantry.cloud/apiv1/pantry/6a63afc9-b82b-4e17-a319-17d5ab82e9cd/basket/bin-0001";

const int irSensorPin = 5;
const int redLEDPin = 4;
const int whiteLEDPin = 0;

bool binFull = false;
unsigned long lastNotificationTime = 0;
const unsigned long notificationInterval = 1000; // Reduced to 5 seconds

// Debouncing variables
const int READINGS_COUNT = 3;  // Reduced to speed up detection
int readings[READINGS_COUNT];
int readIndex = 0;
unsigned long lastReadTime = 0;
const unsigned long READ_DELAY = 100;

WiFiClientSecure client;

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println("\n\nStarting up...");
    for (int i = 0; i < READINGS_COUNT; i++) {
        readings[i] = HIGH;  // Initialize as HIGH (empty)
    }

    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
        delay(500);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }

    client.setInsecure();
    pinMode(irSensorPin, INPUT);
    pinMode(redLEDPin, OUTPUT);
    pinMode(whiteLEDPin, OUTPUT);
    digitalWrite(whiteLEDPin, HIGH);
    Serial.println("Setup complete!\n");
}

// Get stable IR reading
bool isObjectDetected() {
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime >= READ_DELAY) {
        lastReadTime = currentTime;
        readings[readIndex] = digitalRead(irSensorPin);
        readIndex = (readIndex + 1) % READINGS_COUNT;

        int lowCount = 0;
        for (int i = 0; i < READINGS_COUNT; i++) {
            if (readings[i] == LOW) {
                lowCount++;
            }
        }
        return (lowCount > READINGS_COUNT / 2);
    }
    int lowCount = 0;
    for (int i = 0; i < READINGS_COUNT; i++) {
        if (readings[i] == LOW) {
            lowCount++;
        }
    }
    return (lowCount > READINGS_COUNT / 2);
}

void sendNotification() {
    Serial.println("\n=== Starting HTTP Request ===");

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        bool beginSuccess = http.begin(client, apiEndpoint);
        
        if (!beginSuccess) {
            Serial.println("Failed to begin HTTP connection!");
            return;
        }
        
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");

        String jsonData = "{\"status\":\"" + String(binFull ? "full" : "not_full") + "\"}";
        Serial.println("Payload: " + jsonData);

        int httpResponseCode = http.PUT(jsonData);

        Serial.print("Response Code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response: " + response);
            digitalWrite(whiteLEDPin, LOW);
            delay(200);
            digitalWrite(whiteLEDPin, HIGH);
        } else {
            Serial.print("Error: ");
            Serial.println(http.errorToString(httpResponseCode));

            for (int i = 0; i < 3; i++) {
                digitalWrite(redLEDPin, HIGH);
                delay(100);
                digitalWrite(redLEDPin, LOW);
                delay(100);
            }
        }

        http.end();
        Serial.println("=== HTTP Request Complete ===\n");
    } else {
        Serial.println("WiFi disconnected! Attempting to reconnect...");
        WiFi.begin(ssid, password);
    }
}

void loop() {
    bool objectDetected = isObjectDetected();

    static bool lastObjectState = !objectDetected;
    if (lastObjectState != objectDetected) {
        Serial.print("IR State Changed - Object Detected: ");
        Serial.println(objectDetected ? "YES" : "NO");
        lastObjectState = objectDetected;
    }

    unsigned long currentTime = millis();

    if (objectDetected && !binFull) {
        Serial.println("\n*** Bin state changed to FULL ***");
        binFull = true;
        digitalWrite(redLEDPin, HIGH);
        digitalWrite(whiteLEDPin, LOW);
        sendNotification();  // Immediate notification
        lastNotificationTime = currentTime;
    } 
    else if (!objectDetected && binFull) {
        Serial.println("\n*** Bin state changed to EMPTY ***");
        binFull = false;
        digitalWrite(redLEDPin, LOW);
        digitalWrite(whiteLEDPin, HIGH);
        sendNotification();  // Immediate notification
        lastNotificationTime = currentTime;
    }

    delay(100);  // Reduced delay for more responsive readings
}
