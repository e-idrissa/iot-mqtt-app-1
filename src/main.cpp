#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// Définition des broches
#define DHTPIN 12
#define LED 14
#define SERVO_PIN 26

// Paramètres du capteur DHT
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

// Initialisation du servo
Servo servo;

// Informations de connexion WiFi et MQTT
const char *ssid = "your-wifi-ssid";
const char *password = "your-wifi-password";
const char *mqttServer = "your-mqtt.broker.com";
const char *clientID = "your-client-id";
const char *topic = "your-topic";

// Variables pour le timing sans blocage
unsigned long previousMillis = 0;
const long interval = 5000;
String msgStr = "";
float temp = 0.0;
float hum = 0.0;

unsigned long lastReconnectAttempt = 0;

// Création du client WiFi et MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Fonction appelée lorsqu’un message MQTT est reçu
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("📩 Message reçu sur le topic : ");
    Serial.println(topic);
    String data = "";
    for (int i = 0; i < length; i++)
    {
        data += (char)payload[i];
    }
    Serial.println("📄 Contenu : " + data);

    if (String(topic) == "your-name/lights")
    {
        data.toLowerCase();
        if (data == "true")
        {
            digitalWrite(LED, HIGH);
        }
        else
        {
            digitalWrite(LED, LOW);
        }
    }
    else if (String(topic) == "your-name/servo")
    {
        int degree = data.toInt();
        Serial.print("⚙️ Déplacement du servo à : ");
        Serial.println(degree);
        servo.write(degree);
    }
}

// Fonction pour se connecter au réseau WiFi
void connectToWiFi()
{
    Serial.println("🔃 Connexion au WiFi...");
    WiFi.begin(ssid, password);
}

// Fonction pour se reconnecter au broker MQTT en cas de déconnexion
bool reconnect()
{
    Serial.print("⛓️‍💥 Tentative de connexion MQTT...");
    if (client.connect(clientID))
    {
        Serial.println("✅ Connecté !");
        client.subscribe("your-name/lights");
        client.subscribe("your-name/servo");
    }
    else
    {
        Serial.print("❌ Échec, code=");
        Serial.println(client.state());
    }
    return client.connected();
}

// Fonction setup exécutée au démarrage
void setup()
{
    Serial.begin(115200);
    delay(100); // Petite pause pour la stabilisation

    // Initialisation des composants
    dht.begin();
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    servo.attach(SERVO_PIN);
    servo.write(20);

    // Définir le serveur MQTT et le callback
    client.setServer(mqttServer, 1883);
    client.setCallback(callback);

    // Lancer la première tentative de connexion WiFi (non bloquante)
    connectToWiFi();
}

// Boucle principale exécutée en continu
void loop()
{
    // Si pas connecté au WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("Statut WiFi: En attente... Code: ");
        Serial.println(WiFi.status());
        if (millis() - lastReconnectAttempt > 5000)
        {
            lastReconnectAttempt = millis();
            connectToWiFi();
        }
        Serial.print(".");
        return;
    }

    // Le WiFi est connecté, on affiche l'IP une seule fois
    if (WiFi.status() == WL_CONNECTED && lastReconnectAttempt == 0)
    {
        Serial.println("");
        Serial.println("------------------------------------------");
        Serial.println("🛜 WiFi connecté !");
        Serial.print("Adresse IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("------------------------------------------");
        lastReconnectAttempt = millis(); // Pour ne pas réafficher l'IP
    }

    // Si le client MQTT n'est pas connecté
    if (!client.connected())
    {
        if (millis() - lastReconnectAttempt > 5000)
        {
            lastReconnectAttempt = millis();
            if (reconnect())
            {
                lastReconnectAttempt = 0; // Réinitialiser le timer après une connexion réussie
            }
        }
    }

    // Gérer la communication MQTT
    client.loop();

    // Logique du programme (lecture des capteurs, etc.)
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;

        sensors_event_t event;
        float currentTemp = temp;
        float currentHum = hum;

        // 1. TENTATIVE DE LECTURE DE LA TEMPÉRATURE
        dht.temperature().getEvent(&event);
        if (!isnan(event.temperature))
        {
            currentTemp = event.temperature;
            Serial.print("🌡️ Température : ");
            Serial.println(currentTemp);
        }

        // 2. TENTATIVE DE LECTURE DE L'HUMIDITÉ
        dht.humidity().getEvent(&event);
        if (!isnan(event.relative_humidity))
        {
            currentHum = event.relative_humidity;
            Serial.print("💧 Humidité : ");
            Serial.println(currentHum);
        }

        // 3. MISE À JOUR DES GLOBALES SEULEMENT APRÈS UNE LECTURE VALIDE
        if (!isnan(currentTemp) && !isnan(currentHum))
        {
            temp = currentTemp;
            hum = currentHum;
        }

        // 4. PUBLICATION MQTT
        // Condition de publication sécurisée : on ne publie que si la connexion est établie
        // ET si les deux valeurs NE sont PAS 0.0 (car 0.0 est la valeur par défaut)
        if (client.connected() && (temp <= 0.0 || hum <= 0.0))
        {
            msgStr = String(temp) + "," + String(hum);
            char msg[msgStr.length() + 1];
            msgStr.toCharArray(msg, sizeof(msg));
            client.publish(topic, msg);
            Serial.print("📤 Données publiées sur ");
            Serial.print(topic);
            Serial.print(" : ");
            Serial.println(msgStr);
        }
        else if (client.connected())
        {
            // Utile pour le diagnostic : indique que les lectures sont encore mauvaises.
            Serial.println("⚠️ DHT non initialisé ou lectures échouées. NON PUBLIÉ.");
        }
    }
}