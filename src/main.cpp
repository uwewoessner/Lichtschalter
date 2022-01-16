
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//#include <Wire.h>
//#include "Adafruit_MCP23017.h"
#include <PubSubClient.h>
#include "RotaryEnc.h"
#include "debounceButton.h"

// Replace with your network credentials
#include "../../../wifiPasswd.h"

void localLoop();
void sendState();
WiFiClient espClient;
PubSubClient client(espClient);

void DebugPrintf(const char *, ...); //Our printf function
char *convert(int, int);    //Convert integer number into octal, hex, etc.

int networkTimeout = 0;
bool PingArrived = false;

unsigned long pUpdate =0;
const char *mqtt_server = "192.168.178.34";
const int BLUELED=13;
const int RELAIS=12;
const int P_A0 = 0;
const int P_A1 = 1;
const int P_A2 = 2;
const int P_A3 = 3;
const int P_A4 = 4;
const int P_A5 = 5;
const int P_A6 = 6;
const int P_A7 = 7;
const int P_B0 = 8;
const int P_B1 = 9;
const int P_B2 = 10;
const int P_B3 = 11;
const int P_B4 = 12;
const int P_B5 = 13;
const int P_B6 = 14;
const int P_B7 = 15;
bool lightState1=false;
bool lightState2=false;

byte power1=50;
byte power2=50;
byte oldp1=power1;
byte oldp2=power2;
debounceButton b0(D0);
debounceButton b1(D1);

/* function prototypes */
void RotaryEncoderChanged(bool clockwise, int id);


/* Array of all rotary encoders and their pins */
RotaryEnc rotaryEncoders[] = {
        // outputA,B on GPA7,GPA6, register with callback and ID=1
        RotaryEnc(D2, D5, &RotaryEncoderChanged, 1),
        RotaryEnc(D6, D7, &RotaryEncoderChanged, 2)
};

void RotaryEncoderChanged(bool clockwise, int id)
{
  if(id==1 && lightState1)
  {
    if(clockwise)
    {
      power1++;
    }
    else{
      power1--;
    }
    if(power1 < 1) // do not  go below 1
       power1 = 1;
    if(power1 > 200)
    power1 = 0;
    if(power1 > 100)
    power1 = 100;
  }
  if(id==2 && lightState2)
  {
    if(clockwise)
    {
      power2++;
    }
    else{
      power2--;
    }
    if(power2 < 1) // do not  go below 1
       power2 = 1;
    if(power2 > 200)
    power2 = 0;
    if(power2 > 100)
    power2 = 100;
  }
    /*Serial.println("Encoder " + String(id) + ": "
            + (clockwise ? String("clockwise") : String("counter-clock-wise")));*/
}
constexpr int numEncoders = (int)(sizeof(rotaryEncoders) / sizeof(*rotaryEncoders));


char linebuf[200];
unsigned int linePos = 0;
unsigned int printLine = 0;
void outputChar(char c)
{

  if (linePos < 199)
  {
    linebuf[linePos] = c;
    linePos++;
    linebuf[linePos] = '\0';
  }
  if (c == '\n')
  {
    linePos = 0;
    if (client.connected())
    {
      char top[50];
      sprintf(top, "Debug/Lichtschalter/%d", printLine);
      client.publish(top, linebuf);
      printLine++;
      if (printLine > 20)
        printLine = 0;
    }
    else
    {
      //Serial.print(linebuf);
    }
    linebuf[0] = '\0';
  }
}
void outputCharp(const char *s)
{
  const char *c;
  for (c = s; *c != '\0'; c++)
  {
    outputChar(*c);
  }
}
void DebugPrintf(const char *format, ...)
{
  const char *traverse;
  int i;
  const char *s;
  char iBuf[20];

  va_list arg;
  va_start(arg, format);

  for (traverse = format; *traverse != '\0'; traverse++)
  {
    while (*traverse != '%' && *traverse != '\0')
    {
      outputChar(*traverse);
      traverse++;
    }
    if (*traverse == '\0')
    {
      break;
    }

    traverse++;

    //Module 2: Fetching and executing arguments
    switch (*traverse)
    {
    case 'c':
      i = va_arg(arg, int); //Fetch char argument
      outputChar(i);
      break;

    case 'd':
      i = va_arg(arg, int); //Fetch Decimal/Integer argument
      if (i < 0)
      {
        i = -i;
        outputChar('-');
      }
      outputCharp(itoa(i,iBuf, 10));
      break;

    case 'o':
      i = va_arg(arg, unsigned int); //Fetch Octal representation
      outputCharp(itoa(i,iBuf, 8));
      break;

    case 's':
      s = va_arg(arg, char *); //Fetch string
      outputCharp(s);
      break;

    case 'x':
      i = va_arg(arg, unsigned int); //Fetch Hexadecimal representation
      outputCharp(itoa(i,iBuf, 16));
      break;
    }
  }

  //Module 3: Closing argument list to necessary clean-up
  va_end(arg);
}

