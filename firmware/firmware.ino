#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define LED_PIN 2
#define NUM_LEDS 60
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "RedRover";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);

// Two gradients only:
uint32_t MODE1_A = strip.Color(255, 150, 50);
uint32_t MODE1_B = strip.Color(255, 20, 147);

uint32_t MODE5_A = strip.Color(0, 150, 50);
uint32_t MODE5_B = strip.Color(120, 255, 90);

// Current active gradient
uint32_t fromA, fromB;
uint32_t toA, toB;

float transitionProgress = 1.0; 

unsigned long lastMillis = 0;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void startTransition(uint32_t newA, uint32_t newB) {
  fromA = toA;
  fromB = toB;
  toA = newA;
  toB = newB;
  transitionProgress = 0.0;
}

uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
  uint8_t r1 = (c1 >> 16) & 255, g1 = (c1 >> 8) & 255, b1 = c1 & 255;
  uint8_t r2 = (c2 >> 16) & 255, g2 = (c2 >> 8) & 255, b2 = c2 & 255;

  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;

  return strip.Color(r, g, b);
}

void renderGradient() {
  unsigned long t = millis();
  float waveSpeed = 0.002;

  for (int i = 0; i < NUM_LEDS; i++) {
    float wave = (sin(i * 0.2 + t * waveSpeed) + 1.0) * 0.5;

    uint32_t cStart = lerpColor(fromA, toA, transitionProgress);
    uint32_t cEnd   = lerpColor(fromB, toB, transitionProgress);

    uint32_t c = lerpColor(cStart, cEnd, wave);
    strip.setPixelColor(i, c);
  }
  
  strip.show();
}

void mqtt_callback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();

  Serial.printf("MQTT [%s] %s\n", topic, msg.c_str());

  if (String(topic) == "esp32/mode") {
    if (msg == "MODE_1") startTransition(MODE1_A, MODE1_B);
    if (msg == "MODE_5") startTransition(MODE5_A, MODE5_B);
  }

  if (String(topic) == "esp32/ping") {
    String reply = "ONLINE|" + WiFi.localIP().toString();
    client.publish("esp32/status", reply.c_str());
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect("ESP32-Lantern")) {
      Serial.println("connected.");
      client.subscribe("esp32/mode");
      client.subscribe("esp32/ping");
      String msg = "ONLINE|" + WiFi.localIP().toString();
      client.publish("esp32/status", msg.c_str());
    } else {
      Serial.println("Retry in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();

  toA = MODE5_A;  
  toB = MODE5_B;

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // smooth easing of transitions
  if (transitionProgress < 1.0) {
    transitionProgress += 0.02;  // adjust for speed
    if (transitionProgress > 1.0) transitionProgress = 1.0;
  }

  renderGradient();

  delay(20);
}
