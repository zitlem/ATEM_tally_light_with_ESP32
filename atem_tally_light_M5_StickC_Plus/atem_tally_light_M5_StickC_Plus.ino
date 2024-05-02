#ifndef VERSION
#define VERSION "dev"
#endif

int key = 0;              // temporary variable to get tallyNo incremented via Button
int keyR = 0;             // another temporary variable to get tallyNo incremented via Button
int prevStateTesting = 0; // used to put tally's previous state (LED Strip)
int buttStateTesting = 0; // used to put button's previous state (LED Strip)
int prevStateTesting2 = 0; // used to put tally's previous state (LCD Screen)
int buttStateTesting2 = 0; // used to put button's previous state (LCD Screen)
int tallyState = 0;
int buttonTallyNo = 0; // on boot Tally Number will be provied by web GUI, once "A"-button is pressed, then tally no will be incremented from button

#ifndef CHIP_FAMILY
#define CHIP_FAMILY "Unknown"
#endif

#ifndef VERSION
#define VERSION "Unknown"
#endif

#define DISPLAY_NAME "Tally Light"

#include "ImprovWiFiLibrary.h"
#include "M5StickCPlus.h"
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ATEMmin.h>
#include <TallyServer.h>
#include <FastLED.h>

// Define LED colors
#define LED_OFF 0
#define LED_RED 1
#define LED_GREEN 2
#define LED_BLUE 3
#define LED_YELLOW 4
#define LED_PINK 5
#define LED_WHITE 6
#define LED_ORANGE 7

// Map "old" LED colors to CRGB colors
CRGB color_led[8] = {CRGB::Black, CRGB::Red, CRGB::Lime, CRGB::Blue, CRGB::Yellow, CRGB::Fuchsia, CRGB::White, CRGB::Orange};

// Define states
#define STATE_STARTING 0
#define STATE_CONNECTING_TO_WIFI 1
#define STATE_CONNECTING_TO_SWITCHER 2
#define STATE_RUNNING 3

// Define modes of operation
#define MODE_NORMAL 1
#define MODE_PREVIEW_STAY_ON 2
#define MODE_PROGRAM_ONLY 3
#define MODE_ON_AIR 4

#define TALLY_FLAG_OFF 0
#define TALLY_FLAG_PROGRAM 1
#define TALLY_FLAG_PREVIEW 2

// Define Neopixel status-LED options
#define NEOPIXEL_STATUS_FIRST 1
#define NEOPIXEL_STATUS_LAST 2
#define NEOPIXEL_STATUS_NONE 3

// FastLED
#define TALLY_DATA_PIN 26 // pin to be connected to WS2812 data pin
#define POTENTIOMETER_PIN 36  // data pin to connect potentiometer

int brightness_value_prev;
int brightness_value_post;

int numTallyLEDs;
int numStatusLEDs;
CRGB *leds;
CRGB *tallyLEDs;
CRGB *statusLED;
bool neopixelsUpdated = false;

// Initialize global variables

WebServer server(80);

#ifndef TALLY_TEST_SERVER
ATEMmin atemSwitcher;
#else
int tallyFlag = TALLY_FLAG_OFF;
#endif

TallyServer tallyServer;

ImprovWiFi improv(&Serial);

uint8_t state = STATE_STARTING;

// Define struct for holding tally settings (mostly to simplify EEPROM read and write, in order to persist settings)
struct Settings
{
    char tallyName[32] = "";
    uint8_t tallyNo;
    uint8_t tallyModeLED1;
    uint8_t tallyModeLED2;
    bool staticIP;
    bool potEnable;
    IPAddress tallyIP;
    IPAddress tallySubnetMask;
    IPAddress tallyGateway;
    IPAddress switcherIP;
    uint16_t neopixelsAmount;
    uint8_t neopixelStatusLEDOption;
    uint8_t neopixelBrightness;
    int firstEverBoot;
};

Settings settings;

bool firstRun = true;

int bytesAvailable = false;
uint8_t readByte;

void onImprovWiFiErrorCb(ImprovTypes::Error err)
{
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password)
{
}

// naming functions that are to be used in program
void changeState(uint8_t stateToChangeTo);      // Handle the change of states in the program
void setSTRIP(uint8_t color, uint16_t tallyNo); // Set the LED Strip array
void setStatusLED(uint8_t color);               // Set Status led on LED Strip
int getTallyState(uint16_t tallyNo);            // Get the tally state from Atem switcher
int getLedColor(int tallyMode, uint16_t tallyNo);              // Get color according to Atem switcher mode
void getLcdColor(int tallyMode, uint16_t tallyNo);
void handleRoot();                              // handle html page
void handleSave();                              // set values in Eeprom from webpage
void handleNotFound();                          // if server not avail 404
String getSSID();
void setWiFi(String ssid, String pwd);
void batts();
unsigned long startTime;
unsigned long countdownDuration = 120000; // 120 seconds
bool countdownStarted = false;
int secondsRemaining = 0; 