char *convert(int num, int base)
{
  static char Representation[] = "0123456789ABCDEF";
  static char buffer[50];
  char *ptr;

  ptr = &buffer[49];
  *ptr = '\0';

  do
  {
    *--ptr = Representation[num % base];
    num /= base;
  } while (num != 0);

  return (ptr);
}

//MQTT
#define Name0 "Lichtschalter"

void callback(char *topicP, byte *payloadP, unsigned int length)
{
  char topic[200];
  char payload[200];
  strncpy(topic, topicP, 200);
  strncpy(payload, (char *)payloadP, length);
  payload[length] = '\0';
  unsigned long now = millis();

  //DebugPrintf("Message arrived [%s] %s\n", topic, payload);
  if ((strcmp(topic, "wohnzimmer/essen/brightness") == 0)|| (strcmp(topic, "wohnzimmer/essen/bstatus") == 0))
  {
    int p;
      float fp=0;
      //sscanf(payload, "0,0,%d", &p);
      sscanf(payload, "%f", &fp);
      p = fp;
    if((pUpdate+2000)< now) // only update current power if button was not turned for at least two seconds, otherwise this is probably our own message with old power values
        oldp2 = power2 = p;
    if(p > 0)
        lightState2 = true;
    else
        lightState2 = false;
  }
  if (strcmp(topic, "wohnzimmer/essen/reset") == 0)
  {
    DebugPrintf("reset\n");
    ESP.restart();
  }
  if ((strcmp(topic, "wohnzimmer/essen/command") == 0)|| (strcmp(topic, "wohnzimmer/essen/status") == 0))
  {
    if(strcmp(payload,"ON")==0)
    {
        lightState2 = true;
    }
    else
    {
        lightState2 = false;
    }
    DebugPrintf("lightState2In%d\n", (int)lightState2);
  }
  if ((strcmp(topic, "wohnzimmer/wohnen/command") == 0) || (strcmp(topic, "wohnzimmer/wohnen/status") == 0))
  {
    if(strcmp(payload,"ON")==0)
    {
        lightState1 = true;
    }
    else
    {
        lightState1 = false;
    }
    //DebugPrintf("lightState1%d\n", (int)lightState1);
  }
  if ((strcmp(topic, "wohnzimmer/wohnen/brightness") == 0)|| (strcmp(topic, "wohnzimmer/wohnen/bstatus") == 0))
  {
    int p;
      float fp=0;
      //sscanf(payload, "0,0,%d", &p);
      sscanf(payload, "%f", &fp);
      p = fp;
    if((pUpdate+2000)< now) // only update current power if button was not turned for at least two seconds, otherwise this is probably our own message with old power values
        oldp1 = power1 = p;
    if(p > 0)
        lightState1 = true;
    else
        lightState1 = false;
  }
  else if(strcmp(topic,"IOT/Ping") == 0)
  {
    networkTimeout = 0;
    PingArrived = true;
  }
  sendState();
}

void sendState()
{
  #ifdef Terrasse
  if (lightState)
  {
    client.publish("terrasse/licht/status", "ON");
  }
  else
  {
    client.publish("terrasse/licht/status", "OFF");
  }
  #endif
}

void reconnect()
{
  #ifdef NO_MQTT
  return;
  #endif

  //DebugPrintf("Attempting MQTT connection...\n");
#ifdef ESP32
  esp_task_wdt_init(25, true); //socket timeout is 15seconds
#endif
  // Attempt to connect
  if (client.connect("clLichtschalter"))
  {
    DebugPrintf("connected\n");
    // Once connected, publish an announcement...
    sendState();
    // ... and resubscribe
    client.subscribe("wohnzimmer/essen/command");
    client.subscribe("wohnzimmer/essen/brightness");
    client.subscribe("wohnzimmer/wohnen/command");
    client.subscribe("wohnzimmer/wohnen/brightness");
    client.subscribe("wohnzimmer/essen/status");
    client.subscribe("wohnzimmer/essen/bstatus");
    client.subscribe("wohnzimmer/wohnen/status");
    client.subscribe("wohnzimmer/wohnen/bstatus");
    client.subscribe("IOT/Ping");
  }
  else
  {
    DebugPrintf("failed, rc=");
    DebugPrintf("%d", client.state());
    DebugPrintf(" try again in 5 seconds\n");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Lichtschalter");

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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  pinMode(D0,INPUT_PULLUP);
  pinMode(D1,INPUT_PULLUP);
  pinMode(D2,INPUT_PULLUP);
  pinMode(D5,INPUT_PULLUP);
  pinMode(D6,INPUT_PULLUP);
  pinMode(D7,INPUT_PULLUP);

   //Initialize input encoders (pin mode, interrupt)
    for(int i=0; i < numEncoders; i++) {
        rotaryEncoders[i].init();
    }

  DebugPrintf("%s\n", WiFi.localIP().toString().c_str());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  
  b0.init(false);
  b1.init(false);
  debounceButton::debounceTime = 20; // the debounce time, increase if the output flickers
  debounceButton::klickTime = 210; // the timelimit after which two klicks are counted as a doubleklick
  debounceButton::doubleKlickTime = 250;
}

void reconnectWifi()
{
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED)
  {
    long start = millis();
    while (millis() - start < 500)
    {
      localLoop();
    }
    ledState = !ledState;
    digitalWrite(13, ledState);
  }
}
long lastReconnectAttempt = 0;
long lastReconnectWifiAttempt = 0;



