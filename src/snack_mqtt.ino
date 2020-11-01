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
// MCP23017
#include "Adafruit_MCP23017.h"
Adafruit_MCP23017 mcp;

//SDCARDPLAYER
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
SoftwareSerial mySoftwareSerial(D5, D6); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

//Define Firebase Data object
FirebaseData firebaseData;
FirebaseData data_nowsnack;
FirebaseJson nowbuy;

// Config Wifi
const char *ssid = "Room215";
const char *password = "248163264";

// Config MQTT Server
//const char *topic = "/server/qrtext";       // topic ชื่อ /server
#define mqtt_server "soldier.cloudmqtt.com" // Server MQTT
#define mqtt_port 11970                     // Port MQTT
#define mqtt_user "snackvending"            // Username
#define mqtt_password "1234"                // Password
WiFiClient espClient;
PubSubClient client(espClient);

// Config KEYPAD
#define I2CADDR 0x24 // addressของkeypad
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
#define SPR 200 //stepsPerRevolution = 200
#define EN_MOTOR 10
#define DIR_MOTOR 11

bool qr_confirm;

void setup()
{
  // Declare pins as output:
  //pinMode(ENMOTOR, OUTPUT);
  //digitalWrite(ENMOTOR, HIGH);
  //pinMode(STEP_0, OUTPUT);
  //pinMode(DIR, OUTPUT);
  delay(10);
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting...");
  delay(10);

  //Setup DF player
  mySoftwareSerial.begin(9600);
  delay(100);
  if (!myDFPlayer.begin(mySoftwareSerial))
  { //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    //while (true);
  }
  delay(10);
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms
  myDFPlayer.volume(30);      //Set volume value (0~30).
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  //myDFPlayer.enableDAC();  //Enable On-chip DAC
  //----Mp3 play----
  //Serial.println(F("play prated gu me"));
  //myDFPlayer.play(2); //Play the first mp3
  delay(100);
  if (myDFPlayer.available())
  {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }

  // Setup Keypad
  Wire.begin();         // GDY200622
  customKeypad.begin(); // GDY120705
  mcp.begin();          // use default address 0 = 0x21
  for (size_t i = 0; i < 10; i++)
  {
    mcp.pinMode(i, OUTPUT);
    //mcp.digitalWrite(i, LOW);
  }
  mcp.pinMode(EN_MOTOR, OUTPUT);    //10
  mcp.pinMode(DIR_MOTOR, OUTPUT);   // 11
  mcp.digitalWrite(EN_MOTOR, HIGH); // 10 มอเตอร์ active low

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
  client.setServer(mqtt_server, mqtt_port); // เชื่อมต่อ mqtt server
  client.setCallback(callback);             // สร้างฟังก์ชันเมื่อมีการติดต่อจาก mqtt มา
}

