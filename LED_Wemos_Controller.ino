#include <FastLED.h>

#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>

#include <ArduinoOTA.h>

#include <aREST.h>

#define MIN_COLOR 0x0
#define MAX_COLOR 0xffffff

#define NUM_LEDS 40
#define DATA_PIN 15
#define CYCLE_START_HUE HSVHue::HUE_RED
#define CYCLE_TOTAL_TIME 3000 //milliseconds
#define PATTERN_LENGTH 4
//Definition of global vars
CRGB leds[NUM_LEDS];

aREST rest = aREST();

const char *ssid = "Tell my Wi-Fi love her_EXT";
const char *password = "Paris2016";

// The port to listen for incoming TCP connections
#define LISTEN_PORT 80

// Create an instance of the server
WiFiServer server(LISTEN_PORT);

bool cycleEnabled = false;
CHSV cycleHSV = CHSV(CYCLE_START_HUE, 255, 255);
unsigned long lastCycle = 0; // will store last cycle tick
unsigned long cycleInterval;

//Definition of routes
int colorCallback(String);        //Sets general color
int shutdownLEDsCallback(String); //Shuts down LEDs
int cycleCallback(String);        //Cycles color of LEDs
int patternCallback(String);      //Sets each LED's color according to pattern
//TODO: Dim and brighten

//Definition of private methods
void setAllLEDsToColor(long);
void shutdownLEDs();
void handleCycle();
void parseLEDPatternConfig(String, String, long*);

void setup()
{
    //Setup pins for level converter
    pinMode(14, OUTPUT); // D5 LV
    pinMode(12, OUTPUT); // D6 GND
    pinMode(15, OUTPUT); // Data
    pinMode(13, INPUT);
    pinMode(16, INPUT);
    digitalWrite(12, LOW);
    digitalWrite(14, HIGH);
    digitalWrite(15, LOW);

    // Start Serial
    Serial.begin(74880);

    //Setup LEDs
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

    // Declare routes
    rest.function("color", colorCallback);
    rest.function("shutdown", shutdownLEDsCallback);
    rest.function("cycle", cycleCallback);
    rest.function("pattern", patternCallback);

    // Give name & ID to the device (ID should be 6 characters long)
    rest.set_id("1");
    rest.set_name("LED_DESK");

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    // Start the server
    server.begin();
    Serial.println("Server started");

    // Print the IP address
    Serial.println(WiFi.localIP());

    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if (!MDNS.begin("LED_DESK"))
    {
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");

    // Start TCP (HTTP) server
    server.begin();
    Serial.println("TCP server started");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", LISTEN_PORT);

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname("LED_DESK");

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");

    ArduinoOTA.onStart([]() {
        Serial.println("Start");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    //Calculates cycle interval value
    cycleInterval = CYCLE_TOTAL_TIME / 255;
}

void loop()
{
    ArduinoOTA.handle();

    //Handle cycle
    handleCycle();

    // Handle REST calls
    WiFiClient client = server.available();

    if (!client)
    {
        return;
    }

    while (!client.available())
    {
        delay(1);
    }

    rest.handle(client);
}

//Implementation of routes

//Sets general color
int colorCallback(String rgb)
{
    Serial.println("colorCallback: " + rgb);
    long color = strtol(rgb.c_str(), NULL, 16);
    Serial.println("Color number: " + String(color, HEX));

    if (color == 0)
        return 0;

    cycleEnabled = false;

    color = constrain(color, MIN_COLOR, MAX_COLOR);

    setAllLEDsToColor(color);

    return true;
}

//Shuts down LEDs
int shutdownLEDsCallback(String)
{
    shutdownLEDs();

    return true;
}

//Cycles color of LEDs
int cycleCallback(String enableCycle)
{
    cycleEnabled = (enableCycle.length() == 0 || enableCycle.equalsIgnoreCase("true"));

    if (cycleEnabled)
        cycleHSV = CHSV(CYCLE_START_HUE, 255, 255);

    return cycleEnabled;
}

//Sets each LED's color according to config
int patternCallback(String pattern)
{

    long ledValues[PATTERN_LENGTH];
    parseLEDPatternConfig(pattern, ";", ledValues);

    for (int i = 0; i < NUM_LEDS; i += PATTERN_LENGTH)
    {
        for (int j = 0; j < PATTERN_LENGTH; j++)
        {
            leds[i + j] = constrain(ledValues[j], MIN_COLOR, MAX_COLOR);
        } 
    }

    FastLED.show();
    return true;
}

//General methods
void setAllLEDsToColor(long color)
{
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = color;

    FastLED.show();
}

void setAllLEDsToColor(CHSV color)
{
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = color;

    FastLED.show();
}

void shutdownLEDs()
{
    cycleEnabled = false;
    setAllLEDsToColor(CRGB::Black);
}

void handleCycle()
{

    if (cycleEnabled)
    {

        unsigned long currentMillis = millis();

        if (currentMillis - lastCycle >= cycleInterval)
        {

            lastCycle = currentMillis;

            cycleHSV.hue++;
            setAllLEDsToColor(cycleHSV);
        }
    }
}

void parseLEDPatternConfig(String pattern, String delimiter, long* array)
{
    char *pattern_c = new char[pattern.length()];
    strcpy(pattern_c, pattern.c_str());

    const char *delimiters_c = delimiter.c_str();

    char *splitted = strtok(pattern_c, delimiters_c);

    int i = 0;
    while (splitted != NULL && i < PATTERN_LENGTH)
    {
        array[i] = strtol(splitted, NULL, 16);

        splitted = strtok(NULL, delimiters_c);
        i++;
    }
}
