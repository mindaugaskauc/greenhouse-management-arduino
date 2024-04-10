#include <Arduino.h>
#include <dht.h> /* for weather temperature sensor */
#include <OneWire.h> /* for soil temperature sensor */
#include <DallasTemperature.h> /* for soil temperature sensor */
#include <AltSoftSerial.h> /* for soil acidity sensor */
#include <Wire.h> /* for soil acidity sensor */


#define SERIAL_SPEED 115200
//#define SERIAL_SPEED 9600

//Set interval of watering and measuring sensor data
#define INTERVAL_WATER 10000
#define INTERVAL_MEASURE 11000


/* Field temperature with DHT22 sensor */
//Define Constants 
#define dataPin 3 // Defines digital pin number to which the sensor is connected
dht DHT; // create a DHT object

/* TMP */
/*Soil temperature oneWire sensor */
// Data wire is plugged into digital pin 2 on the arduino
#define ONE_WIRE_BUS 2 //Defines pin number for soil temperature sensor
// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);
// Location of the sensor on the bus
#define TMP_SENSOR_ID 0 

/* Soil acidity */
// RO to pin 8 & DI to pin 9 when using AltSoftSerial
#define RE 6
#define DE 7
 
const byte soil_ph[] = {0x01, 0x03, 0x00, 0x06, 0x00, 0x01, 0x64, 0x0b};
byte values[11];
AltSoftSerial mod;

/* Status */
#define R_OK 0
#define R_ERR 1

// #define DEBUG

//OneWire oneWire(ONE_WIRE_BUS);
//DallasTemperature sensors(&oneWire);
//#define dataPin 3 // Defines pin number to which the sensor is connected

//Field temperature initial limit
float weather_t_upper_limit = 0.0;
float weather_t_lower_limit = 0.0;

//Soil temperature initial limit
float soil_t_upper_limit = 0;
float soil_t_lower_limit = 0;

// soil moisture initial limit
int soil_h_upper_limit = 0;
int soil_h_lower_limit = 0;


//int moisturePin = A0;
//int moistureValue = (100 - (moisturePin / 1023.00) * 100 )); //convert analogue value to percentage

//Watering variables
int motorPin = 4; // pin that turns on the motor
int blinkPin = 13; // pin that turns on the led;
int watertime = 5; // how long it will be watering (in seconds)
int waittime = 1; // how long to wait between watering (in minutes)
int motorState; // set motor logic state 1 or (High/Low)/motor state used to turn on and off motor
int ledState; // ledState used to set the led
//String waterOn = "1";
//String waterOff = "0";
//set watering initial value
String water_value = " ";

//Open-close doors variables
const int forwards = 10;
const int backwards = 11;//assign relay INx pin to arduino pin
String window_value = " ";


void setup()
{
  Serial.begin(SERIAL_SPEED);
  //-------------Soil Acidity Sensor Setup Start----------------------
  /* mod.begin() for soil acidity sensor */
  mod.begin(9600);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);

   // put RS-485 into receive mode
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);

  delay( 1000 );

  //----------Soil Acidity Sensor Setup End-------------------------
  
  /* Watering setup */
  pinMode(motorPin, OUTPUT); // set PIN 4 to an output
  pinMode(blinkPin, OUTPUT); // set PIN 13 to an output

  // Open-Close windows setup
  pinMode(forwards, OUTPUT);//set relay as an output
  pinMode(backwards, OUTPUT);//set relay as an output

  /* TMP */
  sensors.begin();

}

/* Writes "OK" to the console if success, otherwise "NOK" */
void print_resp(int cmd)
{
  if (cmd == R_OK)
    Serial.println("OK");
  else
    Serial.println("NOK");
}

void handle_weather_temp_status()
{
  Serial.print("weather_t_LOWER "); 
  Serial.println(weather_t_lower_limit);
  Serial.print("weather_t_UPPER ");
  Serial.println(weather_t_upper_limit);
}

