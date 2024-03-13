#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial

// Debug Level from 0 to 4
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3

//DHCP option is turned ON below
// Select the IP address according to your local network
IPAddress myIP(192, 168, 10, 31);
IPAddress myGW(192, 168, 10, 1);
IPAddress mySN(255, 255, 255, 0);
// Google DNS Server IP
IPAddress myDNS(8, 8, 8, 8);

#ifndef VERSION
#define VERSION "dev"
#endif

#define FASTLED_ALLOW_INTERRUPTS 0

#ifndef CHIP_FAMILY
#define CHIP_FAMILY "Unknown"
#endif

#ifndef VERSION
#define VERSION "Unknown"
#endif

#define DISPLAY_NAME "Tally Light"



//Include libraries:
#include <WebServer_WT32_ETH01.h>
#include <EEPROM.h>
#include <ATEMmin.h>
#include <TallyServer.h>
#include <FastLED.h>



//Define LED colors
#define LED_OFF     0
#define LED_RED     1
#define LED_GREEN   2
#define LED_BLUE    3
#define LED_YELLOW  4
#define LED_PINK    5
#define LED_WHITE   6
#define LED_ORANGE  7

//Map "old" LED colors to CRGB colors
CRGB color_led[8] = { CRGB::Black, CRGB::Red, CRGB::Lime, CRGB::Blue, CRGB::Yellow, CRGB::HotPink , CRGB::White, CRGB::Orange };

//Define states
#define STATE_STARTING                  0
#define STATE_CONNECTING_TO_ETH         1
#define STATE_CONNECTING_TO_SWITCHER    2
#define STATE_RUNNING                   3

//Define modes of operation
#define MODE_NORMAL                     1
#define MODE_PREVIEW_STAY_ON            2
#define MODE_PROGRAM_ONLY               3
#define MODE_ON_AIR                     4

#define TALLY_FLAG_OFF                  0
#define TALLY_FLAG_PROGRAM              1
#define TALLY_FLAG_PREVIEW              2

//Define Neopixel status-LED options
#define NEOPIXEL_STATUS_FIRST           1
#define NEOPIXEL_STATUS_LAST            2
#define NEOPIXEL_STATUS_NONE            3

//FastLED Pin
#define DATA_PIN    04

int tallyTest = 0;
int loopcounter = 0;

int numTallyLEDs;
int numStatusLEDs;
CRGB *leds;
CRGB *tallyLEDs;
CRGB *statusLED;
bool neopixelsUpdated = false;


//Initialize global variables

WebServer server(80);
ATEMmin atemSwitcher;

TallyServer tallyServer;


uint8_t state = STATE_STARTING;



//Define sturct for holding tally settings (mostly to simplify EEPROM read and write, in order to persist settings)
struct Settings {
  
    
    bool staticIP;
    IPAddress tallyIP;
    IPAddress tallySubnetMask;
    IPAddress tallyGateway;
    IPAddress switcherIP;
    uint16_t neopixelsAmount;
    uint8_t neopixelStatusLEDOption;
    uint8_t neopixelBrightness;
    uint8_t ledBrightness;
    char tallyName[32] = "";    
};

Settings settings;

bool firstRun = true;



//naming functions that are to be used in program
void changeState(uint8_t stateToChangeTo);     //Handle the change of states in the program
void setSTRIP(uint8_t color, uint16_t tallyNo); //Set the LED Strip array
void setStatusLED(uint8_t color);              //Set Status led on LED Strip
int getTallyState(uint16_t tallyNo);           //Get the tally state from Atem switcher
int getLedColor(uint16_t tallyNo);   //Get color according to Atem switcher mode
void handleRoot();                             //handle html page
void handleSave();                             //set values in Eeprom from webpage
void handleNotFound();                         //if server not avail 404


