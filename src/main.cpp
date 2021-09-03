#include <FS.h> 

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <NeoPixelBus.h>
#include <WiFiManager.h>

#define HOSTNAME_PREFIX "door-leds-1"
#define SEG_LEDS 134
#define NUM_LEDS (SEG_LEDS + SEG_LEDS + SEG_LEDS + SEG_LEDS + 6)


// Topology
//
//      ====|====
//      ====|====
//      ====|====
//      |=======|
//      |AUSGANG|
//      |=======|
// 
//       ^ ------> R_R
// L_L   | NUM_LEDS |   
//  |    |     |    |
//  v    ^     ^    v
//  |    |     |    |
//  \ > L_R    \-<-R_R
//

#define LED_INDEX_SEG_L_L_START 0
#define LED_INDEX_SEG_L_L_END (LED_INDEX_SEG_L_L_START + SEG_LEDS)
#define LED_INDEX_SEG_L_R_START (LED_INDEX_SEG_L_L_END)
#define LED_INDEX_SEG_L_R_END (LED_INDEX_SEG_L_R_START + SEG_LEDS)

#define LED_INDEX_SEG_R_R_START (LED_INDEX_SEG_L_R_END)
#define LED_INDEX_SEG_R_R_END (LED_INDEX_SEG_R_R_START + SEG_LEDS)
#define LED_INDEX_SEG_R_L_START (LED_INDEX_SEG_R_R_END + 6)
#define LED_INDEX_SEG_R_L_END (LED_INDEX_SEG_R_L_START + SEG_LEDS)

#define UDP_PORT 2342
#define FRAMES_PER_SECOND 120
#define PKT_MAX_LEN 24

typedef void (*anim_fn_t) (uint8_t *fps);

NeoPixelBus<NeoGrbwFeature, NeoEsp8266Dma800KbpsMethod> strip(NUM_LEDS);
anim_fn_t anim_fn = NULL;
uint8_t fps = FRAMES_PER_SECOND;

RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor white(255);
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

bool status_led_state = false;

void test_fn(uint8_t *fps);

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
  IPAddress static_IP(10, 23, 40, 163);
  IPAddress gw_IP(10, 23, 40, 1);
  IPAddress mask_IP(255, 255, 255, 0);
  WiFi.hostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.config(static_IP, gw_IP, mask_IP);
  WiFi.begin("lab23-technik", "93h31m3r73cHN1kk3Y");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //WiFiManager wifiManager;
  // uncomment to reset saved settings
  // wifiManager.resetSettings();ArduinoOTA

  // fetches ssid and key from eeprom/flash and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  //wifiManager.autoConnect("lab");
  Serial.println("connected...yeey :)");

  // print this client's IP on the UART for debugging purposes
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  cmd_udp.begin(UDP_PORT);

  ArduinoOTA.setHostname(cb);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  anim_fn = test_fn;
  test_fn(&fps);
  strip.Begin();
  strip.Show();
}

void strobe_fn(uint8_t *fps) {
  *fps=2;
  static int x = 0;
  x++;
  HslColor color = hslWhite;
  if(x % 2) {
    for (int p = LED_INDEX_SEG_L_L_START; p < LED_INDEX_SEG_L_L_END; p++) {
      strip.SetPixelColor(p, color);
    }
    for (int p = LED_INDEX_SEG_L_R_START; p < LED_INDEX_SEG_L_R_END; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
    for (int p = LED_INDEX_SEG_R_L_START; p < LED_INDEX_SEG_R_L_END; p++) {
      strip.SetPixelColor(p, color);
    }
    for (int p = LED_INDEX_SEG_R_R_START; p < LED_INDEX_SEG_R_R_END; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
  } else { 
    for (int p = LED_INDEX_SEG_L_L_START; p < LED_INDEX_SEG_L_L_END; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
    for (int p = LED_INDEX_SEG_L_R_START; p < LED_INDEX_SEG_L_R_END; p++) {
      strip.SetPixelColor(p, color);
    }
    for (int p = LED_INDEX_SEG_R_L_START; p < LED_INDEX_SEG_R_L_END; p++) {
      strip.SetPixelColor(p, hslBlack);
    }
    for (int p = LED_INDEX_SEG_R_R_START; p < LED_INDEX_SEG_R_R_END; p++) {
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
    HslColor color = HslColor((float)((i+hue) % (int)(NUM_LEDS/compression)) / ((float)NUM_LEDS/compression), 0.85, 0.1);
    strip.SetPixelColor(i, color);
  }
  hue+=huestep;
}

void off_fn(uint8_t *fps) {
  for(int i = 0; i < NUM_LEDS; i++){
    strip.SetPixelColor(i, hslBlack);
  }
}

void flag_strips(uint8_t *fps, std::vector<HtmlColor> flag) {
  *fps=2;
  if(flag.size() < 1) return;
  int flagstrip_numleds = SEG_LEDS / flag.size();

  for (int flagstrip_index = 0; flagstrip_index < flag.size(); flagstrip_index++) {
    for (int px = flagstrip_numleds * flagstrip_index; px < flagstrip_numleds * (flagstrip_index+1); px++) {
      strip.SetPixelColor(px+LED_INDEX_SEG_L_L_START, flag[flagstrip_index]);
      strip.SetPixelColor(LED_INDEX_SEG_L_R_END - px, flag[flagstrip_index]);
      strip.SetPixelColor(px + LED_INDEX_SEG_R_R_START, flag[flagstrip_index]);
      strip.SetPixelColor(LED_INDEX_SEG_R_L_END - px, flag[flagstrip_index]);
//      flag[flagstrip_index].ToNumericalString(cb, 200);
//      Serial.print(px);Serial.print(" ");Serial.println(cb);
    }
  }
}

void lgbt_flag_fn(uint8_t *fps) {
  flag_strips(fps,std::vector<HtmlColor> { 
    HtmlColor(0xe40303),
    HtmlColor(0xff8c00),
    HtmlColor(0xffed00),
    HtmlColor(0x008026),
    HtmlColor(0x004dff),
    HtmlColor(0x750787) });
}

void trans_flag_fn(uint8_t *fps) {
  flag_strips(fps,std::vector<HtmlColor> { 
    HtmlColor(0x5bcefa),
    HtmlColor(0xf5a9b8),
    HtmlColor(0xffffff),
    HtmlColor(0xf5a9b8),
    HtmlColor(0x5bcefa) });
}

void test_fn(uint8_t *fps) {
  strip.SetPixelColor(NUM_LEDS - 1, hslWhite);
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
        off_fn(&fps);
        anim_fn = rainbow_fn;
        break;
      case 't':
        resp_data[0] = 't';
        off_fn(&fps);
        anim_fn = strobe_fn;
        break;
      case 'o':
        resp_data[0] = 'o';
        anim_fn = off_fn;
        break;
      case 'l':
        resp_data[0] = 'l';
        off_fn(&fps);
        anim_fn = lgbt_flag_fn;
        break;
      case 'a':
        resp_data[0] = 'a';
        off_fn(&fps);
        anim_fn = trans_flag_fn;
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
    digitalWrite(LED_BUILTIN, status_led_state);
    status_led_state = !status_led_state;

  }
}

void loop()
{
  static unsigned long loop_ms = 0;
  
//  AsyncElegantOTA.loop();
  ArduinoOTA.handle();

  net_parse();
      
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