// Perform initial setup on power on
void setup()
{
    M5.begin();                        // initiliaze m5stickC Plus library
    // Start Serial
    Serial.begin(115200);
    Serial.println("--------------------");
    Serial.println("Serial started");

    // Read settings from EEPROM. WIFI settings are stored separately by the ESP
    EEPROM.begin(sizeof(settings)); // Needed on ESP8266 module, as EEPROM lib works a bit differently than on a regular Arduino
    EEPROM.get(0, settings);

   // M5.begin();                        // initiliaze m5stickC Plus library
    pinMode(36,INPUT);    // pin for Potentiometer
    gpio_pulldown_dis(GPIO_NUM_25); // pin 36 and 25 are same on stick5 so to use pin 36 pin 25 should be let afloat
    gpio_pullup_dis(GPIO_NUM_25); // pin 36 and 25 are same on stick5 so to use pin 36 pin 25 should be let afloat

    M5.Lcd.fillScreen(BLACK);          // set screen black at start
    M5.Lcd.setTextColor(WHITE, BLACK); // set the starting text color white and its background black
    M5.Lcd.setRotation(0);             // screen orientation: portrait = 0, inverted portrait = 3, landscape = 2, inv landscape = 4

    // Initialize LED strip
    if (0 < settings.neopixelsAmount && settings.neopixelsAmount <= 1000)
    {
        leds = new CRGB[settings.neopixelsAmount];
        FastLED.addLeds<WS2812B, TALLY_DATA_PIN, GRB>(leds, settings.neopixelsAmount);
        if (settings.neopixelStatusLEDOption != NEOPIXEL_STATUS_NONE)
        {
            numStatusLEDs = 1;
            numTallyLEDs = settings.neopixelsAmount - numStatusLEDs;
            if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST)
            {
                statusLED = leds;
                tallyLEDs = leds + numStatusLEDs;
            }
            else
            { // if last or or other value
                statusLED = leds + numTallyLEDs;
                tallyLEDs = leds;
            }
        }
        else
        {
            numTallyLEDs = settings.neopixelsAmount;
            numStatusLEDs = 0;
            tallyLEDs = leds;
        }
    }
    else
    {
        settings.neopixelsAmount = 0;
        numTallyLEDs = 0;
        numStatusLEDs = 0;
    }
    

    FastLED.setBrightness(settings.neopixelBrightness);
    brightness_value_prev = settings.neopixelBrightness;
    setSTRIP(LED_OFF);
    setStatusLED(LED_BLUE);
    M5.Lcd.fillScreen(BLUE);          // fill screen with blue color
    M5.Lcd.setTextColor(WHITE, BLUE); // set text color white and its background blue
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.drawString("INIT", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2); // display word INIT on screen
    Serial.println("INIT");
    FastLED.show();
    batts();


if(settings.firstEverBoot != 1337)
{ 
  String NaMe = "My Tally Light";
  settings.firstEverBoot = 1337;
  settings.potEnable = false;
  settings.tallyNo = 0;
  NaMe.toCharArray(settings.tallyName, (uint8_t)32);

  String ssid;
  String pwd;
  ssid = "test";
  pwd = "123";

  if (ssid && pwd)
  {
    WiFi.persistent(true); // Needed by ESP8266
    // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
    // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
    WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
  }

  EEPROM.put(0, settings);
  EEPROM.commit();
}


    Serial.println(settings.tallyName);

    if (settings.staticIP && settings.tallyIP != IPADDR_NONE)
    {
        WiFi.config(settings.tallyIP, settings.tallyGateway, settings.tallySubnetMask);
    }
    else
    {
        settings.staticIP = false;
    }

    // Put WiFi into station mode and make it connect to saved network
    WiFi.mode(WIFI_STA);

    WiFi.setHostname(settings.tallyName);

    WiFi.setAutoReconnect(true);
    WiFi.begin();

    Serial.println("------------------------");
    Serial.println("Connecting to WiFi...");
    Serial.println("Network name (SSID): " + getSSID());

    // Initialize and begin HTTP server for handeling the web interface
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.onNotFound(handleNotFound);
    server.begin();

    tallyServer.begin();

    improv.setDeviceInfo(CHIP_FAMILY, DISPLAY_NAME, VERSION, "Tally Light", "");
    improv.onImprovError(onImprovWiFiErrorCb);
    improv.onImprovConnected(onImprovWiFiConnectedCb);

    // Wait for result from first attempt to connect - This makes sure it only activates the softAP if it was unable to connect,
    // and not just because it hasn't had the time to do so yet. It's blocking, so don't use it inside loop()
    unsigned long start = millis();
    while ((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && (millis() - start) < 10000LU)
    {
        bytesAvailable = Serial.available();
        if (bytesAvailable > 0)
        {
            readByte = Serial.read();
            improv.handleByte(readByte);
        }
    }

    // Set state to connecting before entering loop
    changeState(STATE_CONNECTING_TO_WIFI);

#ifdef TALLY_TEST_SERVER
    tallyServer.setTallySources(40);
#endif
}


void updateCountdown() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - startTime;

  // Check for overflow
  if (elapsedTime > countdownDuration) {
    elapsedTime = countdownDuration; // Cap elapsedTime to countdownDuration
  }

  unsigned long remainingTime = countdownDuration - elapsedTime;

  if (remainingTime <= 0) {
    Serial.println("done");
    M5.Lcd.fillScreen(TFT_BLACK); // Clear the screen
    M5.Axp.PowerOff();
    while(1); // Infinite loop to stop further execution
  }
  
  secondsRemaining = remainingTime / 1000; // Convert milliseconds to seconds
}