//Perform initial setup on power on
void setup() {

    //Start Serial
    Serial.begin(115200);
    Serial.println("########################");
    Serial.println("Serial started");

    //Read settings from EEPROM. Nerwork settings are stored seperately by the ESP
    EEPROM.begin(sizeof(settings)); //Needed on ESP32 module, as EEPROM lib works a bit differently than on a regular arduino
    EEPROM.get(0, settings);

    //Initialize LED strip
    if (0 < settings.neopixelsAmount && settings.neopixelsAmount <= 1000) { 
       leds = new CRGB[settings.neopixelsAmount]; //pointer used for array reference
        FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, settings.neopixelsAmount);

        if (settings.neopixelStatusLEDOption != NEOPIXEL_STATUS_NONE) {
            numStatusLEDs = 1;
            numTallyLEDs = settings.neopixelsAmount - numStatusLEDs;
            if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST) {
                statusLED = leds;                     //pointer used
                tallyLEDs = leds + numStatusLEDs;     //pointer used
            } else { // if last or or other value
                statusLED = leds + numTallyLEDs;
                tallyLEDs = leds;
            }
        } else {
            numTallyLEDs = settings.neopixelsAmount;
            numStatusLEDs = 0;
            tallyLEDs = leds;
        }
    } else {
        settings.neopixelsAmount = 0;
        numTallyLEDs = 0;
        numStatusLEDs = 0;
    }

    FastLED.setBrightness(settings.neopixelBrightness);
    setSTRIP(LED_OFF, 99);
    setStatusLED(LED_BLUE);
    FastLED.show();




  Serial.print("\nStarting HelloServer on " + String(ARDUINO_BOARD));
  Serial.println(" with " + String(SHIELD_TYPE));
  Serial.println(WEBSERVER_WT32_ETH01_VERSION);
  // To be called before ETH.begin()
  WT32_ETH01_onEvent();
  //bool begin(uint8_t phy_addr=ETH_PHY_ADDR, int power=ETH_PHY_POWER, int mdc=ETH_PHY_MDC, int mdio=ETH_PHY_MDIO,
  //           eth_phy_type_t type=ETH_PHY_TYPE, eth_clock_mode_t clk_mode=ETH_CLK_MODE);
  //ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
 //  ETH.config(myIP, myGW, mySN, myDNS);
  if (settings.staticIP && settings.tallyIP != IPADDR_NONE) {  
  ETH.config(settings.tallyIP, settings.tallyGateway, settings.tallySubnetMask, myDNS);
    } else {
        settings.staticIP = false;
    }
    
  WT32_ETH01_waitForConnect();
  Serial.print(F("HTTP EthernetWebServer is @ IP : "));
  Serial.println(ETH.localIP());



    // Initialize and begin HTTP server for handeling the web interface
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.onNotFound(handleNotFound);
    server.begin();
    tallyServer.begin();          
    changeState(STATE_CONNECTING_TO_ETH);
     
}  


void loop() { 

    switch (state) {
        case STATE_CONNECTING_TO_ETH:
        
                changeState(STATE_CONNECTING_TO_SWITCHER);

            break;    

        case STATE_CONNECTING_TO_SWITCHER:
            // Initialize a connection to the switcher:
            if (firstRun) {
                atemSwitcher.begin(settings.switcherIP);
                
                Serial.println("------------------------");
                Serial.println("Connecting to switcher...");
                Serial.println((String)"Switcher IP:         " + settings.switcherIP[0] + "." + settings.switcherIP[1] + "." + settings.switcherIP[2] + "." + settings.switcherIP[3]);
                firstRun = false;
            }
            atemSwitcher.runLoop();
            if (atemSwitcher.isConnected()) {
                changeState(STATE_RUNNING);
                Serial.println("Connected to switcher");
            }
            break;


        case STATE_RUNNING:      

            //Handle data exchange and connection to swithcher
            atemSwitcher.runLoop();



            int tallySources = atemSwitcher.getTallyByIndexSources();
            tallyTest = tallySources;
            tallyServer.setTallySources(tallySources);
            for (int i = 0; i < tallySources; i++) {
                tallyServer.setTallyFlag(i, atemSwitcher.getTallyByIndexTallyFlags(i));
            }

            //Handle Tally Server
            tallyServer.runLoop();

            //Set Neopixel colors accordingly
            for(uint16_t j = 0; j < tallyTest; j++)
            {
            int color = getLedColor(j);
            setSTRIP(color, j);          
            }
           //Serial.println("-----------NEXT-STATE-------------");
            

            //Switch state if ATEM connection is lost...
            if (!atemSwitcher.isConnected()) { // will return false if the connection was lost
                Serial.println("------------------------");
                Serial.println("Connection to Switcher lost...");
                changeState(STATE_CONNECTING_TO_SWITCHER);

                //Reset tally server's tally flags, so clients turn off their lights.
                tallyServer.resetTallyFlags();
            }
           
            break;
    }



    //Show strip only on updates
    if(neopixelsUpdated) {
        FastLED.show();
        //Serial.println("Show LED Strip");
        neopixelsUpdated = false;
    }


    //Handle web interface
    server.handleClient();


  if(loopcounter == 5000)
  {
    //Serial.println("Loop Running");
    //Serial.print("Tally Sources: ");
    //Serial.println(tallyTest);
    loopcounter = 0;
    }

    loopcounter++;
}


