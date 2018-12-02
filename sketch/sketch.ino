#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

// config
const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";

const String API_KEY = "API_KEY";

const char* SYMBOLS[] = {"BTC", "ETH"};
const char* CONVERT_TO = "USD";

const long DEFAULT_DELAY = 60 * 1000 * 5; // wait 5 minutes between API request; +info https://pro.coinmarketcap.com/account/plan

// network
WiFiClientSecure client;

const char* HOST = "pro-api.coinmarketcap.com";
const int HTTPS_PORT = 443;

// presentation
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 4, /* data=*/ 5);

const int SEPARATOR = 2;
const String PERCENTAGE = "  "; // or "% "

int x = 0;
int y = 0;

String symbol = "BTC";
int symbolsCurrentPosition = -1;

String name = "";
float price = 0;
float percent1h = 0;
float percent24h = 0;
float percent7d = 0;
float cap = 0;
String lastUpdated = "";

void setup() {
  setupWifi();
  setupDisplay();
}

void loop() {
  if (getInfo()) {
    showInfo();
  }

  delay();
}

void setupWifi() {
  Serial.begin(115200);
  delay(100);
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
}

void setupDisplay() {
  u8g2.begin();
  u8g2.setFontMode(0);
}

bool getInfo() {
  if (!client.connect(HOST, HTTPS_PORT)) {
    Serial.println("Connection failed!");
    return false;
  }

  symbol = getSymbol();
  String endpoint = "/v1/cryptocurrency/quotes/latest?CMC_PRO_API_KEY=" + API_KEY + "&symbol=" + symbol  + "&convert=" + CONVERT_TO;

  Serial.print("Requesting: ");
  Serial.println(endpoint);

  client.print(String("GET ") + endpoint + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  if (client.println() == 0) {
    Serial.println("Failed to send request");
    return false;
  }

  // check HTTP response
  char httpStatus[32] = {0};
  client.readBytesUntil('\r', httpStatus, sizeof(httpStatus));
  if (strcmp(httpStatus, "HTTP/1.1 200 OK") != 0) {
    Serial.print("Unexpected response: ");
    Serial.println(httpStatus);
    return false;
  }

  // skip HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  // skip content length
  if (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  // get response
  String response = "";
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    line.trim();
    if (line != "\r") {
      response += line;
    }
  }

  client.stop();

  // parse response
  DynamicJsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, response);
  if (error) {
    Serial.println("Deserialization failed");
    return false;
  }
  JsonObject root = jsonDocument.as<JsonObject>();

  // check API status
  JsonObject status = root["status"];
  int statusErrorCode = status["error_code"];
  if (statusErrorCode != 0) {
    String statusErrorMessage = status["error_message"];
    Serial.print("Error: ");
    Serial.println(statusErrorMessage);
    delay();
    return false;
  }

  JsonObject coin = root["data"][symbol];
  name = coin["name"].as<String>();
  lastUpdated = coin["last_updated"].as<String>();

  JsonObject quote = coin["quote"][CONVERT_TO];
  price = quote["price"];
  percent1h = quote["percent_change_1h"];
  percent24h = quote["percent_change_24h"];
  percent7d = quote["percent_change_7d"];
  cap = quote["market_cap"];

  return true;
}

String getSymbol() {
  if (symbolsCurrentPosition == -1 || symbolsCurrentPosition + 1 > (sizeof(SYMBOLS) / sizeof(SYMBOLS[0])) - 1) {
    symbolsCurrentPosition = 0;
  } else {
    symbolsCurrentPosition++;
  }
  return SYMBOLS[symbolsCurrentPosition];
}

void showInfo() {
  u8g2.firstPage(); // + info https://github.com/olikraus/u8glib/wiki/tpictureloop
  do {
    // symbol
    u8g2.setFont(u8g2_font_fub11_tf);
    x = 0;
    y = 11; // from below, capital A size. +info https://github.com/olikraus/u8g2/wiki/fntgrpfreeuniversal
    u8g2print(name);

    // price
    u8g2.setFont(u8g2_font_fub14_tf);
    x = 0;
    y += 14 + SEPARATOR;
    String priceFormatted = String(price, 2);
    u8g2print(priceFormatted);

    x = getStringWidth(priceFormatted) + SEPARATOR; // measure before changing font size
    // y = same baseline
    u8g2.setFont(u8g2_font_fur11_tf);
    u8g2print(CONVERT_TO);

    // percentages
    String percent1hFormatted = String(percent1h, 1) + PERCENTAGE;
    String percent24hFormatted = String(percent24h, 1) + PERCENTAGE;
    String percent7dFormatted = String(percent7d, 1) + PERCENTAGE;

    u8g2.setFont(u8g2_font_fub11_tf);
    x = 0;
    y += 11 + SEPARATOR;
    u8g2print(percent1hFormatted);

    x += getStringWidth(percent1hFormatted) + SEPARATOR;
    u8g2print(percent24hFormatted);

    x += getStringWidth(percent24hFormatted) + SEPARATOR;
    u8g2print(percent7dFormatted);

    // cap
    u8g2.setFont(u8g2_font_profont15_tf);
    x = 0;
    y += 9 + SEPARATOR;
    String capFormatted = String(cap, 2);
    u8g2print(capFormatted);

    u8g2.setFont(u8g2_font_profont15_tf);
    y += 9 + SEPARATOR;
    u8g2print(convertToTime(lastUpdated));
  } while ( u8g2.nextPage() );
}

int getStringWidth(String string) {
  char *charString = const_cast<char*>(string.c_str());
  return u8g2.getStrWidth(charString);
}

void u8g2print(String string) {
  u8g2.setCursor(x, y);
  u8g2.print(string);
}

String convertToTime(String timestamp) {
  return timestamp.substring(11, 16); // extract HH:mm
}

void delay() {
  Serial.println("");
  Serial.println("Keep calm and hodl!");
  delay(DEFAULT_DELAY);
}