void loop()
{

    bytesAvailable = Serial.available();
    if (bytesAvailable > 0)
    {
        readByte = Serial.read();
        improv.handleByte(readByte);
    }

        M5.update(); // used to get button pressed state all the time
        
        /// code to control brightness of LED lights via potentiometer
        if(settings.potEnable){
        int val = analogRead(POTENTIOMETER_PIN);
        int brightness_value_post = map(val, 0, 4095, 0, 255);

        if(brightness_value_post != brightness_value_prev){
          FastLED.setBrightness(brightness_value_post);
          brightness_value_prev = brightness_value_post;
          neopixelsUpdated = true;
          //delay(30);
        }
        }
        else{
          //settings.potEnable = false;
          neopixelsUpdated = true;
          }

    switch (state)
    {
    case STATE_CONNECTING_TO_WIFI:
        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.mode(WIFI_STA); // Disable softAP if connection is successful
            Serial.println("------------------------");
            Serial.println("Connected to WiFi:   " + getSSID());
            Serial.println("IP:                  " + WiFi.localIP().toString());
            Serial.println("Subnet Mask:         " + WiFi.subnetMask().toString());
            Serial.println("Gateway IP:          " + WiFi.gatewayIP().toString());
#ifdef TALLY_TEST_SERVER
            Serial.println("Press enter (\\r) to loop through tally states.");
            changeState(STATE_RUNNING);
#else
            changeState(STATE_CONNECTING_TO_SWITCHER);

            if(!settings.staticIP)
            {
              settings.tallyIP = WiFi.localIP();
              settings.tallySubnetMask = WiFi.subnetMask();
              settings.tallyGateway = WiFi.gatewayIP();
              
              }
#endif
        }
        else if (firstRun)
        {
            firstRun = false;
            countdownStarted = false; // Stop the countdown
            Serial.println("Unable to connect. Serving \"Tally Light Setup\" WiFi for configuration, while still trying to connect...");
            WiFi.softAP((String)DISPLAY_NAME + " Setup");
            WiFi.mode(WIFI_AP_STA); // Enable softAP to access web interface in case of no WiFi
            //setSTRIP(LED_WHITE);
            setStatusLED(LED_WHITE);
            M5.Lcd.fillScreen(WHITE);          // set screen white to indicate Access Point Mode
            M5.Lcd.setTextColor(BLACK, WHITE); // text color is set to be black with white background
            M5.Lcd.setTextSize(1);
            M5.Lcd.setTextDatum(MC_DATUM);
            M5.Lcd.drawString("AP Mode", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2); // display word AP Mode on screen
            Serial.println("AP Mode");
            batts();
            startTime = millis();
            countdownStarted = true;         
        }
        break;
#ifndef TALLY_TEST_SERVER
    case STATE_CONNECTING_TO_SWITCHER:
        // Initialize a connection to the switcher:
        if (firstRun)
        {
            atemSwitcher.begin(settings.switcherIP);
            // atemSwitcher.serialOutput(0xff); //Makes Atem library print debug info
            Serial.println("------------------------");
            Serial.println("Connecting to switcher...");
            Serial.println((String) "Switcher IP:         " + settings.switcherIP[0] + "." + settings.switcherIP[1] + "." + settings.switcherIP[2] + "." + settings.switcherIP[3]);
            firstRun = false;
        }
        atemSwitcher.runLoop();
        if (atemSwitcher.isConnected())
        {
            buttStateTesting = 1;
            buttStateTesting2 = 1;
            changeState(STATE_RUNNING);
            Serial.println("Connected to switcher");
        }
        break;
#endif

    case STATE_RUNNING:

        // Handle data exchange and connection to swithcher
        atemSwitcher.runLoop();

        int tallySources = atemSwitcher.getTallyByIndexSources();
        tallyServer.setTallySources(tallySources);
        for (int i = 0; i < tallySources; i++)
        {
            tallyServer.setTallyFlag(i, atemSwitcher.getTallyByIndexTallyFlags(i));
        }

        if (M5.BtnA.wasReleased())
        { // when button A is pressed
            buttonTallyNo = 1; // will be zero until button is pressed atlease 1 time after boot
            buttStateTesting = 1; // button presslogger for led strip
            buttStateTesting2 = 1; // button presslogger for lcd screen
            if (key == tallySources - 1) // compare with total sources avail on switcher
            {
                key = 0; // if reached hightes tally light, start it back from 0
            }
            else if (key < tallySources)
            {
                key++; // increment 1 by 1 to next tally light's status upon button press
            }
            else if (key > tallySources - 1 )
            {
                key = 0; 
            }
        }

        if (buttonTallyNo == 1)
        {
            settings.tallyNo = key;
            EEPROM.put(0, settings);
            EEPROM.commit();
        }
        else
        {
            key = settings.tallyNo;
        }

        // Handle Tally Server
        tallyServer.runLoop();

        // Set LED and Neopixel colors accordingly
        getLcdColor(settings.tallyModeLED2, settings.tallyNo);
        int color = getLedColor(settings.tallyModeLED1, settings.tallyNo);

        if (color != 8)
        {
            setSTRIP(color);
        }

#ifndef TALLY_TEST_SERVER
        // Switch state if ATEM connection is lost...
        if (!atemSwitcher.isConnected())
        { // will return false if the connection was lost
            Serial.println("------------------------");
            Serial.println("Connection to Switcher lost...");
            changeState(STATE_CONNECTING_TO_SWITCHER);

            // Reset tally server's tally flags, so clients turn off their lights.
            tallyServer.resetTallyFlags();
        }
#endif

        // Commented out for userst without batteries - Also timer is not done properly
        //  batteryLoop();
        break;
    }

    // Switch state if WiFi connection is lost...
    if (WiFi.status() != WL_CONNECTED && state != STATE_CONNECTING_TO_WIFI)
    {
        Serial.println("------------------------");
        Serial.println("WiFi connection lost...");
        changeState(STATE_CONNECTING_TO_WIFI);

#ifndef TALLY_TEST_SERVER
        // Force atem library to reset connection, in order for status to read correctly on website.
        atemSwitcher.begin(settings.switcherIP);
        atemSwitcher.connect();
#endif

        // Reset tally server's tally flags, They won't get the message, but it'll be reset for when the connectoin is back.
        tallyServer.resetTallyFlags();
    }

    // Show strip only on updates
  neopixelsUpdated = true;