void changeState(uint8_t stateToChangeTo) {
    firstRun = true;
    switch (stateToChangeTo) {
        case STATE_CONNECTING_TO_ETH:
            state = STATE_CONNECTING_TO_ETH;
            setStatusLED(LED_BLUE);
            setSTRIP(LED_OFF, 99);
            break; 
        case STATE_CONNECTING_TO_SWITCHER:
            state = STATE_CONNECTING_TO_SWITCHER;
            setStatusLED(LED_PINK);
            setSTRIP(LED_OFF, 99);
            break;
        case STATE_RUNNING:
            state = STATE_RUNNING;
            setStatusLED(LED_ORANGE);
            break;
    }
}



//Set the color of the LED strip, except for the status LED
void setSTRIP(uint8_t color,uint16_t tallyNo) {
     //   Serial.println("set strip 01");
        if(tallyNo == 99)
        {
 
      if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST){
           for(int i = 1; i <= numTallyLEDs; i++){
            leds[i] = color_led[LED_OFF];
     //       delay(30);
            }
      }
      
      if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_LAST){
           for(int i = 0; i < numTallyLEDs; i++){
            leds[i] = color_led[LED_OFF];
       //     delay(30);
            }        
      }
      
      if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_NONE){
           for(int i = 0; i <= numTallyLEDs; i++){
            leds[i] = color_led[LED_OFF];
       //     delay(30);
            }        
      }          
          }
          else {

          int temp;
           if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST) {
           temp = tallyNo +1;
           }
           else{
            temp = tallyNo;
           }
           
        leds[temp] = color_led[color];

          }
        neopixelsUpdated = true;
 //       Serial.println("set strip 02");
    }



//Set the single status LED (last LED)
void setStatusLED(uint8_t color) {
    if (numStatusLEDs > 0 && statusLED[0] != color_led[color]) {
        for (int i = 0; i < numStatusLEDs; i++) {
            statusLED[i] = color_led[color];
            if (color == LED_ORANGE) {
                statusLED[i].fadeToBlackBy(0); // when connected to switcher status led will be dimmed by 0% (0/255), so that other LEDs will stand out.
            } else {
                statusLED[i].fadeToBlackBy(0);  // when not connected to switcher status led will take its brightness from the web settings.
            }
        }
        neopixelsUpdated = true;
    }
}


int getTallyState(uint16_t tallyNo) {
    if(tallyNo >= atemSwitcher.getTallyByIndexSources()) { //out of range
     //   Serial.println("getTallyState OUT OF RANGE");
        return TALLY_FLAG_OFF;
    }

    uint8_t tallyFlag = atemSwitcher.getTallyByIndexTallyFlags(tallyNo);
    if (tallyFlag & TALLY_FLAG_PROGRAM) {
  //    Serial.println("getTallyState TALLY_FLAG_PROGRAM");
        return TALLY_FLAG_PROGRAM;
    } else if (tallyFlag & TALLY_FLAG_PREVIEW) {
  //    Serial.println("getTallyState TALLY_FLAG_PREVIEW");
        return TALLY_FLAG_PREVIEW;
    } else {
  //    Serial.println("getTallyState TALLY_FLAG_OFF");
        return TALLY_FLAG_OFF;
    }
}


int getLedColor(uint16_t tallyNo) {
    
     //   if(atemSwitcher.getStreamStreaming()) {
     //       return LED_RED;
     //   }
       
      // return LED_OFF;
        

    int tallyState = getTallyState(tallyNo);

    if (tallyState == TALLY_FLAG_PROGRAM) {             //if tally live
    //   Serial.println("getLedColor RED");
        return LED_RED;
    } else if (tallyState == TALLY_FLAG_PREVIEW ) {     //and not program only
    //    Serial.println("getLedColor GREEN");
        return LED_GREEN;
    } else {                                            //if tally is neither
    //   Serial.println("getLedColor OFF");
        return LED_OFF;
    }
}


