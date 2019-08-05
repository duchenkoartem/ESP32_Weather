// Including Lib's
#include <WiFi.h>
#include <ArduinoJson.h>    //
#include <PubSubClient.h>

// Define hardware
#define LED_RED 12
#define LED_GREEN 13
#define LED_BLUE 15

// Define LED states
#define OFF 0
#define RED 1
#define GREEN 2
#define BLUE 3


// Define Parameters
#define UPDATE_PERIOD_S 10 //weather update period in seconds
#define LOW_TEMP 20
#define HI_TEMP 25

// Wifi
// Replace with your SSID and password
const char* ssid     = "........"; // Enter your wi-fi network SSID
const char* password = "........"; // Enter your wi-fi network password

//JSON Weather Input Server
const char* weather_server = "api.openweathermap.org";
String APIKEY = "a07521e340e0a8c642ea392958c6ed7f"; // personal API key
String CityID = "703448"; //Kyiv city ID

//MQTT Output Server
const char* mqtt_server = "iot.eclipse.org";
const char* OutTopic = "KievWeather";

// Project Variables
int update_counter = 0; // counts update period seconds

float temperature = 0; // global temperature variable 
String weatherDescription =""; // date and time details

#define OutMLength 30 // output MQTT massage length 
#define DataMLength 10 // date massage length

void setup() {

  Serial.begin(9600);// start debug
  
  InitLED(); // HW LED init
  SetLED(OFF); // Set LED state OFF

  connectToWifi();

}

void loop() {
  // put your main code here, to run repeatedly:
  if (update_counter<=0)
  {
    Serial.println("\n-----------------------------------------------------------");
    Serial.println("-----------------------------------------------------------");
    Serial.print("Try to get the weather data");
    // start weathet update
    getWeatherData();
    Serial.println("-----------------------------------------------------------");
    Serial.println("Try to send the weather data");
    sendWeatherData();
    Serial.println("-----------------------------------------------------------");
    Serial.println("-----------------------------------------------------------");
    update_counter=UPDATE_PERIOD_S;
    Serial.print("Update Timeout(");
    Serial.print(UPDATE_PERIOD_S);
    Serial.print("s):");
  }
  else
  {
     update_counter--;
     delay(1000);
     Serial.print(".");
  }

}

void sendWeatherData()
{
  char OutMassage[OutMLength]; // output MQTT massage buffer
  
  WiFiClient espClient;
  PubSubClient client(espClient);
  client.setServer(mqtt_server, 1883);
  
  if (!client.connected()) {
    //Reconnect
    // Loop until we're reconnected
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
  }
  client.loop();

  char date [DataMLength+1];
  weatherDescription.toCharArray(date,sizeof(date));
  date[DataMLength+1] = '\0'; 
  
  snprintf (OutMassage, OutMLength, ("Date: %s Temp: %.2f"),date, temperature);
  
  Serial.print("Publish message: ");
  Serial.println(OutMassage);
  client.publish(OutTopic, OutMassage);
  client.disconnect();

  Serial.println("DONE! ");
}

void getWeatherData() //client function to send/receive GET request data.
{
  String JsonInMassage =""; // input Json massage buffer
  
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(weather_server, httpPort)) {
        Serial.println (" - failed!");
        return;
    }
      // We now create a URI for the request
    String url = "/data/2.5/forecast?id="+CityID+"&units=metric&cnt=1&APPID="+APIKEY;
       // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + weather_server + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            client.stop();
            Serial.println("Error: Connection timeout");
            return;
        }
    }

    // Read all the lines of the reply from server
    while(client.available()) {
        JsonInMassage = client.readStringUntil('\r');
    }

  JsonInMassage.replace('[', ' ');
  JsonInMassage.replace(']', ' ');

  Parse_Json(JsonInMassage); // parsing Json massage
}