void handle_soil_temp_status()
{
  Serial.print("soil_t_LOWER ");
  Serial.println(soil_t_lower_limit);
  Serial.print("soil_t_UPPER ");
  Serial.println(soil_t_upper_limit);
}

void handle_soil_hum_status()
{
  Serial.print("soil_h_LOWER ");
  Serial.println(soil_h_lower_limit);
  Serial.print("soil_h_UPPER ");
  Serial.println(soil_h_upper_limit);
}


/* Restarts program from beginning but does not reset the peripherals and
   registers */
void handle_reset()
{
  asm volatile("  jmp 0");
}

// Set window command
int handle_set_window(String window_recv)
{
 // char soil_h_mode;
 // float soil_h_value;
  String window_val_str;

  /* Drop messages shorter then 8 symbols - WATER ON has 8 symbols, so the string length must be 8 except for the last symbole /0
  https://www.scaler.com/topics/string-length-in-c/ */
  if (window_recv.length() < 12) //vietoj soil_h_recv
    return R_ERR;

 // https://www.geeksforgeeks.org/get-a-substring-in-c/ positions start from first letter W (pos 0 is first symbol in a string) in this ca
  window_val_str = window_recv.substring(0); //vietoj soil_h_value_str = soil_h_recv. start reading value from pos 0
  //soil_h_value = soil_h_value_str.toFloat();
  window_value = window_val_str;

  return R_OK;
}

// Set watering command
int handle_set_water(String water_recv)
{
 // char soil_h_mode;
 // float soil_h_value;
  String water_val_str;

  /* Drop messages shorter then 8 symbols - WATER ON has 8 symbols, so the string length must be 8 except for the last symbole /0
  https://www.scaler.com/topics/string-length-in-c/ */
  if (water_recv.length() < 8) 
    return R_ERR;

 // soil_h_mode = soil_h_recv[4];
 // https://www.geeksforgeeks.org/get-a-substring-in-c/ positions start from first letter W (pos 0 is first symbol in a string) in this ca
  water_val_str = water_recv.substring(0);
  //soil_h_value = soil_h_value_str.toFloat();
  water_value = water_val_str;

  return R_OK;
}

// Set upper and lower weather temperature
int handle_seta_weather(String weather_t_recv) 
{
  unsigned int weather_t_cursor_pos;
  String weather_t_value1_str;
  String weather_t_value2_str;
  float weather_t_value1;
  float weather_t_value2;

  /* Drop messages shorter then 12 symbols */
  if (weather_t_recv.length() < 12 || weather_t_recv[4] != 'W')
    return R_ERR;

 
  // first simbol should start here
  weather_t_cursor_pos = 10;
  for (unsigned int i = weather_t_cursor_pos; i < weather_t_recv.length(); i++) {
    if (isspace(weather_t_recv[i])) {
      weather_t_value1_str = weather_t_recv.substring(weather_t_cursor_pos, i);
      weather_t_cursor_pos = i + 1;
      break;
    }
  }

  weather_t_value2_str = weather_t_recv.substring(weather_t_cursor_pos);

  weather_t_value1 = weather_t_value1_str.toFloat();
  weather_t_value2 = weather_t_value2_str.toFloat();

   /* HACK: to avoid error then value equals to 0, since toFloat()
     returns 0 on error */
  if (weather_t_value1 == 0 && weather_t_value1_str != "0") 
    return R_ERR;

     /* HACK: to avoid error then value equals to 0, since toFloat()
     returns 0 on error */
  if (weather_t_value2 == 0 && weather_t_value2_str != "0")
    return R_ERR;

  if (weather_t_value1 > weather_t_value2)
    return R_ERR;

  weather_t_lower_limit = weather_t_value1;
  weather_t_upper_limit = weather_t_value2;

  return R_OK;

}