//Serve setup web page to client, by sending HTML with the correct variables
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><title>Tally Light setup</title></head><script>function switchIpField(e){console.log(\"switch\");console.log(e);var target=e.srcElement||e.target;var maxLength=parseInt(target.attributes[\"maxlength\"].value,10);var myLength=target.value.length;if(myLength>=maxLength){var next=target.nextElementSibling;if(next!=null){if(next.className.includes(\"IP\")){next.focus();}}}else if(myLength==0){var previous=target.previousElementSibling;if(previous!=null){if(previous.className.includes(\"IP\")){previous.focus();}}}}function ipFieldFocus(e){console.log(\"focus\");console.log(e);var target=e.srcElement||e.target;target.select();}function load(){var containers=document.getElementsByClassName(\"IP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}containers=document.getElementsByClassName(\"tIP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}toggleStaticIPFields();}function toggleStaticIPFields(){var enabled=document.getElementById(\"staticIP\").checked;document.getElementById(\"staticIPHidden\").disabled=enabled;var staticIpFields=document.getElementsByClassName('tIP');for(var i=0;i<staticIpFields.length;i++){staticIpFields[i].disabled=!enabled;}}</script><style>a{color:#0F79E0}</style><body style=\"font-family:Verdana;white-space:nowrap;\"onload=\"load()\"><table cellpadding=\"2\"style=\"width:100%\"><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h1>&nbsp;Tally Light setup</h1><h2>&nbsp;Status:</h2></td></tr><tr><td><br></td><td></td><td style=\"width:100%;\"></td></tr><tr><td colspan=\"2\">";

   // html += "Connected to network";


    html += "<tr><td>Static IP:</td><td colspan=\"2\">";
    html += settings.staticIP == true ? "True" : "False";
    
    html += "</td></tr><tr><td>Tally Light IP:</td><td colspan=\"2\">";
    html += ETH.localIP().toString();
    html += "</td></tr><tr><td>Subnet mask: </td><td colspan=\"2\">";
    html += ETH.subnetMask().toString();
    html += "</td></tr><tr><td>Gateway: </td><td colspan=\"2\">";
    html += ETH.gatewayIP().toString();
   
    html += "</td></tr><tr><td><br></td></tr><tr><td>ATEM switcher status:</td><td colspan=\"2\">";
    // if (atemSwitcher.hasInitialized())
    //     html += "Connected - Initialized";
    // else
    if (atemSwitcher.isRejected())
        html += "Connection rejected - No empty spot";
    else if (atemSwitcher.isConnected())
        html += "Connected"; // - Wating for initialization";
    else 
        html += "Disconnected - No response from switcher";
    //else
    //    html += "Disconnected - Waiting for WiFi";
    html += "</td></tr><tr><td>ATEM switcher IP:</td><td colspan=\"2\">";
    html += (String)settings.switcherIP[0] + '.' + settings.switcherIP[1] + '.' + settings.switcherIP[2] + '.' + settings.switcherIP[3];
    html += "</td></tr><tr><td><br></td></tr>";

    html += "<tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h2>&nbsp;Settings:</h2></td></tr><tr><td><br></td></tr><form action=\"/save\"method=\"post\"><tr><td>Tally Light name: </td><td><input type=\"text\"size=\"30\"maxlength=\"30\"name=\"tName\"value=\"";
 
    
    html += settings.tallyName;
  
    
    html += "\"required/></td></tr><tr><td><br></td></tr><tr><td>Amount of LED pixels:</td><td><input type=\"number\"size=\"5\"min=\"0\"max=\"1000\"name=\"neoPxAmount\"value=\"";
    html += settings.neopixelsAmount;
    html += "\"required/></td></tr><tr><td>LED pixel status LED: </td><td><select name=\"neoPxStatus\"><option value=\"";
    html += (String) NEOPIXEL_STATUS_FIRST + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST)
        html += "selected";
    html += ">First LED</option><option value=\"";
    html += (String) NEOPIXEL_STATUS_LAST + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_LAST)
        html += "selected";
    html += ">Last LED</option><option value=\"";
    html += (String) NEOPIXEL_STATUS_NONE + "\"";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_NONE)
        html += "selected";
    html += ">None</option></select></td></tr><tr><td> LED pixel brightness: </td><td><input type=\"number\"size=\"5\"min=\"0\"max=\"255\"name=\"neoPxBright\"value=\"";
    html += settings.neopixelBrightness;
    html += "\"/></td></tr><tr><td><br></td></tr><tr><td>Use static IP: </td><td><input type=\"hidden\"id=\"staticIPHidden\"name=\"staticIP\"value=\"false\"/><input id=\"staticIP\"type=\"checkbox\"name=\"staticIP\"value=\"true\"onchange=\"toggleStaticIPFields()\"";
    if (settings.staticIP)
        html += "checked";
    html += "/></td></tr><tr><td>Tally Light IP: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"tIP1\"pattern=\"\\d{0,3}\"value=\"";
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

    html += "<tr><td><br></td></tr><tr><td>ATEM switcher IP: </td><td><input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP1\"pattern=\"\\d{0,3}\"value=\"";
     html += settings.switcherIP[0];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[1];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[2];
    html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.switcherIP[3];
    html += "\"required/></tr>";

    
    html += "<tr><td><br></td></tr><tr><td/><td style=\"float: right;\"><input type=\"submit\"value=\"Save Changes\"/></td></tr></form><tr bgcolor=\"#cccccc\"style=\"font-size: .8em;\"><td colspan=\"3\"><p>&nbsp;<a href=\"https://github.com/zitlem\">Zitlem</a></p><p>&nbsp;Based on ATEM libraries for Arduino by <a href=\"https://www.skaarhoj.com/\">SKAARHOJ</a></p><p>&nbsp;<a href=\"https://github.com/AronHetLam\">Credit to Aron N. Het Lam</a></p></td></tr></table></body></html>";
    
    server.send(200, "text/html", html);
}

