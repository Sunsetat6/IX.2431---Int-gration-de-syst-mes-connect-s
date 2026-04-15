#include <WiFiS3.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define SIGPIN 3

DHT dht(DHTPIN, DHTTYPE);

char ssid[] = "荣耀Magic5";
char pass[] = "12345656";

const char USERNAME[] = "admin";
const char PASSWORD[] = "123456";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

float temperatureC = NAN;
float humidityPct = NAN;
float distanceCm = -1;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

// 读取并更新传感器数据
void updateSensors() {
  if (millis() - lastSensorRead < sensorInterval && lastSensorRead != 0) {
    return;
  }
  lastSensorRead = millis();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h)) humidityPct = h;
  if (!isnan(t)) temperatureC = t;

  pinMode(SIGPIN, OUTPUT);
  digitalWrite(SIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(SIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SIGPIN, LOW);

  pinMode(SIGPIN, INPUT);
  long duration = pulseIn(SIGPIN, HIGH, 30000UL);

  if (duration > 0) {
    distanceCm = duration * 0.034 / 2.0;
  } else {
    distanceCm = -1;
  }

  Serial.print("temperature: ");
  if (isnan(temperatureC)) Serial.print("N/A");
  else Serial.print(temperatureC);
  Serial.print(" °C  ");

  Serial.print("humidity: ");
  if (isnan(humidityPct)) Serial.print("N/A");
  else Serial.print(humidityPct);
  Serial.print(" %  ");

  Serial.print("distance: ");
  if (distanceCm < 0) Serial.print("N/A");
  else Serial.print(distanceCm);
  Serial.println(" cm");
}

String getParam(String url, String key) {
  String token = key + "=";
  int start = url.indexOf(token);
  if (start == -1) return "";

  start += token.length();
  int end = url.indexOf('&', start);
  if (end == -1) end = url.indexOf(' ', start);
  if (end == -1) end = url.length();

  return url.substring(start, end);
}

bool isAuthorized(String url) {
  String u = getParam(url, "u");
  String p = getParam(url, "p");
  return (u == USERNAME && p == PASSWORD);
}

void sendLoginPage(WiFiClient &client, String msg = "") {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>UNO R4 Login</title>");
  client.println("<style>");
  client.println("body{font-family:Arial;background:#f3f6fb;text-align:center;padding-top:50px;}");
  client.println(".box{display:inline-block;background:#fff;padding:24px 28px;border-radius:14px;box-shadow:0 2px 10px rgba(0,0,0,.12);}");
  client.println("input{display:block;width:220px;margin:10px auto;padding:10px;font-size:16px;}");
  client.println("button{padding:10px 18px;font-size:16px;cursor:pointer;}");
  client.println(".msg{color:#d00;height:24px;}");
  client.println("</style></head><body>");

  client.println("<div class='box'>");
  client.println("<h2>UNO R4 Sensor Login</h2>");
  client.print("<div class='msg'>");
  client.print(msg);
  client.println("</div>");
  client.println("<form action='/data' method='get'>");
  client.println("<input name='u' placeholder='Username'>");
  client.println("<input name='p' type='password' placeholder='Password'>");
  client.println("<button type='submit'>Login</button>");
  client.println("</form></div></body></html>");
}

void sendDataPage(WiFiClient &client) {
  updateSensors();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<meta http-equiv='refresh' content='3'>");
  client.println("<title>UNO R4 Dashboard</title>");
  client.println("<style>");
  client.println("body{font-family:Arial;background:#eef3f8;margin:0;padding:24px;text-align:center;}");
  client.println(".card{display:inline-block;background:#fff;padding:24px 30px;border-radius:16px;box-shadow:0 2px 10px rgba(0,0,0,.12);min-width:320px;}");
  client.println(".item{font-size:20px;margin:14px 0;}");
  client.println(".label{color:#555;display:inline-block;width:120px;text-align:right;margin-right:10px;}");
  client.println(".value{font-weight:bold;color:#1d4ed8;}");
  client.println("a{display:inline-block;margin-top:16px;}");
  client.println("</style></head><body>");

  client.println("<div class='card'>");
  client.println("<h2>Live Sensor Data</h2>");

  client.print("<div class='item'><span class='label'>Temperature:</span><span class='value'>");
  if (isnan(temperatureC)) client.print("N/A");
  else {
    client.print(temperatureC, 1);
    client.print(" &deg;C");
  }
  client.println("</span></div>");

  client.print("<div class='item'><span class='label'>Humidity:</span><span class='value'>");
  if (isnan(humidityPct)) client.print("N/A");
  else {
    client.print(humidityPct, 1);
    client.print(" %");
  }
  client.println("</span></div>");

  client.print("<div class='item'><span class='label'>Distance:</span><span class='value'>");
  if (distanceCm < 0) client.print("N/A");
  else {
    client.print(distanceCm, 1);
    client.print(" cm");
  }
  client.println("</span></div>");

  client.println("<div style='color:#666;font-size:14px;'>Auto refresh every 3 seconds</div>");
  client.println("<a href='/'>Back to login</a>");
  client.println("</div></body></html>");
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  dht.begin();
  pinMode(SIGPIN, INPUT);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  server.begin();

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Open browser: http://");
  Serial.println(WiFi.localIP());

  updateSensors();
}

void loop() {
  updateSensors();

  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("new client");
  String requestLine = "";
  bool currentLineIsBlank = true;

  while (client.connected()) {
    if (client.available()) {
      char c = client.read();

      if (requestLine.length() < 180 && c != '\n' && c != '\r') {
        requestLine += c;
      }

      if (c == '\n' && currentLineIsBlank) {
        Serial.print("Request: ");
        Serial.println(requestLine);

        if (requestLine.indexOf("GET / ") >= 0) {
          sendLoginPage(client);
        } else if (requestLine.indexOf("GET /data") >= 0) {
          if (isAuthorized(requestLine)) sendDataPage(client);
          else sendLoginPage(client, "Wrong username or password");
        } else {
          sendLoginPage(client);
        }
        break;
      }

      if (c == '\n') currentLineIsBlank = true;
      else if (c != '\r') currentLineIsBlank = false;
    }
  }

  delay(1);
  client.stop();
  Serial.println("client disconnected");
}