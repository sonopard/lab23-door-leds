#include <FS.h> 

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <DNSServer.h>
#include <ESPAsyncWiFiManager.h>     
#include <NeoPixelBus.h>
#include <ArduinoJson.h>

#define HOSTNAME_PREFIX "door-leds-1"

#define NUM_LEDS (134+134+134+134)
#define SATURATION 16

//      ====|====
//      ====|====
//      ====|====
//      |=======|
//      |AUSGANG|
//      |=======|
// 
// L_L   ^ -> R_L  ????? not sure anymore tbh
//  |    |     |
//  |    |     |
//  |    |     |
//  \ > L_R    v
//
#define LED_INDEX_SEG_L_L 0
#define LED_INDEX_SEG_L_R (LED_INDEX_SEG_L_L + 134)
#define LED_INDEX_SEG_R_L (LED_INDEX_SEG_L_R + 134)
#define LED_INDEX_SEG_R_R (LED_INDEX_SEG_R_L + 134)

#define UDP_PORT 2342
#define FRAMES_PER_SECOND 120
#define PKT_MAX_LEN 24

typedef void (*anim_fn_t) (uint8_t *fps);

NeoPixelBus<NeoGrbwFeature, NeoEsp8266Dma800KbpsMethod> strip(NUM_LEDS);
anim_fn_t anim_fn = NULL;
uint8_t fps = FRAMES_PER_SECOND;

RgbColor red(SATURATION, 0, 0);
RgbColor green(0, SATURATION, 0);
RgbColor blue(0, 0, SATURATION);
RgbColor white(SATURATION);
RgbColor black(0);

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);


WiFiUDP cmd_udp;
uint8_t pkt_data[PKT_MAX_LEN];
char resp_data[2];
char cb[200];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

AsyncWebServer server(80);
DNSServer dns;

void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    Serial.println();
    Serial.println("Initializing...");
    Serial.flush();
  // construct hostname string
  String hostname = String(strlen(HOSTNAME_PREFIX) + 6 + 1);
  hostname = HOSTNAME_PREFIX;
  /*hostname += WiFi.macAddress().substring(6,8);
  hostname += WiFi.macAddress().substring(9,11);
  hostname += WiFi.macAddress().substring(12,14);
  hostname += WiFi.macAddress().substring(15,17);*/
  hostname.toCharArray(cb, sizeof(cb));

  Serial.print("I am ");
  Serial.println(hostname);
  
  WiFi.hostname(hostname);
  
  AsyncWiFiManager wifiManager(&server,&dns);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // uncomment to reset saved settings
  // wifiManager.resetSettings();

  // fetches ssid and key from eeprom/flash and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(cb);
  Serial.println("connected...yeey :)");
  
  // print this client's IP on the UART for debugging purposes
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  cmd_udp.begin(UDP_PORT);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "I am door-leds-1.");
  });

  AsyncElegantOTA.begin(&server);
  server.begin();

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();


}

void strobe_fn(uint8_t *fps) {
  *fps=2;
  static int x = 0;
  x++;
  HslColor color = hslWhite;
  if(x % 2) {
    for (int p = LED_INDEX_SEG_L_L; p < LED_INDEX_SEG_L_R; p++) {
      strip.SetPixelColor(p, color);
    }
    for (int p = LED_INDEX_SEG_L_R; p < LED_INDEX_SEG_R_R; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
  } else { 
    for (int p = LED_INDEX_SEG_L_L; p < LED_INDEX_SEG_L_R; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
    for (int p = LED_INDEX_SEG_L_R; p < LED_INDEX_SEG_R_R; p++) {
      strip.SetPixelColor(p, color);
    }    
  }
}

void rainbow_fn(uint8_t *fps) {
  *fps=60;
  static uint16_t hue;
  float compression=3;
  int huestep=1;
  if(hue>=NUM_LEDS)
   hue=0;
 
  for(uint16_t i = 0; i < NUM_LEDS; i++) {
    HslColor color = HslColor((float)((i+hue) % (int)(NUM_LEDS/compression)) / ((float)NUM_LEDS/compression), 0.85, (float)SATURATION / 255.0f);
    strip.SetPixelColor(i, color);
  }
  hue+=huestep;
}

void off_fn(uint8_t *fps) {
  for(int i = 0; i < NUM_LEDS; i++){
    strip.SetPixelColor(i, hslBlack);
  }
}

void net_parse() {
  // receive buffer write position
  static uint16_t txt_data_index = 0;

  // handle incoming UDP packets
  if (cmd_udp.parsePacket()) {
    // copy data into receive buffer
    // increment receive buffer write position by amount of data received
    // only consume as many bytes from the packet buffer as the receive buffer can contain.
    txt_data_index += cmd_udp.read(&(pkt_data[txt_data_index]), PKT_MAX_LEN - txt_data_index);
  }

  if(txt_data_index > 0) {
    switch(pkt_data[0]) {
      case 'r':
        resp_data[0] = 'r';
        anim_fn = rainbow_fn;
        break;
      case 't':
        resp_data[0] = 't';
        anim_fn = strobe_fn;
        break;
      case 'o':
        resp_data[0] = 'o';
        anim_fn = off_fn;
        break;
      case 'x':

      default:
      resp_data[0] = '?';
    }
    cmd_udp.beginPacket(cmd_udp.remoteIP(), cmd_udp.remotePort());
    resp_data[1] = 0;
    cmd_udp.write(resp_data);
    Serial.print(resp_data);
    cmd_udp.endPacket();
    txt_data_index = 0;
  }
}

void loop()
{
    static unsigned long loop_ms = 0;
    
    net_parse();
    digitalWrite(LED_BUILTIN, 0);
        
    if(anim_fn) {
      if((millis() - loop_ms) > (1000/fps)) {
        anim_fn(&fps);
        strip.Show();
        if(anim_fn == off_fn) 
          anim_fn = NULL;
        loop_ms = millis();
      }
    }
}