//Save new settings from client in EEPROM and restart module
void handleSave() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>Tally Light setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;" +
    (String)DISPLAY_NAME +
    " setup</h1></td></tr></table><br>Request without posting settings not allowed</body></html>");
    } else {

        bool change = false;
        for (uint8_t i = 0; i < server.args(); i++) {
            change = true;
            String var = server.argName(i);
            String val = server.arg(i);

            if (var == "tName") {
                val.toCharArray(settings.tallyName, (uint8_t)32);
            }else  if (var == "neoPxAmount") {
                settings.neopixelsAmount = val.toInt();
            } else if (var == "neoPxStatus") {
                settings.neopixelStatusLEDOption = val.toInt();
            } else if (var == "neoPxBright") {
                settings.neopixelBrightness = val.toInt();
            } else if (var == "staticIP") {
                settings.staticIP = (val == "true");
            } else if (var == "tIP1") {
                settings.tallyIP[0] = val.toInt();
            } else if (var == "tIP2") {
                settings.tallyIP[1] = val.toInt();
            } else if (var == "tIP3") {
                settings.tallyIP[2] = val.toInt();
            } else if (var == "tIP4") {
                settings.tallyIP[3] = val.toInt();
            } else if (var == "mask1") {
                settings.tallySubnetMask[0] = val.toInt();
            } else if (var == "mask2") {
                settings.tallySubnetMask[1] = val.toInt();
            } else if (var == "mask3") {
                settings.tallySubnetMask[2] = val.toInt();
            } else if (var == "mask4") {
                settings.tallySubnetMask[3] = val.toInt();
            } else if (var == "gate1") {
                settings.tallyGateway[0] = val.toInt();
            } else if (var == "gate2") {
                settings.tallyGateway[1] = val.toInt();
            } else if (var == "gate3") {
                settings.tallyGateway[2] = val.toInt();
            } else if (var == "gate4") {
                settings.tallyGateway[3] = val.toInt();
            } else if (var == "aIP1") {
                settings.switcherIP[0] = val.toInt();
            } else if (var == "aIP2") {
                settings.switcherIP[1] = val.toInt();
            } else if (var == "aIP3") {
                settings.switcherIP[2] = val.toInt();
            } else if (var == "aIP4") {
                settings.switcherIP[3] = val.toInt();
            }
        }

        if (change) {
            EEPROM.put(0, settings);
            EEPROM.commit();

            server.send(200, "text/html", (String)"<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>Tally Light setup</title></head><body><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"font-family:Verdana;color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;Tally Light setup</h1></td></tr></table><br>Settings saved successfully.</body></html>");

            // Delay to let data be saved, and the response to be sent properly to the client
            server.close(); // Close server to flush and ensure the response gets to the client
            delay(100);


            //Delay to apply settings before restart
            delay(100);
            ESP.restart();
        }
    }
}

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound() {
    server.send(404, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>Tally Light setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp Tally Light setup</h1></td></tr></table><br>404 - Page not found</body></html>");
}
                  
