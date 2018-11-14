
/*
TODO 
- knipperen bij statusverandering
- http server voor debug info
- klok etc

*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  
#include <TimeLib.h> //TimeLib library is needed https://github.com/PaulStoffregen/Time
#include <NtpClientLib.h> //Include NtpClient library header

const char* mqtt_server = "192.168.0.3";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

float temp;
float solar;


// leds
#include <NeoPixelBus.h>

const uint16_t PixelCount = 12;
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 16

// three element pixels, in different order and speeds
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

RgbColor red(colorSaturation, 0, 0);
RgbColor pink(colorSaturation, 0, colorSaturation);
RgbColor yellow(colorSaturation, colorSaturation, 0);
RgbColor purple(colorSaturation/4, 0, colorSaturation);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

// int hourr = 12;
const int poweron = 9; //turn leds on at 9 o'clock
const int poweroff = 18; //off at six

void setup_wifi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("Pretty fly for a wifi");
  
//   Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message = "";
  String topics = topic;

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();
  Serial.println(message);
  Serial.println(topics);
  if (topic[0] == 's') {
      solar = message.toFloat();
  }

  if (topic[0] =='/') {
      temp = message.toFloat();
  }

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

Serial.println(temp);
Serial.println(solar);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("test", "hello world");
      // ... and resubscribe
      client.subscribe("/temp/picaxe");
      client.subscribe("solar/power");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, 0); // led aan
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.publish("test", "ledringklok online");

  // NTP begin with default parameters:
	//   NTP server: pool.ntp.org
	//   TimeZone: UTC
	//   Daylight saving: off
	// NTP.begin (); // Only statement needed to start NTP sync.
        NTP.begin ("pool.ntp.org", 1, true, 0);
        NTP.setInterval (63);
        NTP.getTime(); // daadwerkelijk tijd ophalen

  //leds
      // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();
    digitalWrite(BUILTIN_LED, 1); // led uit

}

boolean powersave() {
  return hour() < poweron || hour() >= poweroff; 
}

void error(RgbColor color) {
  // 3 manieren om een fout af te handelen:
  // 1: doe niks -> de laatste kleur blijft staan
  
  // 2: in geval van twijfel -> niks tonen. paint it black!
  //  paint(black);

  // 3: informatieve maar irritante kleurtjes
  // paint(color);
}

void paint(RgbColor color) {
  if (powersave()) { color = black;}

    for (int x=0; x<PixelCount; x++) {
      strip.SetPixelColor(x, color); 
    }
    strip.Show();
}

time_t decode(String hrs, String min) {
  return 3600 * (hrs.toInt()) + 60 * (min.toInt());
  }

bool raincheck() {
  //Neerslagintensiteit = 10^((waarde-109)/32)
 bool somerain = false;

if(WiFi.status()== WL_CONNECTED){ //Check WiFi connection status
  
    time_t nu = elapsedSecsToday(NTP.getTime()) - 300;
    time_t straks = nu + 2100;

    HTTPClient http; //Declare an object of class HTTPClient
    http.begin("http://gpsgadget.buienradar.nl/data/raintext/?lat=53.19&lon=6.56"); //Specify request destination
    int httpCode = http.GET(); //Send the request
    if (httpCode == 200 ) { //Check the returning code // && http.getString().length() > 66
      String payload = http.getString(); //Get the request response payload
      Serial.println(payload); //Print the response payload

      // read 24 lines
      for (int x=0; x<24; x++) {
        time_t linetime = decode(payload.substring((x*11)+4, (x*11)+6), payload.substring((x*11)+7, (x*11)+9));
        if (linetime > nu && linetime < straks) {
          Serial.println(payload.substring((x*11)+4, (x*11)+9));
          somerain = somerain || payload.substring(x*11, (x*11)+3) != "000";
        }
      }
      Serial.println(somerain ? "rain" : "no rain");
    } else {
      error(yellow);
      delay(5000);
    }

    http.end(); //Close connection
  } else {
    Serial.println("Error in WiFi connection");   
    error(purple);
    delay(5000);
  }
  return somerain;
}


void loop() {
// geen mqtt op kantoor
  // if (!client.connected()) {
  //   reconnect();
  // }
  // client.loop();

Serial.println (NTP.getTimeDateString()); 
Serial.println(NTP.getLastNTPSync());



  // if (raincheck()) {
  //   paint(blue);
  //   digitalWrite(BUILTIN_LED, 0);
  // } else {
  //   paint(green);
  //   digitalWrite(BUILTIN_LED, 1);
  // }
  paint(black);
  Serial.println(hourFormat12());
  Serial.println(minute());
  Serial.println(second());
  strip.SetPixelColor(second()*PixelCount/60, blue); 
  strip.SetPixelColor(minute()*PixelCount/60, green); 
  strip.SetPixelColor((hour()%12)*PixelCount/12, red); 
  strip.Show();

  delay(1000); //Send a request every 300 seconds

}