void Parse_Json(String InputBuffer)
{
  const size_t capacity = 2*JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + 2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(8) + 440;
  DynamicJsonBuffer jsonBuffer(capacity);
    
  char json [InputBuffer.length()+1];
  InputBuffer.toCharArray(json,sizeof(json));
  json[InputBuffer.length() + 1] = '\0';

  JsonObject& root = jsonBuffer.parseObject(json);
  
  const char* cod = root["cod"]; // "200"
  float message = root["message"]; // 0.0114
  int cnt = root["cnt"]; // 1
  
  JsonObject& list = root["list"];
  long list_dt = list["dt"]; // 1564920000
  
  JsonObject& list_main = list["main"];
  float list_main_temp = list_main["temp"]; // 22.09
  float list_main_temp_min = list_main["temp_min"]; // 21.5
  float list_main_temp_max = list_main["temp_max"]; // 22.09
  float list_main_pressure = list_main["pressure"]; // 1006.26
  float list_main_sea_level = list_main["sea_level"]; // 1006.26
  float list_main_grnd_level = list_main["grnd_level"]; // 990.78
  int list_main_humidity = list_main["humidity"]; // 37
  float list_main_temp_kf = list_main["temp_kf"]; // 0.59
  
  JsonObject& list_weather = list["weather"];
  int list_weather_id = list_weather["id"]; // 804
  const char* list_weather_main = list_weather["main"]; // "Clouds"
  const char* list_weather_description = list_weather["description"]; // "overcast clouds"
  const char* list_weather_icon = list_weather["icon"]; // "04d"
  
  int list_clouds_all = list["clouds"]["all"]; // 100
  
  float list_wind_speed = list["wind"]["speed"]; // 5.2
  float list_wind_deg = list["wind"]["deg"]; // 351.275
  
  const char* list_sys_pod = list["sys"]["pod"]; // "d"
  
  const char* list_dt_txt = list["dt_txt"]; // "2019-08-04 12:00:00"
  
  JsonObject& city = root["city"];
  long city_id = city["id"]; // 703448
  const char* city_name = city["name"]; // "Kiev"
  
  float city_coord_lat = city["coord"]["lat"]; // 50.4333
  float city_coord_lon = city["coord"]["lon"]; // 30.5167
  
  const char* city_country = city["country"]; // "UA"
  int city_timezone = city["timezone"]; // 10800
  

  Serial.println (" - success!");
  //Serial.println("-----------------------------------------------------------");
  Serial.print("Date and Time: ");
  Serial.print(list_dt_txt);
  Serial.print("\nWeatherID: ");
  Serial.print(city_id);
  Serial.print("\nLocation: ");
  Serial.print(city_name);
  Serial.print("\nTemperature: ");
  Serial.println(list_main_temp);
  Serial.println("-----------------------------------------------------------");

  temperature = list_main_temp; // copy temperature value to global
  weatherDescription = list_dt_txt; // copy date and time value to global

  if (temperature<LOW_TEMP)
  {
    SetLED(BLUE);
    Serial.println("So cold!");
    Serial.println("Blue LED ON");
  }
  else if (temperature>=HI_TEMP)
  {
    SetLED(RED);
    Serial.println("So hot!");
    Serial.println("Red LED ON");
  }
  else 
  {
    SetLED(GREEN);
    Serial.println("Nice weather!");
    Serial.println("Green LED ON");
  }
  
}

void connectToWifi()
{
  WiFi.enableSTA(true);
  
  delay(2000);

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
   Serial.println(" ");
  Serial.println("Connected!");
  
}

void InitLED()
{
  pinMode (LED_RED, OUTPUT);
  pinMode (LED_GREEN, OUTPUT);
  pinMode (LED_BLUE, OUTPUT);
}

void SetLED(int State)
{
  switch (State){
  case OFF:
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, HIGH);
  break;
    case RED:
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW);
  break;
    case GREEN:
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, HIGH);
  break;
    case BLUE:
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, HIGH);
  break;

  default:
  break;
  }
}