if (M5.BtnB.wasReleased())
{
    neopixelsUpdated = true;

  }






    
    if (neopixelsUpdated)
    {
        FastLED.show();

        neopixelsUpdated = false;
    }

    // Handle web interface
    server.handleClient();
    batts();
}

// Handle the change of states in the program
void changeState(uint8_t stateToChangeTo)
{
    firstRun = true;
    switch (stateToChangeTo)
    {
    case STATE_CONNECTING_TO_WIFI:
        countdownStarted = false; // Stop the countdown
        state = STATE_CONNECTING_TO_WIFI;
        setStatusLED(LED_BLUE);
        setSTRIP(LED_OFF);
        M5.Lcd.fillScreen(BLUE); // display screen blue as connectin wifi
        M5.Lcd.setTextColor(WHITE, BLUE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("Wifi Connecting", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2); // display word AP on screen
        Serial.println("Wifi Connecting");
        batts();
        startTime = millis();
        countdownStarted = true;   

        break;
    case STATE_CONNECTING_TO_SWITCHER:
        countdownStarted = false; // Stop the countdown
        state = STATE_CONNECTING_TO_SWITCHER;
        setStatusLED(LED_PINK);
        setSTRIP(LED_OFF);
        M5.Lcd.fillScreen(PINK); // display screen pink as connectin switcher
        M5.Lcd.setTextColor(BLACK, PINK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("Switcher Connecting", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2); // display word AP on screen
        Serial.println("Switcher Connecting");
        batts();
        startTime = millis();
        countdownStarted = true;   
        break;
    case STATE_RUNNING:
        state = STATE_RUNNING;
        setStatusLED(LED_ORANGE);
        countdownStarted = false; // Stop the countdown
        startTime = millis(); // Reset the start time
        break;
    }
}

// Set the color of the LED strip, except for the status LED
void setSTRIP(uint8_t color)
{
    if (numTallyLEDs > 0 && tallyLEDs[0] != color_led[color])
    {
        for (int i = 0; i < numTallyLEDs; i++)
        {
            tallyLEDs[i] = color_led[color];
        }
        neopixelsUpdated = true;
    }
}

// Set the single status LED (last LED)
void setStatusLED(uint8_t color)
{
    if (numStatusLEDs > 0 && statusLED[0] != color_led[color])
    {
        for (int i = 0; i < numStatusLEDs; i++)
        {
            statusLED[i] = color_led[color];
            if (color == LED_ORANGE)
            {
                statusLED[i].fadeToBlackBy(0); // when connected to switcher status led will be dimmed by 90% (230/255), so that other LEDs will stand out.
            }
            else
            {
                statusLED[i].fadeToBlackBy(0); // when not connected to switcher status led will take its brightness from the web settings.
            }
        }
        neopixelsUpdated = true;
    }
}

int getTallyState(uint16_t tallyNo)
{
#ifndef TALLY_TEST_SERVER
    if (tallyNo >= atemSwitcher.getTallyByIndexSources())
    { // out of range
        return TALLY_FLAG_OFF;
    }

    uint8_t tallyFlag = atemSwitcher.getTallyByIndexTallyFlags(tallyNo);
#endif
    if (tallyFlag & TALLY_FLAG_PROGRAM)
    {

        return TALLY_FLAG_PROGRAM;
    }
    else if (tallyFlag & TALLY_FLAG_PREVIEW)
    {

        return TALLY_FLAG_PREVIEW;
    }
    else
    {

        return TALLY_FLAG_OFF;
    }
}

int getLedColor(int tallyMode, int tallyNo)
{

    keyR = tallyNo + 1;
    tallyState = getTallyState(tallyNo);


if(tallyMode == MODE_NORMAL)
{
    if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting != 1) || (buttStateTesting == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { // if tally live

       
        prevStateTesting = 1;
        buttStateTesting = 0;
       
        return LED_RED;
    }
    else if ((tallyState == TALLY_FLAG_PREVIEW  && prevStateTesting != 2) || (buttStateTesting == 1 && tallyState == TALLY_FLAG_PREVIEW))
    { // and not program only

      
        prevStateTesting = 2;
        buttStateTesting = 0;
        
        return LED_GREEN;
    }
    else if ((tallyState == TALLY_FLAG_OFF && prevStateTesting != 0) || (buttStateTesting == 1 && tallyState == TALLY_FLAG_OFF))
    { // if tally is neither

       
        prevStateTesting = 0;
        buttStateTesting = 0;
        
        return LED_OFF;
    }
}
  else if (tallyMode == MODE_PREVIEW_STAY_ON)
   {
   
       if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting != 1) || (buttStateTesting == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { 
       
        prevStateTesting = 1;
        buttStateTesting = 0;
        
        return LED_RED;
    }
    else if (((tallyState == TALLY_FLAG_PREVIEW || tallyState == TALLY_FLAG_OFF) && prevStateTesting != 2) || (buttStateTesting == 1 && (tallyState == TALLY_FLAG_PREVIEW || tallyState == TALLY_FLAG_OFF)))
    { 
        
        prevStateTesting = 2;
        buttStateTesting = 0;
       
        return LED_GREEN;
    }

   }

   
  else if (tallyMode == MODE_PROGRAM_ONLY)
   {

    if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting != 1) || (buttStateTesting == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { 

       
        prevStateTesting = 1;
        buttStateTesting = 0;
        
        return LED_RED;
    }
      else if (((tallyState == TALLY_FLAG_OFF || tallyState == TALLY_FLAG_PREVIEW ) && prevStateTesting != 0) || (buttStateTesting == 1 && (tallyState == TALLY_FLAG_OFF || tallyState == TALLY_FLAG_PREVIEW)))
    { 

       
        prevStateTesting = 0;
        buttStateTesting = 0;
        
        return LED_OFF;
    }

   }
   
   
  else if (tallyMode == MODE_ON_AIR)
   {

        if(atemSwitcher.getStreamStreaming()) {
          Serial.println("On Air");
       
            return LED_RED;
            
        }

        Serial.println("OFF");
       
        return LED_OFF;
   }
    
    return 8;
}





void getLcdColor(int tallyMode, int tallyNo)
{

    keyR = tallyNo + 1;
    tallyState = getTallyState(tallyNo);


if(tallyMode == MODE_NORMAL)
{
    if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting2 != 1) || (buttStateTesting2 == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { // if tally live

        // display screen RED with tally light No
        M5.Lcd.fillScreen(MAROON);
        M5.Lcd.setTextColor(WHITE, MAROON);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 1;
        buttStateTesting2 = 0;
        batts();
       
    }
    else if ((tallyState == TALLY_FLAG_PREVIEW  && prevStateTesting2 != 2) || (buttStateTesting2 == 1 && tallyState == TALLY_FLAG_PREVIEW))
    { // and not program only

        // display screen Green with tally light No
        M5.Lcd.fillScreen(DARKGREEN);
        M5.Lcd.setTextColor(WHITE, DARKGREEN);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 2;
        buttStateTesting2 = 0;
        batts();
        
    }
    else if ((tallyState == TALLY_FLAG_OFF && prevStateTesting2 != 0) || (buttStateTesting2 == 1 && tallyState == TALLY_FLAG_OFF))
    { // if tally is neither

        // display screen Grey/inactive with tally light No
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 0;
        buttStateTesting2 = 0;
        batts();
        
    }
}
  else if (tallyMode == MODE_PREVIEW_STAY_ON)
   {
   
       if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting2 != 1) || (buttStateTesting2 == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { 
        // display screen RED with tally light No
        M5.Lcd.fillScreen(MAROON);
        M5.Lcd.setTextColor(WHITE, MAROON);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 1;
        buttStateTesting2 = 0;
        batts();
        
    }
    else if (((tallyState == TALLY_FLAG_PREVIEW || tallyState == TALLY_FLAG_OFF) && prevStateTesting2 != 2) || (buttStateTesting2 == 1 && (tallyState == TALLY_FLAG_PREVIEW || tallyState == TALLY_FLAG_OFF)))
    { 
        // display screen Green with tally light No
        M5.Lcd.fillScreen(DARKGREEN);
        M5.Lcd.setTextColor(WHITE, DARKGREEN);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 2;
        buttStateTesting2 = 0;
        batts();
        
    }

   }

   
  else if (tallyMode == MODE_PROGRAM_ONLY)
   {

    if ((tallyState == TALLY_FLAG_PROGRAM && prevStateTesting2 != 1) || (buttStateTesting2 == 1 && tallyState == TALLY_FLAG_PROGRAM))
    { 

        // display screen RED with tally light No
        M5.Lcd.fillScreen(MAROON);
        M5.Lcd.setTextColor(WHITE, MAROON);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 1;
        buttStateTesting2 = 0;
        batts();
        
    }
      else if (((tallyState == TALLY_FLAG_OFF || tallyState == TALLY_FLAG_PREVIEW ) && prevStateTesting2 != 0) || (buttStateTesting2 == 1 && (tallyState == TALLY_FLAG_OFF || tallyState == TALLY_FLAG_PREVIEW)))
    { 

        // display screen Grey/inactive with tally light No
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(String(keyR), M5.Lcd.width() / 2, M5.Lcd.height() / 2, 8);
        prevStateTesting2 = 0;
        buttStateTesting2 = 0;
        batts();
        
    }

   }
   
   
  else if (tallyMode == MODE_ON_AIR)
   {

        if(atemSwitcher.getStreamStreaming()) {
          Serial.println("On Air");
        M5.Lcd.fillScreen(MAROON);
        M5.Lcd.setTextColor(WHITE, MAROON);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("On Air", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2);
            
            
        }

        Serial.println("OFF");
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("Off Air", M5.Lcd.width() / 2, M5.Lcd.height() / 2, 2);
        
   }
    
    
}















// Serve Setup web page to client, by sending HTML with the correct variables
void handleRoot()
{
    String html = "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><title>Tally Light Setup</title></head><script>function switchIpField(e){console.log(\"switch\");console.log(e);var target=e.srcElement||e.target;var maxLength=parseInt(target.attributes[\"maxlength\"].value,10);var myLength=target.value.length;if(myLength>=maxLength){var next=target.nextElementSibling;if(next!=null){if(next.className.includes(\"IP\")){next.focus();}}}else if(myLength==0){var previous=target.previousElementSibling;if(previous!=null){if(previous.className.includes(\"IP\")){previous.focus();}}}}function ipFieldFocus(e){console.log(\"focus\");console.log(e);var target=e.srcElement||e.target;target.select();}function load(){var containers=document.getElementsByClassName(\"IP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}containers=document.getElementsByClassName(\"tIP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}toggleStaticIPFields();}function toggleStaticIPFields(){var enabled=document.getElementById(\"staticIP\").checked;document.getElementById(\"staticIPHidden\").disabled=enabled;var staticIpFields=document.getElementsByClassName('tIP');for(var i=0;i<staticIpFields.length;i++){staticIpFields[i].disabled=!enabled;}}</script><style>a{color:#0F79E0}</style><body style=\"font-family:Verdana;white-space:nowrap;\"onload=\"load()\"><table cellpadding=\"2\"style=\"width:100%\"><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h1>&nbsp;" +
                  (String)DISPLAY_NAME +
                  " Setup</h1><h2>&nbsp;Status:</h2></td></tr><tr><td><br></td><td></td><td style=\"width:100%;\"></td></tr><tr><td>Connection Status:</td><td colspan=\"2\">";
    switch (WiFi.status())
    {
    case WL_CONNECTED:
        html += "Connected to network";
        break;
    case WL_NO_SSID_AVAIL:
        html += "Network not found";
        break;
    case WL_CONNECT_FAILED:
        html += "Invalid password";
        break;
    case WL_IDLE_STATUS:
        html += "Changing state...";
        break;
    case WL_DISCONNECTED:
        html += "Station mode disabled";
        break;
#if ESP32
    default:
#else
    case -1:
#endif
        html += "Timeout";
        break;
    }

    html += "</td></tr><tr><td>Network name (SSID):</td><td colspan=\"2\">";
    html += getSSID();
    html += "</td></tr><tr><td><br></td></tr><tr><td>Signal strength:</td><td colspan=\"2\">";
    html += WiFi.RSSI();
    html += " dBm</td></tr>";
    // Commented out for users without batteries
    //html += "<tr><td><br></td></tr><tr><td>Battery voltage:</td><td colspan=\"2\">";
    //html += dtostrf(uBatt, 0, 3, buffer);
    //html += " V</td></tr>";

    html += "<tr><td>Static IP:</td><td colspan=\"2\">";
    html += settings.staticIP == true ? "True" : "False";

    html += "</td></tr><tr><td>" +
            (String)DISPLAY_NAME +
            " IP:</td><td colspan=\"2\">";
    html += WiFi.localIP().toString();
    html += "</td></tr><tr><td>Subnet mask: </td><td colspan=\"2\">";
    html += WiFi.subnetMask().toString();
    html += "</td></tr><tr><td>Gateway: </td><td colspan=\"2\">";
    html += WiFi.gatewayIP().toString();
    html += "</td></tr><tr><td><br></td></tr>";
#ifndef TALLY_TEST_SERVER
    html += "<tr><td>ATEM switcher status:</td><td colspan=\"2\">";
    // if (atemSwitcher.hasInitialized())
    //     html += "Connected - Initialized";
    // else
    if (atemSwitcher.isRejected())
        html += "Connection rejected - No empty spot";
    else if (atemSwitcher.isConnected())
        html += "Connected"; // - Wating for initialization";
    else if (WiFi.status() == WL_CONNECTED)
        html += "Disconnected - No response from switcher";
    else
        html += "Disconnected - Waiting for WiFi";
    html += "</td></tr><tr><td>ATEM switcher IP:</td><td colspan=\"2\">";
    html += (String)settings.switcherIP[0] + '.' + settings.switcherIP[1] + '.' + settings.switcherIP[2] + '.' + settings.switcherIP[3];
    html += "</td></tr><tr><td><br></td></tr>";
#endif
    html += "<tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h2>&nbsp;Settings:</h2></td></tr><tr><td><br></td></tr><form action=\"/save\"method=\"post\"><tr><td>Tally Light name: </td><td><input type=\"text\"size=\"30\"maxlength=\"30\"name=\"tName\"value=\"";
#if ESP32
    html += WiFi.getHostname();
#else
    html += WiFi.hostname();
#endif

    html += "\"required/></td></tr><tr><td><br></td></tr><tr><td>Tally Light number: </td><td><input type=\"number\"size=\"5\"min=\"1\"max=\"41\"name=\"tNo\"value=\"";
    html += (settings.tallyNo + 1);

  html += "\"required/></td></tr><tr><td>Tally Light mode (LCD):</td><td><select name=\"tModeLED2\"><option value=\"";
    html += (String) MODE_NORMAL + "\"";
    if (settings.tallyModeLED2 == MODE_NORMAL)
        html += "selected";
    html += ">Normal</option><option value=\"";
    html += (String) MODE_PREVIEW_STAY_ON + "\"";
    if (settings.tallyModeLED2 == MODE_PREVIEW_STAY_ON)
        html += "selected";
    html += ">Preview stay on</option><option value=\"";
    html += (String) MODE_PROGRAM_ONLY + "\"";
    if (settings.tallyModeLED2 == MODE_PROGRAM_ONLY)
        html += "selected";
    html += ">Program only</option><option value=\"";
    html += (String) MODE_ON_AIR + "\"";
    if (settings.tallyModeLED2 == MODE_ON_AIR)
        html += "selected";
    html += ">On Air</option></select></td></tr><tr><td><br></td></tr><tr><td>Tally Light mode (LED Strip):</td><td><select name=\"tModeLED1\"><option value=\"";
    html += (String) MODE_NORMAL + "\"";
    if (settings.tallyModeLED1 == MODE_NORMAL)
        html += "selected";
    html += ">Normal</option><option value=\"";
    html += (String) MODE_PREVIEW_STAY_ON + "\"";
    if (settings.tallyModeLED1 == MODE_PREVIEW_STAY_ON)
        html += "selected";
    html += ">Preview stay on</option><option value=\"";
    html += (String) MODE_PROGRAM_ONLY + "\"";
    if (settings.tallyModeLED1 == MODE_PROGRAM_ONLY)
        html += "selected";
    html += ">Program only</option><option value=\"";
    html += (String)MODE_ON_AIR + "\"";
    if (settings.tallyModeLED1 == MODE_ON_AIR)
        html += "selected";

        

    html += ">On Air</option></select></td></tr>";
     
     html += "<tr><td> Amount of Neopixels:</td><td><input type=\"number\"size=\"5\"min=\"0\"max=\"1000\"name=\"neoPxAmount\"value=\"";
    
    html += settings.neopixelsAmount;
    html += "\"required/></td></tr><tr><td>Neopixel status LED: </td><td><select name=\"neoPxStatus\"><option value=\"";
    html += (String)NEOPIXEL_STATUS_FIRST + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST)
        html += "selected";
    html += ">First LED</option><option value=\"";
    html += (String)NEOPIXEL_STATUS_LAST + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_LAST)
        html += "selected";
    html += ">Last LED</option><option value=\"";
    html += (String)NEOPIXEL_STATUS_NONE + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_NONE)
        html += "selected";
    html += ">None</option></select></td></tr>";

    html += "<tr><td>Use Potentiometer: </td><td><input type=\"hidden\"id=\"potEnableHidden\"name=\"potEnable\"value=\"false\"/><input id=\"potEnable\"type=\"checkbox\"name=\"potEnable\"value=\"true\"";
    if (settings.potEnable){
      html += "checked";
    }
    html += "/></td></tr>";


    html += "<td> Neopixel brightness: </td><td><input type=\"number\"size=\"5\"min=\"0\"max=\"255\"name=\"neoPxBright\"value=\"";
    html += settings.neopixelBrightness;
    html += "\"required/></td></tr><tr><td><br></td></tr><tr><td>Network name(SSID): </td><td><input type =\"text\"size=\"30\"maxlength=\"30\"name=\"ssid\"value=\"";
    html += getSSID();
    html += "\"required/></td></tr><tr><td>Network password: </td><td><input type=\"password\"size=\"30\"maxlength=\"30\"name=\"pwd\"pattern=\"^$|.{8,32}\"value=\"";
    if (WiFi.isConnected()) // As a minimum security meassure, to only send the wifi password if it's currently connected to the given network.
        html += WiFi.psk();