void loop()
{
WELLCOME:
  myDFPlayer.play(1);
  Serial.println("welcome !!");
  lcd.clear();
  display.clear();
  display.display();
  //qrcode.init();
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
  Serial.print("client.connected = ");
  Serial.println(client.connected());
  if (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password))
    {
      client.subscribe("/server/qrtext"); // ชื่อ topic ที่ต้องการติดตาม
      //client.subscribe(topic); // ชื่อ topic ที่ต้องการติดตาม
      Serial.println("connected_first");
      lcd.setCursor(0, 2); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
      lcd.print("STATUS : READY!");
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
    lcd.setCursor(0, 2);
    lcd.print("STATUS : OK!");
  }

  Firebase.deleteNode(firebaseData, "/nowbuy/");
  Firebase.getJSON(data_nowsnack, "/nowsnack");
  FirebaseJson &json_nowsnack = data_nowsnack.jsonObject();
  FirebaseJsonData jsondata_nowsnack;

  lcd.setCursor(0, 3); // ไปที่ตัวอักษรที่ 6 แถวที่ 2
  lcd.print("Press # to Order");

  char key = customKeypad.waitForKey();
  Serial.println(key);
  if (key == '#')
  {
    //prepare
    nowbuy.clear(); //clear jsonnowbuy
  SELECTSNACK:
    myDFPlayer.play(2);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Select Snack");
    lcd.setCursor(0, 3);
    //lcd.
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
      json_nowsnack.get(jsondata_nowsnack, "/s" + String(select_key) + "/amount");
      int amount = jsondata_nowsnack.intValue;
      json_nowsnack.get(jsondata_nowsnack, "/s" + String(select_key) + "/price");
      int price = jsondata_nowsnack.intValue;
    SELECTNUMOFSNACK:
      myDFPlayer.play(3);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Choose Amount");
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

          int cost = (i_nos_key * price);
          nowbuy.set("s" + String(select_key) + "/amount", i_nos_key); //set amount = key
          nowbuy.set("s" + String(select_key) + "/cost", cost);

        CONFIRM:
          int total_buy = 0;
          FirebaseJsonData data_nowbuy;
          for (unsigned int j = 0; j <= 9; j++)
          {
            nowbuy.get(data_nowbuy, "/s" + String(j) + "/cost");
            total_buy += data_nowbuy.intValue;
            nowbuy.set("/total_buy", total_buy);
          }

          /*if (Firebase.getJSON(firebaseData, "/nowbuy"))
          {
            FirebaseJson &json = firebaseData.jsonObject();
            FirebaseJsonData data_nowbuy;

            for (size_t j = 0; j <= 9; j++)
            {
              json.get(data_nowbuy, "/s" + String(j) + "/cost");
              total_buy += data_nowbuy.intValue;
            }
          }
          else
          {
            lcd.clear();
            lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
            lcd.print(" FireBase Error ");
            Serial.println("Error : " + firebaseData.errorReason());
            delay(2000);
            goto WELLCOME;
          }*/

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
          else if (confirm_key == 'B')
          {
          CHECKORDER:
            lcd.clear();
            lcd.setCursor(0, 0);
            for (size_t i = 0; i <= 4; i++)
            {
              nowbuy.get(data_nowbuy, "/s" + String(i) + "/amount");
              String text = ":" + String(data_nowbuy.intValue) + " ";
              lcd.print(i);
              lcd.print(text);
            }
            lcd.setCursor(0, 1);
            for (size_t i = 5; i <= 9; i++)
            {
              nowbuy.get(data_nowbuy, "/s" + String(i) + "/amount");
              String text = ":" + String(data_nowbuy.intValue) + " ";
              lcd.print(i);
              lcd.print(text);
            }
            lcd.setCursor(0, 3);
            lcd.print("[*]Back ");
            char close_key = customKeypad.waitForKey();
            if (close_key != '*')
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Entered incorrectly!");
              delay(500);
              goto CHECKORDER;
            }
            else
            {
              goto CONFIRM;
            }
          }
          else if (confirm_key == '#') //ถ้ากดปุ่ม#หลังจากConfirm
          {
            if (Firebase.setJSON(firebaseData, "/nowbuy", nowbuy))
            {
              myDFPlayer.play(4);
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Processing!");
              Serial.println("Processing!");

              String s_totalbuy = String(total_buy);
              unsigned int str_len = s_totalbuy.length() + 1;
              char c_totalbuy[str_len];
              s_totalbuy.toCharArray(c_totalbuy, str_len);

              reconnect_mqtt();

              client.publish("/NodeMCU/cost", c_totalbuy);
              Serial.println("cost publish!");
              
              do
              {
                qr_confirm = false;
                client.loop();
              } while (qr_confirm == false);
              step(HIGH, 0, 1); // clockwise, number, round
              char close_key = customKeypad.waitForKey();
            }
            else
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print(" FireBase Error ");
              Serial.println("Error : " + firebaseData.errorReason());
              delay(2000);
              goto WELLCOME;
            }
          }
          else //ถ้าพิมนอกจาก A,B,*,#
          {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Entered incorrectly!");
            Serial.println("Enter AB*#");
            delay(500);
            goto CONFIRM;
          }
        }
        else //ถ้ามีขนมไม่พอ
        {
          lcd.clear();
          lcd.setCursor(0, 0); // ไปที่ตัวอักษรที่ 0 แถวที่ 1
          lcd.print("Not enought");
          delay(1000);
          goto SELECTNUMOFSNACK;
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

  /*while (client.connected())
  {
    client.loop();
    Serial.print("client.connected = ");
    Serial.println(client.connected());
    //Serial.print("client.loop return = ");
    Serial.println(client.loop());
    delay(1000);
  }*/

  Serial.println("end program");
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  unsigned int i = 0;
  while (i < length)
  {
    msg += (char)payload[i++];
  }
  Serial.print("receive ");
  Serial.println(msg); // แสดงข้อความที่ได้รับจาก topic

  if (strcmp(topic, "/server/qrtext") == 0)
  {
    //Serial.println(msg);
    qr_confirm = true;
    qrcode.create(msg);
  }

  /*if (msg != "0")
  {
    int roundd = msg.toInt();
    //step(HIGH, STEP_0, roundd); // Set the spinning direction clockwise:
    delay(100);
    client.publish("/NodeMCU", "STEPED!"); // ส่งข้อความกลับไปที่ topic คือ ชื่ออุปกรณ์ที่ส่ง , ข้อความ
    Serial.println("OKKK!");
    return;
  }*/
}

