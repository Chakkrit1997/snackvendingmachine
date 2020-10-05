#include <Arduino.h>
//firebase
#include "FirebaseESP8266.h"
#define FIREBASE_HOST "snackvending-c37c3.firebaseio.com" //Without http:// or https:// schemes
#define FIREBASE_AUTH "K5BmUe8oKOTfEPh1FUTT9JkaqUrksbrlirNwjnSO"
// Server
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

// Hardware
#include <qrcode.h>
#include <SSD1306.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Define Firebase Data object
FirebaseData firebaseData;
FirebaseJson data;

// Config Wifi
const char *ssid = "Room215";
const char *password = "248163264";

// Config MQTT Server
const char *topic = "/server";              // topic ชื่อ /server
#define mqtt_server "soldier.cloudmqtt.com" // Server MQTT
#define mqtt_port 11970                     // Port MQTT
#define mqtt_user "snackvending"            // Username
#define mqtt_password "1234"                // Password
WiFiClient espClient;
PubSubClient client(espClient);

// Config KEYPAD
#define I2CADDR 0x20 // addressของkeypad
char hexaKeys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[4] = {0, 1, 2, 3}; //connect to the row pinouts of the keypad
byte colPins[4] = {4, 5, 6, 7}; //connect to the column pinouts of the keypad
//initialize an instance of class NewKeypad
Keypad_I2C customKeypad(makeKeymap(hexaKeys), rowPins, colPins, 4, 4, I2CADDR);

// Config SSD1306 QRCode
// SDA --> D2 SCL --> D1
SSD1306 display(0x3c, D2, D1);
QRcode qrcode(&display);

// Config LED
LiquidCrystal_I2C lcd(0x3f, 20, 4);

// Config Stepmotor
#define DIR D3
#define STEP_0 D4
#define ENMOTOR D5
#define SPR 200 //stepsPerRevolution = 200

void setup()
{
  // Declare pins as output:
  pinMode(ENMOTOR, OUTPUT);
  digitalWrite(ENMOTOR, HIGH);
  pinMode(STEP_0, OUTPUT);
  pinMode(DIR, OUTPUT);
  delay(10);

  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting...");
  delay(10);

  // SetuP connect wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // สร้างฟังก์ชันเมื่อมีการติดต่อจาก mqtt มา

  // Setup Keypad
  Wire.begin();         // GDY200622
  customKeypad.begin(); // GDY120705

  // Setup SSD1306
  display.init(); //oled
  display.clear();
  display.display();
  // enable debug qrcode
  // qrcode.debug();
  // Initialize QRcode display using library
  qrcode.init();
  // create qrcode
  qrcode.create("00020101021129370016A000000677010111011300669590877525802TH540511.00530376463048A1C");

  // Setup LCD
  lcd.begin();
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.clear();

  /*
    lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
    lcd.print("Welcome To");
    lcd.setCursor(0, 1); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("Snack VendingMachine");
    lcd.setCursor(2, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("Payment via");
    lcd.setCursor(3, 3); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("QR Code");
    delay(1000);
  */
  //ESP.wdtDisable();
  //ESP.wdtEnable(WDTO_8S);
}

char snack1;

