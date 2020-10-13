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
FirebaseJson nowbuy;

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
  //qrcode.create("00020101021129370016A000000677010111011300669590877525802TH540511.00530376463048A1C");

  // Setup LCD
  lcd.begin();
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.clear();
  //ESP.wdtDisable();
  //ESP.wdtEnable(WDTO_8S);

  // SetuP connect wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting ");
  lcd.setCursor(0, 1);
  lcd.print("to Network ...");

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
}

void loop()
{
WELLCOME:
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Snack VendingMachine");
  lcd.setCursor(0, 1);
  lcd.print("Payment via QR Code");

  /*lcd.setCursor(0, 2);
  lcd.print("WIFI:"); //Network : CONNECTED
  if (WiFi.status() == WL_CONNECTED)
  {
    lcd.setCursor(5, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print(" OK!");
  }
  else
  {
    lcd.setCursor(5, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print(" NO!");
  }*/

  if (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password))
    {
      client.subscribe(topic); // ชื่อ topic ที่ต้องการติดตาม
      Serial.println("connected");
      lcd.setCursor(0, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
      lcd.print("STATUS : OK!");
    }
    else
    { // ในกรณีเชื่อมต่อ mqtt ไม่สำเร็จ
      lcd.setCursor(0, 2);
      lcd.print("STATUS : NO!");
      lcd.setCursor(0, 3);
      lcd.print("Connecting......");
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // หน่วงเวลา 5 วินาที แล้วลองใหม่
      return;
    }
  }
  else
  {
    Serial.println("connected");
    lcd.setCursor(0, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
    lcd.print("STATUS : OK!");
  }
  lcd.setCursor(0, 3); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
  lcd.print("Press # to Order");

  Firebase.deleteNode(firebaseData, "/nowbuy/");
  char key = customKeypad.waitForKey();
  Serial.println(key);
  if (key == '#')
  {
    //prepare

  SELECTSNACK:
    lcd.clear();
    lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
    lcd.print("Select Snack");
    lcd.setCursor(0, 3);
    lcd.print("[*]Back");
    Serial.print("Select Snack");
    //key = NULL;
    char select_key = customKeypad.waitForKey();
    Serial.println(select_key);
    if (select_key != 'A' && select_key != 'B' && select_key != 'C' && select_key != 'D' && select_key != '*' && select_key != '#')
    //Check select_key is number0-9
    {
      //String text = "/nowsnack/s" + String(key);
      //Serial.println(text); //Debug
      //FETCH DATA TO JSON
      if (Firebase.getJSON(firebaseData, "/nowsnack"))
      {
        FirebaseJson &json = firebaseData.jsonObject();
        FirebaseJsonData data;
        json.get(data, "/s" + String(select_key) + "/amount");
        int amount = data.intValue;
        json.get(data, "/s" + String(select_key) + "/price");
        int price = data.intValue;
      SELECTNUMOFSNACK:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter Amount");
        lcd.setCursor(2, 1);
        lcd.print(amount);
        lcd.setCursor(5, 1);
        lcd.print("Price left");
        lcd.setCursor(2, 2);
        lcd.print(price);
        lcd.setCursor(5, 2);
        lcd.print("Bath/Price");

        lcd.setCursor(0, 3);
        lcd.print("[*]Back");
        //Serial.print("now_amount");
        Serial.print(amount);
        //Serial.print("now_price");
        Serial.print(price);
        //key = NULL;
        char nos_key = customKeypad.waitForKey(); //รอรับค่า จำนวนขนมที่ลูกค้าต้องการ
        Serial.println(nos_key);
        if (nos_key != 'A' && nos_key != 'B' && nos_key != 'C' && nos_key != 'D' && nos_key != '*' && nos_key != '#' && nos_key != '0')
        {
          int i_nos_key = (int)nos_key - 48;
          //Serial.println(i_nos_key); //debug
          if (i_nos_key <= amount) //เช็คว่ามีขนมพอไหม?
          {
            if (Firebase.setInt(firebaseData, "/nowbuy/s" + String(select_key) + "/amount", i_nos_key))
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Please wait!");
              Serial.println("Success!");
              int cost = (i_nos_key * price);
              Firebase.setInt(firebaseData, "/nowbuy/s" + String(select_key) + "/cost", cost);
              delay(1000);
            }

            //nowbuy.set("s" + String(select_key) + "/amount", i_nos_key); //set amount = key
            //nowbuy.clear();
            //FirebaseJson data_test;
            //nowbuy.get(data_test,"",)

            /*if (Firebase.setJSON(firebaseData, "/nowbuy", nowbuy)) // อัพลงfirebase
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Success!");
              Serial.println("Success!");
              delay(1000);
            }*/

            else //ถ้าอัพลงไม่ได้
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print(" FireBase Error ");
              Serial.println("Error : " + firebaseData.errorReason());
              delay(2000);
              goto WELLCOME;
            }

          CONFIRM:
            int total_buy = 0;
            if (Firebase.getJSON(firebaseData, "/nowbuy"))
            {
              FirebaseJsonData data_nowbuy;
              for (size_t j = 0; j <= 9; j++)
              {
                json.get(data_nowbuy, "/s" + String(j) + "/cost");
                total_buy += data_nowbuy.intValue;
              }
              //Firebase.setInt(firebaseData, "/nowbuy/total_buy", total_buy);
              //int total_buy = 0;
            }
            else
            {
              lcd.clear();
              lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
              lcd.print(" FireBase Error ");
              Serial.println("Error : " + firebaseData.errorReason());
              delay(2000);
              goto WELLCOME;
            }

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Total : ");
            lcd.setCursor(8, 0);
            lcd.print(total_buy);
            lcd.setCursor(12, 0);
            lcd.print("Bath.");
            lcd.setCursor(0, 1);
            lcd.print("[A]BuyMore/Change");
            lcd.setCursor(0, 2);
            lcd.print("[B]CheckOrder");
            lcd.setCursor(0, 3);
            lcd.print("[*]Cancel [#]Confirm");
            char confirm_key = customKeypad.waitForKey(); //รอรับค่า
            Serial.println(confirm_key);
            if (confirm_key == '*')
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Order canceled!");
              if (Firebase.deleteNode(firebaseData, "/nowbuy/"))
              {
                Serial.println("Deleted");
              }
              else
              {
                Serial.println("Error : " + firebaseData.errorReason());
              }
              goto WELLCOME;
            }
            else if (confirm_key == 'A')
            {
              goto SELECTSNACK;
            }
            else if (confirm_key == '#')
            {
              /* code */
            }
            else
            {
              lcd.clear();
              lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
              lcd.print("Entered incorrectly!");
              Serial.println("Enter A*#");
              //delay(500);
              goto CONFIRM;
            }
          }
          else //ถ้ามีขนมไม่พอ
          {
            lcd.clear();
            lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
            lcd.print("Not enought");
            delay(1000);
            goto SELECTSNACK;
          }
        }
        else if (nos_key == '*')
        {
          goto SELECTSNACK;
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
          lcd.print("Enter 1-9");
          Serial.println("Enter 1-9");
          delay(1000);
          goto SELECTNUMOFSNACK;
        }

        // Do something
      }
      else
      {
        lcd.clear();
        lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
        lcd.print(" FireBase Error ");
        Serial.println("Error : " + firebaseData.errorReason());
        delay(2000);
        goto WELLCOME;
      }
    }
    else if (select_key == '*')
    {
      goto WELLCOME;
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
      lcd.print("Enter 0-9");
      Serial.println("Enter 0-9");
      delay(1000);
      goto SELECTSNACK;
    }
  }

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