//<tr><td><br></td></tr>

    html += "\"/></td></tr>";  




    //html += "/></td></tr>"
    html += "<tr><td><br></td></tr><tr><td>Use static IP: </td><td><input type=\"hidden\"id=\"staticIPHidden\"name=\"staticIP\"value=\"false\"/><input id=\"staticIP\"type=\"checkbox\"name=\"staticIP\"value=\"true\"onchange=\"toggleStaticIPFields()\"";
    if (settings.staticIP)
        html += "checked";
    html += "/></td></tr><tr><td>" +
            (String)DISPLAY_NAME +
            " IP: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"tIP1\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyIP[0];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"tIP2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyIP[1];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"tIP3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyIP[2];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"tIP4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyIP[3];
    html += "\"required/></td></tr><tr><td>Subnet mask: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask1\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallySubnetMask[0];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallySubnetMask[1];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallySubnetMask[2];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallySubnetMask[3];
    html += "\"required/></td></tr><tr><td>Gateway: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate1\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyGateway[0];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyGateway[1];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyGateway[2];
    html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.tallyGateway[3];
    html += "\"required/></td></tr>";
#ifndef TALLY_TEST_SERVER
    html += "<tr><td><br></td></tr><tr><td>ATEM switcher IP: </td><td><input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP1\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[0];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[1];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[2];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[3];
    html += "\"required/></tr>";