void loop() {
  
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    if (now - lastReconnectWifiAttempt > 60000) // every 60 seconds
    {
      lastReconnectWifiAttempt = now;
      // Attempt to reconnect
      reconnectWifi();
    }
  }
  if (!client.connected())
  {
    Serial.print("disconnected\n");
    if (now - lastReconnectAttempt > 10000) // every 10 seconds
    {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      reconnect();
    }
  }
  else
  {
    // Client connected

    client.loop();
  }

  localLoop(); // local updates which have to be done during reconnect as well;

}
void setPower(int l, byte p)
{
      char val[50];
  if(l==1 && lightState1)
  {
      sprintf(val, "%d", power1);
      client.publish("wohnzimmer/wohnen/brightness", val);
  }
  if(l==2 && lightState2)
  {
      sprintf(val, "%d", power2);
      client.publish("wohnzimmer/essen/brightness", val);
  }
}

void localLoop()
{
  unsigned long now = millis();
  if(now < pUpdate)
      pUpdate = now; // handle overrun
  debounceButton::update();
  for (int i = 0; i < numEncoders; i++)
  {
    //only feed this in the encoder if this
    //is coming from the correct MCP
    rotaryEncoders[i].update();
  }
  if (oldp1 != power1 && (pUpdate +100) < now)
  {
    if(lightState1 == false && oldp1 == 0 && power1 > 0)
    {
      lightState1 = true;
    }
    if(power1 == 0)
    {
      lightState1 = false;
    }
    pUpdate = now;
    //DebugPrintf("power1 %d\n", power1);
    setPower(1,power1);
    
    oldp1 = power1;
  }
  if (oldp2 != power2 && (pUpdate +100) < now)
  {
    if(lightState2 == false && oldp2 == 0 && power2 > 0)
    {
      lightState2 = true;
    }
    if(power2 == 0)
    {
      lightState2 = false;
    }
    pUpdate = now;
    //DebugPrintf("power2 %d\n", power2);
    setPower(2,power2);
    oldp2 = power2;
  }
  if (b1.wasPressed())
  {
    lightState2 = lightState1 = !lightState1;
    if (lightState1)
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "ON");
      if (power1 < 10)
      {
        power1 = 10;
        pUpdate = now;
        setPower(1, power1);
      }
    }
    else
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "OFF");
    }
    if (lightState2)
    {
      client.publish("wohnzimmer/essen/command", "ON");
      if (power2 < 10)
      {
        power2 = 10;
        pUpdate = now;
        setPower(2, power2);
      }
    }
    else
      client.publish("wohnzimmer/essen/command", "OFF");
  }
  if (b1.wasKlicked())
  {
    lightState1 = !lightState1;
    if (lightState1)
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "ON");
      if (power1 < 10)
      {
        power1 = 10;
        pUpdate = now;
        setPower(1, power1);
      }
    }
    else
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "OFF");
    }
  }
  if (b0.wasKlicked())
  {
    lightState2 = !lightState2;
    DebugPrintf("lightState2 b %d\n", lightState2);
    if (lightState2)
    {
      client.publish("wohnzimmer/essen/command", "ON");
      if (power2 < 10)
      {
        power2 = 10;
        pUpdate = now;
        setPower(2, power2);
      }
    }
    else
    {
      client.publish("wohnzimmer/essen/command", "OFF");
    }
  }
  if (b0.wasPressed())
  {
    lightState1 = lightState2 = !lightState2;
    if (lightState1)
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "ON");
      if (power1 < 10)
      {
        power1 = 10;
        pUpdate = now;
        setPower(1, power1);
      }
    }
    else
    {
      bool sendState = client.publish("wohnzimmer/wohnen/command", "OFF");
    }
    if (lightState2)
    {
      client.publish("wohnzimmer/essen/command", "ON");
      if (power2 < 10)
      {
        power2 = 10;
        pUpdate = now;
        setPower(2, power2);
      }
    }
    else
      client.publish("wohnzimmer/essen/command", "OFF");
  }
  
  if(b0.wasDoubleKlicked() || b1.wasDoubleKlicked())
  {
    ESP.restart();
  }

  static unsigned long networkMinute = 0;
  if((now - networkMinute) > 60000)
  {
    networkMinute = now;
    if(PingArrived) // only activate network timeout if at least one Ping has arrived
    {
      networkTimeout++; // this is reset whenever an mqtt network ping arrives
    }
  }
  if(networkTimeout > 5) // 5 minute timeout
  {
    ESP.restart();
  }


  ArduinoOTA.handle();
}