// Set upper and lower soil humidity
int handle_seta_hum(String soil_h_recv)
{
  unsigned int soil_h_cursor_pos;
  String soil_h_value1_str;
  String soil_h_value2_str;
  int soil_h_value1;
  int soil_h_value2;

  /* Drop messages shorter then 11 symbols */
  if (soil_h_recv.length() < 11 || soil_h_recv[4] != 'H')
    return R_ERR;

  // first simbol should start here
  soil_h_cursor_pos = 8;
  for (unsigned int i = soil_h_cursor_pos; i < soil_h_recv.length(); i++) {
    if (isspace(soil_h_recv[i])) {
      soil_h_value1_str = soil_h_recv.substring(soil_h_cursor_pos, i);
      soil_h_cursor_pos = i + 1;
      break;
    }
  }

  soil_h_value2_str = soil_h_recv.substring(soil_h_cursor_pos);

  soil_h_value1 = soil_h_value1_str.toInt();
  soil_h_value2 = soil_h_value2_str.toInt();

  if (soil_h_value1 > soil_h_value2)
    return R_ERR;

  soil_h_lower_limit = soil_h_value1;
  soil_h_upper_limit = soil_h_value2;

  return R_OK;
}

void loop()
{
  String recv; //variable which receives string values from function readStringUntil
  float soil_tmp; //soil temperature
  String soil_t_recv;
  
  int readData; // weather temperature
  float weather_tmp; 
  String weather_t_recv; 
  int soil_hum; // soil humidity
  String soil_h_recv;

  /* soil acidity */
  float soil_ph_recv;
  
                                         /* Reading sensor values */
  

  //--------------------Read weather temperature-------------------------------------------------
  readData = DHT.read22(dataPin); // Reads the data from the sensor DHT22
  weather_tmp = constrain (DHT.temperature, -100, 60); // Gets the values of the weather temperature vietoj field_tmp

  //--------------------Read soil temperature----------------------------------------------------
  //Send command to get temperature
  sensors.requestTemperatures();
  // assignt soil temperature in Celsius (TMP_SENROS_ID ==0 or write 0) to the variable soil_tmp
  soil_tmp = sensors.getTempCByIndex(TMP_SENSOR_ID);
  
  //--------------------Read soil humidity------------------------

  int moisturePin = analogRead(A0);
  soil_hum = ( 100 - ( ( moisturePin / 1023.00) * 100 ) ); // convert analog value to percentage

  //------------Read soil acidity----------------------------------
  Serial.print("  PH: ");
  soil_ph_recv = ph();
  Serial.print(" = ");
  Serial.print(soil_ph_recv);
  Serial.println();

  delay(1000);
//---------------Read Soil Acidity----------------------------------

                                      /* Show sensor values in monitor */

//------------------Show weather temperature------------------------------------------
#ifdef DEBUG
  Serial.print("weather_tmp = "); 
  Serial.print(weather_tmp);
  Serial.print("; weather_t_lower = ");
  Serial.print(weather_t_lower_limit);
  Serial.print("; weather_t_upper = ");
  Serial.println(weather_t_upper_limit);
#else
  if (DHT.temperature > -100) {
  Serial.print(F("{\"weather_temperature\": ")); //Read weather temperature
  Serial.print(weather_tmp);
  }
#endif 
//------------------------------------------------------------------------------------- 

//--------------Show soil temperature--------------------------------------------------
// if the limit is set print
#ifdef DEBUG
  // Print soil temperature text
  Serial.print("soil_tmp = ");
  // Print soil temperature
  Serial.print(soil_tmp); //Print soil temperature
  Serial.print("; soil_t_lower = ");
  Serial.print(soil_t_lower_limit);
  Serial.print("; soil_t_upper = ");
  Serial.println(soil_t_upper_limit);
#else
// Else print
  Serial.print(F(", \"soil_temperature\": ")); //Read soil temperature
  Serial.print(soil_tmp); // Print soil temperature
#endif
//--------------------------------------------------------------------------------------

//-----------Show soil humidity---------------------------------------------------------
#ifdef DEBUG
  Serial.print("soil_hum = ");
  Serial.print(soil_hum);
  Serial.print("; soil_h_lower = ");
  Serial.print(soil_h_lower_limit);
  Serial.print("; soil_h_upper = ");
  Serial.println(soil_h_upper_limit);
#else
  Serial.print(F(", \"soil_humidity\": ")); //Read soil humidity
  Serial.print(soil_hum);
#endif
//-------------------------------------------------------------------------------------

//----------Show soil pH---------------------------------------------------------------
  Serial.print(F(", \"soil_pH\": ")); //Read soil humidity
  Serial.print(soil_ph_recv);
  Serial.println(F("}"));

//-------------------------------------------------------------------------------------
   // Turn on and off watering and get humidity settings
  if (Serial.available() > 0) {
     recv = Serial.readStringUntil('\n');
     Serial.print("recv: ");
     Serial.println(recv);
      // if recv starts with WINDOW
    if(recv.startsWith("WINDOW")) {
      print_resp(handle_set_window(recv));    
     }
      // if recv starts with WATER     
     else if(recv.startsWith("WATER")) {
       // water_on(100);
       print_resp(handle_set_water(recv));      
     }
      // if recv starts with set
     else if(recv.startsWith("SET WTEMP")) { 
        print_resp(handle_seta_weather(recv));
     }
      // if recv starts with set
     else if(recv.startsWith("SET HUM")) { 
        print_resp(handle_seta_hum(recv));
     }
  }

   /* if can not read weather temperature */
  if (weather_tmp == -999) {
    return;
  }

  /* Can not read soil humidity */
  if (soil_hum == -127) {
    return;
  }

  /* Can not read temperature */
  if (soil_tmp == -127) {
    return;
  }

//---------------------------------Opening and Closing Windows----------------------------------
     //Print the window value
    Serial.print("window_value is: ");
    Serial.println(window_value);
     // Open window if window value "WINDOW OPENS"
    // if (window_value.equals("WINDOW OPENS") && digitalRead(forwards) == LOW) {
     if (window_value.equals("WINDOW OPENS")) {
         digitalWrite(forwards, HIGH);
         digitalWrite(backwards, LOW);//Activate the relay one direction, they must be different to move the motor
         delay(6000);
     }
    // Close window if windo value is "WATER OFF"
    //if (window_value.equals("WINDOW CLOSES") && digitalRead(backwards) == LOW) {
     else if (window_value.equals("WINDOW CLOSES")) {
        digitalWrite(forwards, LOW);
        digitalWrite(backwards, HIGH);//Activate the relay one direction, they must be different to move the motor
        delay(6000);
     } 
//----------------------------------END of Opening and Closing Windows--------------------------

//----------------------------------Opening and closing windows by weather temperature----------
// Print lower weather limit
Serial.print("Lower weather limit is: ");
Serial.println(weather_t_lower_limit);
//Print upper weather limit
Serial.print("Higher weather limit is: ");
Serial.println(weather_t_upper_limit);

//Convert upper and lower limits from float to int as  floats cannot be compared in C language
  // see https://forum.arduino.cc/t/solved-precision-when-comparing-floats/204787/3
  // Convert weather_t_lower_limit to int
  float weather_t_lower_limit_float = weather_t_lower_limit * 100;
  int weather_t_lower_limit_int = round(weather_t_lower_limit_float);

  //Convert weather_t_upper_limit to int
  float weather_t_upper_limit_float = weather_t_upper_limit * 100;
  int weather_t_upper_limit_int = round(weather_t_upper_limit_float);

  //Convert weather_tmp sensor value to int
  float weather_tmp_float = weather_tmp * 100;
  int weather_tmp_int = round(weather_tmp_float);


// Print lower weather limit
Serial.print("Lower weather limit int: ");
Serial.println(weather_t_lower_limit_int);
//Print upper weather limit
Serial.print("Higher weather int limit is: ");
Serial.println(weather_t_upper_limit_int);
// Int value of int is
Serial.print("TMP int value is ");
Serial.println(weather_tmp_int);

 /* if can not read weather temperature of int value (always equals -10000, then the variable weather_tmp_int does not return any value */
  if (weather_tmp_int == -10000) {
    return; // does not return any value
  }
    
    // if temp is higher than upper limit and window is closed, open the window
    /*if (weather_tmp > weather_t_upper_limit && digitalRead(backwards) == HIGH) { change back when new linear act. comes */
    if ( weather_tmp_int > weather_t_upper_limit_int && weather_t_upper_limit_int != 0 ) {
      digitalWrite(forwards, HIGH);
      digitalWrite(backwards, LOW);//Activate the relay one direction, they must be different to move the motor
      delay(6000);
    }
    // if temp is lower than lower limit and window is open, close the window
   /* else if (weather_tmp < weather_t_lower_limit && digitalRead(forwards) == HIGH) { // change back when new linear act comes */
    else if (weather_tmp_int < weather_t_lower_limit_int && weather_t_lower_limit_int != 0 ) {
      digitalWrite(forwards, LOW);
      digitalWrite(backwards, HIGH);//Activate the relay one direction, they must be different to move the motor
      delay(6000);
    }
    else if (weather_t_lower_limit_int == 0 && weather_t_lower_limit_int == 0 && !(window_value.equals("WINDOW OPENS")) ) {
      digitalWrite(forwards, LOW);
      digitalWrite(backwards, HIGH);//Activate the relay one direction, they must be different to move the motor
      delay(6000);
    }

//--------------------------------End of Opening and closing windows by weather temperature----------

//----------------------------------Watering On and Off-----------------------------------------
    //Print the water value
    Serial.print("water_value is: ");
    Serial.println(water_value);
     // Water if value "WATER ON"
     if (water_value.equals("WATER ON")) {
      //  water_on(10);
        digitalWrite(motorPin, HIGH); //turn on the motor
        digitalWrite(blinkPin, HIGH); //turn on the LED
        delay(watertime * 1000); // multiply by 1000 to translate seconds to miliseconds */
     }
    // Stop watering if value "WATER OFF"
   else if (water_value.equals("WATER OFF")) {
      //  water_on(10);
        digitalWrite(motorPin, LOW); //turn on the motor
        digitalWrite(blinkPin, LOW); //turn on the LED
        delay(watertime * 1000); // multiply by 1000 to translate seconds to miliseconds */
     }
 //-----------------------------End of Watering On and Off---------------------------------   

//-------------------Watering by Humidity Limit--------------------------------------------
   if (soil_h_lower_limit != 0 && soil_hum < soil_h_lower_limit) {
      digitalWrite(motorPin, HIGH); //turn on the motor
      digitalWrite(blinkPin, HIGH); //turn on the LED
      delay(watertime * 1000);
   }
   else { // if soil_h_lower_limit is 0 and soil_h_upper_limit is 0, then watering stops
      digitalWrite(motorPin, LOW); //turn off the motor
      digitalWrite(blinkPin, LOW); //turn off the LED
   }   
//----------------End of Watering by Humidity Limit-----------------------------------------
} 

//Functions-----------------//
float ph() {

  float pH;
  // clear the receive buffer
  mod.flushInput();

  // switch RS-485 to transmit mode
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);

  // write out the message
  for (uint8_t i = 0; i < sizeof(soil_ph); i++ ) mod.write( soil_ph[i] );

  // wait for the transmission to complete
  mod.flush();
  
  // switch RS-485 to receive mode
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);

  // crude delay to allow response bytes to be received!
  delay(100);

  // read in the received bytes
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
    
  // concatenate byte 3 with byte 4 to get ph
  pH = (float(values[3] << 8 | values[4]))/100.00;
  /* cannot read soil pH */
  if (pH == -0.01) {
    return 7;
  }
  else {
  //return values[4];
  return pH;
  }  
}