#endif
    html += "<tr><td><br></td></tr><tr><td/><td style=\"float: right;\"><input type=\"submit\"value=\"Save Changes\"/></td></tr></form><tr bgcolor=\"#cccccc\"style=\"font-size: .8em;\"><td colspan=\"3\"><p>&nbsp;<a href=\"https://github.com/zitlem\">Zitlem</a></p><p>&nbsp;Based on ATEM libraries for Arduino by <a href=\"https://www.skaarhoj.com/\">SKAARHOJ</a></p><p>&nbsp;<a href=\"https://github.com/AronHetLam\">Credit to Aron N. Het Lam</a></p></td></tr></table></body></html>";
    server.send(200, "text/html", html);
}

// Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>Tally Light Setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;" + (String)DISPLAY_NAME + " Setup</h1></td></tr></table><br>Request without posting settings not allowed</body></html>");
    }
    else
    {
        String ssid;
        String pwd;
        bool change = false;
        for (uint8_t i = 0; i < server.args(); i++)
        {
            change = true;
            String var = server.argName(i);
            String val = server.arg(i);

            if (var == "tName")
            {
                val.toCharArray(settings.tallyName, (uint8_t)32);
            }
            else if (var == "tModeLED1") 
            {
                settings.tallyModeLED1 = val.toInt();
            }
            else if (var == "tModeLED2") 
            {
                settings.tallyModeLED2 = val.toInt();
            }
            else if (var == "neoPxAmount")
            {
                settings.neopixelsAmount = val.toInt();
            }
            else if (var == "neoPxStatus")
            {
                settings.neopixelStatusLEDOption = val.toInt();
            }
            else if (var == "neoPxBright")
            {
                settings.neopixelBrightness = val.toInt();
            }
            else if (var == "tNo")
            {
                settings.tallyNo = val.toInt() - 1;
            }
            else if (var == "ssid")
            {
                ssid = String(val);
            }
            else if (var == "pwd")
            {
                pwd = String(val);
            }
            else if (var == "potEnable") 
            {
                settings.potEnable = (val == "true");
            } 
            else if (var == "staticIP")
            {
                settings.staticIP = (val == "true");
            }
            else if (var == "tIP1")
            {
                settings.tallyIP[0] = val.toInt();
            }
            else if (var == "tIP2")
            {
                settings.tallyIP[1] = val.toInt();
            }
            else if (var == "tIP3")
            {
                settings.tallyIP[2] = val.toInt();
            }
            else if (var == "tIP4")
            {
                settings.tallyIP[3] = val.toInt();
            }
            else if (var == "mask1")
            {
                settings.tallySubnetMask[0] = val.toInt();
            }
            else if (var == "mask2")
            {
                settings.tallySubnetMask[1] = val.toInt();
            }
            else if (var == "mask3")
            {
                settings.tallySubnetMask[2] = val.toInt();
            }
            else if (var == "mask4")
            {
                settings.tallySubnetMask[3] = val.toInt();
            }
            else if (var == "gate1")
            {
                settings.tallyGateway[0] = val.toInt();
            }
            else if (var == "gate2")
            {
                settings.tallyGateway[1] = val.toInt();
            }
            else if (var == "gate3")
            {
                settings.tallyGateway[2] = val.toInt();
            }
            else if (var == "gate4")
            {
                settings.tallyGateway[3] = val.toInt();
            }
            else if (var == "aIP1")
            {
                settings.switcherIP[0] = val.toInt();
            }
            else if (var == "aIP2")
            {
                settings.switcherIP[1] = val.toInt();
            }
            else if (var == "aIP3")
            {
                settings.switcherIP[2] = val.toInt();
            }
            else if (var == "aIP4")
            {
                settings.switcherIP[3] = val.toInt();
            }
        }

        if (change)
        {
            EEPROM.put(0, settings);
            EEPROM.commit();

            server.send(200, "text/html", (String) "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>Tally Light Setup</title></head><body><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"font-family:Verdana;color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;" + (String)DISPLAY_NAME + " Setup</h1></td></tr></table><br>Settings saved successfully.</body></html>");

            // Delay to let data be saved, and the response to be sent properly to the client
            server.close(); // Close server to flush and ensure the response gets to the client
            delay(100);

            // Change into STA mode to disable softAP
            WiFi.mode(WIFI_STA);
            delay(100); // Give it time to switch over to STA mode (this is important on the ESP32 at least)

            if (ssid && pwd)
            {
                WiFi.persistent(true); // Needed by ESP8266
                // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
                // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
                WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
            }

            // Delay to apply settings before restart
            delay(100);
            ESP.restart();
        }
    }
}

