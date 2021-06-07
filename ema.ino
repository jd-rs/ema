#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <BlynkSimpleEsp8266.h>

#define READ_TEMP (25) //Current water temperature ℃, Or temperature sensor function

#define VREF 5000     //VREF (mv)
#define ADC_RES 65535 //ADC Resolution

//Single-point calibration Mode=0
//Two-point calibration Mode=1
#define TWO_POINT_CALIBRATION 0

#define READ_TEMP (25) //Current water temperature ℃, Or temperature sensor function

//Single point calibration needs to be filled CAL1_V and CAL1_T
#define CAL1_V (70) //mv
#define CAL1_T (22) //℃
//Two-point calibration needs to be filled CAL2_V and CAL2_T
//CAL1 High temperature point, CAL2 Low temperature point
#define CAL2_V (1300) //mv
#define CAL2_T (15)   //℃

String serverUrl = "http://ema-backend-app.herokuapp.com/sensors";
String apiKeyValue = "tPmAT5Ab3j7F9";
String sensorId = "001";
char auth[] = "VOgviAPpjrv3SaZATDB9Ig7G_2Fqh_OI";

float calibration_value = 18.49; // ph

const int oneWireBus = 0; // GPIO where the DS18B20 is connected to

Adafruit_ADS1115 ads;

OneWire oneWire(oneWireBus); // Setup a oneWire instance to communicate with any OneWire devices

DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature sensor

BlynkTimer timer;

float readTemperature()
{
  sensors.requestTemperatures();

  return sensors.getTempCByIndex(0);
}

int16_t readDO(uint8_t temperature_c)
{
  const uint16_t DO_Table[41] = {
      14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
      11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
      9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
      7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};

  uint16_t ADC_Raw = ads.readADC_SingleEnded(0);
  float voltage_mv = ads.computeVolts(ADC_Raw) * 1000;
#if TWO_POINT_CALIBRATION == 0
  uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#else
  uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#endif
}

float readPH()
{
  unsigned long int avgValue; //Store the average value of the sensor feedback
  int buf[10], temp;

  for (int i = 0; i < 10; i++) //Get 10 sample value from the sensor for smooth the value
  {
    buf[i] = ads.readADC_SingleEnded(1);

    delay(10);
  }
  for (int i = 0; i < 9; i++) //sort the analog from small to large
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buf[i] > buf[j])
      {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++) //take the average value of 6 center sample
    avgValue += buf[i];
  avgValue /= 6;
  // float phValue=(float)avgValue*5.0/65535/6; //convert the analog into millivolt
  float phValue = ads.computeVolts(avgValue);    //convert the analog into millivolt
  phValue = -5.70 * phValue + calibration_value; //convert the millivolt into pH value

  return phValue;
}

void myTimerEvent()
{
  float temperature = readTemperature();
  uint8_t Temperaturet = (uint8_t)temperature;
  uint16_t DO = readDO(Temperaturet);
  float PH = readPH();

  Serial.println("Temperature: " + String(temperature));
  Serial.println("DO: " + String(DO));
  Serial.println("ph: " + String(PH));
  Serial.println("");

  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED))
  {

    Blynk.virtualWrite(V1, temperature);
    Blynk.virtualWrite(V2, DO);
    Blynk.virtualWrite(V3, PH);

    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    http.begin(client, serverUrl); //HTTP
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.print("[HTTP] POST...\n");

    String httpRequestData = "api_key=" + apiKeyValue + "&sensor_id=" + sensorId + "&temperature=" + String(temperature) + "&do=" + String(DO) + "&ph=" + String(PH) + "";

    // start connection and send HTTP header and body
    int httpCode = http.POST(httpRequestData);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK)
      {
        const String &payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    }
    else
    {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

void setup()
{
  // explicitly set mode, esp defaults to STA+AP
  WiFi.mode(WIFI_STA);
  // Start the Serial Monitor
  Serial.begin(115200);
  // Start the DS18B20 sensor
  sensors.begin();
  ads.begin();

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else
  {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  Blynk.config(auth);

  myTimerEvent();
  // read values every 10 minutes
  timer.setInterval(1000 * 60 * 10, myTimerEvent);
}

void loop()
{
  Blynk.run();
  timer.run();
}
