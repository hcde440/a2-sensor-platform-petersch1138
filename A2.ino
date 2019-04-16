#include <Adafruit_Si7021.h>


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include <Servo.h>


String ipAddress;
String city;
String windspeed;

Servo servo;

#define SERVO_PIN 2 // defines pin that servo uses
typedef struct { // Weatherdata struct object contains string value pairs humidity, wind speed
  String hd;
  String ws;

} WData;

WData weather;
// set up the feeds
AdafruitIO_Feed *servo_feed = io.feed("servo");
AdafruitIO_Feed *local_humidity = io.feed("localHum");
AdafruitIO_Feed *city_humidity = io.feed("humidity");
AdafruitIO_Feed *wind_speed = io.feed("wind_speed");


Adafruit_Si7021 sensor; // instantiate sensor var
// Peter Schultz HCDE 440 4/16/19
// This script takes in a city name from Adafruit IO that a user can enter on the dashboard, and
// finds the wind speed in that city using the weather API that we used before, and rotates a 
// micro servo with a speed that reflects the wind speed in that city. It also uses an I2C humidity
// sensor, and compares the local humidity to the humidity taken from the weather API.
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.print("This board is running: "); // the big 4
   Serial.println(F(__FILE__));
   Serial.print("Compiled: ");
   Serial.println(F(__DATE__ " " __TIME__));
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA); // wifi mode
  WiFi.begin(WIFI_SSID, WIFI_PASS); // attempts to connect to wifi with given name and password

  while (WiFi.status() != WL_CONNECTED) { // prints dots until connected
    delay(500);
    Serial.print(".");
  }

  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP()); // local/internal ip address from wifi router
  sensor = Adafruit_Si7021(); // initialize i2c humidity sensor
  sensor.begin(); // turn on sensor
  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();
    // set up a message handler for the 'servo' feed.
  // the handleMessage function (defined below)
  // will be called whenever a message is
  // received from adafruit io.
  servo_feed->onMessage(handleMessage);
    // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  servo_feed->get(); // gets data from the adafruit io website for the city name
  servo.attach(SERVO_PIN); // tells servo to get data from this pin
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

}

// this function is called whenever a 'servo' message
// is received from Adafruit IO. it was attached to
// the servo feed in the setup() function above.

void handleMessage(AdafruitIO_Data *data) {

  // convert the data to integer
  String city = data->toString(); // converts incoming data from Adafruit IO to a string
  getWeather(city); // gets the wind speed in the given city

  Serial.println("**********************************************");

  Serial.print("Windspeed in ");
  Serial.print(city);
  Serial.print(": ");
  Serial.print(weather.ws);
  Serial.println(" mph.");
  Serial.println("");

  Serial.print("Humidity in ");
  Serial.print(city);
  Serial.print(": ");
  Serial.print(weather.hd);
  Serial.println("%");

  Serial.print("Local humidity: ");
  int localHum = sensor.readHumidity(); // get humidity from si7021 sensor
  Serial.print(localHum);
  Serial.println("%");
  Serial.println("");


  Serial.print("Difference in humidities: ");
  Serial.print(abs(localHum - weather.hd.toInt())); // find difference between local humidity and city humidity
  Serial.println("%.");
  Serial.println("");

  local_humidity->save(localHum); // saves data to adafruit io
  city_humidity->save(weather.hd);
  wind_speed->save(weather.ws);

  Serial.println("moving servo");
  int delayTime = (15-windspeed.toInt()); // sets the delay time, the faster the wind speed, the faster the rotation
  if(delayTime < 2) { // 15 is the default if there is no wind, and 2 is the minimum delay time (maximum rotation speed)
    delayTime = 2; 
  }
  int pos = 0;
  for (int i = 0; i < 5; i++) {
    for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      servo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(delayTime);                       // waits 15ms for the servo to reach the position
    }
    for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
      servo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(delayTime);                       // waits 15ms for the servo to reach the position
    }
  }
}



void getWeather(String city) { // this method gets the wind speed in the given city from the weather API 
  HTTPClient theClient; // initializes browser
  String apiCall = "http://api.openweathermap.org/data/2.5/weather?q=" + city; // these lines construct api address
  apiCall += "&units=imperial&appid=";
  apiCall += weatherKey; 
  theClient.begin(apiCall); // client connects to given address, openweathermap.org
  int httpCode = theClient.GET(); // gets http response code
  if (httpCode > 0) { 

    if (httpCode == HTTP_CODE_OK) { // 200 means its working
      String payload = theClient.getString(); // gets the payload from the website (json format String)
      DynamicJsonBuffer jsonBuffer; // jsonbuffer will parse the payload
      JsonObject& root = jsonBuffer.parseObject(payload); // jsonObject contains the json data
      if (!root.success()) {
        Serial.println("parseObject() failed in getSpeed().");
        return;
      }
      
      weather.hd = root["main"]["humidity"].as<String>(); // grabs humidity from json format string
      weather.ws = root["wind"]["speed"].as<String>(); // grabs speed of wind from the json format string 
    }
  } else {
    Serial.printf("Something went wrong with connecting to the endpoint in getSpeed().");
  }
}