// Send 404 to client in case of invalid webpage being requested.
void handleNotFound()
{
    server.send(404, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>" + (String)DISPLAY_NAME + " Setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp Tally Light Setup</h1></td></tr></table><br>404 - Page not found</body></html>");
}

String getSSID()
{
#if ESP32
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<const char *>(conf.sta.ssid));
#else
    return WiFi.SSID();
#endif
}


void batts()
{
    float voltage = M5.Axp.GetBatVoltage();
    float current = M5.Axp.GetBatCurrent();
    float batPercentage = (voltage < 3.2) ? 0 : (voltage - 3.2) * 100;

    char batteryStatus[9];
    char chargingIcon = current == 0 ? ' ' : (current > 0 ? '+' : '-');
    sprintf(batteryStatus, "%c %.2f%%", chargingIcon, batPercentage);
    M5.Lcd.setTextDatum(TR_DATUM);
    M5.Lcd.drawString(batteryStatus, M5.Lcd.width() - 1, 1, 1);


  if (countdownStarted) {
    updateCountdown(); // Call the function to update the countdown timer
    char timebuffer[7]; // Buffer for the formatted string, 7 characters including extra spaces
    sprintf(timebuffer, "  %3d  ", secondsRemaining); // Format secondsRemaining with extra spaces
    //Serial.println(secondsRemaining);
    M5.Lcd.drawString(timebuffer, (M5.Lcd.width() / 2) + (M5.Lcd.textWidth(String(timebuffer))/2), M5.Lcd.height() / 4 + M5.Lcd.height() / 2, 2); 
  }

}