void loop()
{
WELLCOME:
  //lcd.clear();
  lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
  lcd.print("Snack VendingMachine");
  lcd.setCursor(0, 1); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
  lcd.print("Payment via QR Code");
  lcd.setCursor(0, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
  lcd.print("WiFiStatus:");
  if (WiFi.status() == WL_CONNECTED)
  {
    lcd.setCursor(12, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("OK!");
  }
  else
  {
    lcd.setCursor(12, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("NO!");
  }

  if (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password))
    {
      client.subscribe(topic); // ชื่อ topic ที่ต้องการติดตาม
      Serial.println("connected");
      lcd.setCursor(0, 3); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
      lcd.print("MQTT: CONNECTED!");
    }
    else
    { // ในกรณีเชื่อมต่อ mqtt ไม่สำเร็จ
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      lcd.setCursor(0, 3); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
      lcd.print("MQTT: FAILED!");
      delay(5000); // หน่วงเวลา 5 วินาที แล้วลองใหม่
      return;
    }
  }

  //

  char key = customKeypad.waitForKey();
  Serial.println(key);
  if (key == '#')
  {
  SELECTSNACK:
    lcd.clear();
    lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
    lcd.print("Select Snack");
    Serial.println("Select Snack");
    key = NULL;
    char key = customKeypad.waitForKey();
    Serial.println(key);
    if (key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '*' && key != '#' && key != '0')
    {

      String text = "/nowsnack/s" + String(key);
      Serial.println(text);

      if (Firebase.getInt(firebaseData, "/nowsnack/s" + String(key)))
      {
        
        int nowsnacknum = firebaseData.intData();
        Serial.println(firebaseData.intData());
      SELECTNUMOFSNACK:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Number of sweets");
        lcd.setCursor(0, 1);
        lcd.print(nowsnacknum);
        Serial.println("Number of sweets");
        key = NULL;
        char key = customKeypad.waitForKey();
        Serial.println(key);
        if (key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '*' && key != '#' && key != '0')
        {
          int keyy = key-48;
          Serial.println(keyy);
          if (keyy <= nowsnacknum)
          {
            lcd.clear();
            lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
            lcd.print("Success");
            delay(1000);
          }
          else
          {
            lcd.clear();
            lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
            lcd.print("Not enought");
            delay(1000);
            goto SELECTSNACK;
          }
        }
        else if (key == '*')
        {
          goto SELECTSNACK;
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
          lcd.print("Enter 1-9");
          Serial.println("Enter 9-9");
          delay(1000);
          goto SELECTNUMOFSNACK;
        }

        //Serial.println(firebaseData.intData());
      }
      else
      {
        lcd.clear();
        lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
        lcd.print("FireBase Error");
        Serial.println("Error : " + firebaseData.errorReason());
        delay(2000);
        goto WELLCOME;
      }
    }
    else if (key == '*')
    {
      goto WELLCOME;
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
      lcd.print("Enter 1-9");
      Serial.println("Enter 1-9");
      delay(1000);
      goto SELECTSNACK;
    }

    //char num = customKeypad.waitForKey();
  }

  //delay(100);

  /*char key = customKeypad.getKey();
  if (key)
  {
    key = NULL;
    lcd.clear();
    lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
    lcd.print("Select Snack");
    Serial.println("Select Snack");

    //Serial.println(key);
    while (key != '*')
    {
      key = customKeypad.getKey();
      if (key)
      {
        snack1 = key;
        lcd.setCursor(0, 1); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
        lcd.print(snack1);
        Serial.println(key);
        //break;
      }

      client.loop();
      delay(50);
    }
    Serial.println(key);
  }*/

  client.loop();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  int i = 0;
  while (i < length)
  {
    msg += (char)payload[i++];
  }
  Serial.print("receive ");
  Serial.println(msg); // แสดงข้อความที่ได้รับจาก topic

  if (msg != "0")
  {
    int roundd = msg.toInt();
    step(HIGH, STEP_0, roundd); // Set the spinning direction clockwise:
    delay(100);
    client.publish("/NodeMCU", "STEPED!"); // ส่งข้อความกลับไปที่ topic คือ ชื่ออุปกรณ์ที่ส่ง , ข้อความ
    Serial.println("Publish !");
    return;
  }
}

void step(boolean directionz, byte stepperPin, int steps)
{
  digitalWrite(ENMOTOR, LOW); // เปิดให้มอเตอร์ทำงาน
  digitalWrite(DIR, directionz);
  delay(100);
  //int roundd = steps * 200;
  for (int j = 0; j < steps; j++)
  {
    for (int i = 0; i < SPR; i++)
    {
      digitalWrite(stepperPin, HIGH);
      delayMicroseconds(1800);
      digitalWrite(stepperPin, LOW);
      delayMicroseconds(1800);
    }
    delay(500);
  }
  delay(100);
  digitalWrite(ENMOTOR, HIGH); // ปิดมอเตอร์
}