void step(boolean dir, byte stepperPin, int steps)
{
  mcp.digitalWrite(EN_MOTOR, LOW); // เปิดให้มอเตอร์ทำงาน
  mcp.digitalWrite(DIR_MOTOR, dir);
  delay(100);
  //int roundd = steps * 200;
  for (int j = 0; j < steps; j++)
  {
    for (int i = 0; i < SPR; i++)
    {
      mcp.digitalWrite(stepperPin, HIGH);
      delayMicroseconds(3000);
      mcp.digitalWrite(stepperPin, LOW);
      delayMicroseconds(3000);
    }
    delay(500);
  }
  delay(100);
  mcp.digitalWrite(EN_MOTOR, HIGH); // ปิดมอเตอร์
}

void printDetail(uint8_t type, int value)
{
  switch (type)
  {
  case TimeOut:
    Serial.println(F("Time Out!"));
    break;
  case WrongStack:
    Serial.println(F("Stack Wrong!"));
    break;
  case DFPlayerCardInserted:
    Serial.println(F("Card Inserted!"));
    break;
  case DFPlayerCardRemoved:
    Serial.println(F("Card Removed!"));
    break;
  case DFPlayerCardOnline:
    Serial.println(F("Card Online!"));
    break;
  case DFPlayerUSBInserted:
    Serial.println("USB Inserted!");
    break;
  case DFPlayerUSBRemoved:
    Serial.println("USB Removed!");
    break;
  case DFPlayerPlayFinished:
    Serial.print(F("Number:"));
    Serial.print(value);
    Serial.println(F(" Play Finished!"));
    break;
  case DFPlayerError:
    Serial.print(F("DFPlayerError:"));
    switch (value)
    {
    case Busy:
      Serial.println(F("Card not found"));
      break;
    case Sleeping:
      Serial.println(F("Sleeping"));
      break;
    case SerialWrongStack:
      Serial.println(F("Get Wrong Stack"));
      break;
    case CheckSumNotMatch:
      Serial.println(F("Check Sum Not Match"));
      break;
    case FileIndexOut:
      Serial.println(F("File Index Out of Bound"));
      break;
    case FileMismatch:
      Serial.println(F("Cannot Find File"));
      break;
    case Advertise:
      Serial.println(F("In Advertise"));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void reconnect_mqtt()
{
  if (!client.connected())
  {
    Serial.print("Re connecting...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password))
    {
      client.subscribe("/server/qrtext"); // ชื่อ topic ที่ต้องการติดตาม
      //client.subscribe(topic); // ชื่อ topic ที่ต้องการติดตาม
      Serial.println("reconnected!");
    }
    else
    { // ในกรณีเชื่อมต่อ mqtt ไม่สำเร็จ
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // หน่วงเวลา 5 วินาที แล้วลองใหม่
      return;
    }
  }
  else
  {
    //client.loop();
    Serial.println("You are connected");
  }
}