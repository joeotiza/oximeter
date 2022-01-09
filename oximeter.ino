/* @file    oximeter.ino
 * @author  Joseph Oduor
 * @reg no  F17/1845/2017
 * @date    08 June 2021
 * @brief   Extract SPO2 and heart rate data
 *          Relay data to IoT app
 */

#include <DFRobot_MAX30102.h>
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>

/*Define authentication token and wi-fi credentials
 * auth = Auth Token from the Blynk app
 * ssid = Wi-Fi name/SSID
 * pass = Wi-Fi password
 */
char auth[] = "eD64Lmt1VkBY__DdWGA0svSxzvSaFFji";
char ssid[] = "JOJCARE B1 ";
char pass[] = "Odour@123";
 
/* Connections on the board are: 
 * SCL PIN - D1
 * SDA PIN - D2
 * INT PIN - D0
 */

//create object 'oximeter' of class DFRobot_MAX30102
DFRobot_MAX30102 oximeter;

/*
Macro definition options in sensor configuration
sampleAverage: SAMPLEAVG_1 SAMPLEAVG_2 SAMPLEAVG_4
               SAMPLEAVG_8 SAMPLEAVG_16 SAMPLEAVG_32
ledMode:       MODE_REDONLY  MODE_RED_IR  MODE_MULTILED
pulseWidth:    PULSEWIDTH_69 PULSEWIDTH_118 PULSEWIDTH_215 PULSEWIDTH_411
sampleRate:    SAMPLERATE_50 SAMPLERATE_100 SAMPLERATE_200 SAMPLERATE_400
               SAMPLERATE_800 SAMPLERATE_1000 SAMPLERATE_1600 SAMPLERATE_3200
adcRange:      ADCRANGE_2048 ADCRANGE_4096 ADCRANGE_8192 ADCRANGE_16384
*/

void setup()
{
  //Set up the connection to the Blynk app
  Blynk.begin(auth, ssid, pass); 
  //Initialize serial and set the arduino to transmit at 115,200 bits per sec
  Serial.begin(115200);
  
  /*Initialize sensor 
   *pWire I2C bus pointer object and construction device, can both pass or not pass parameters (Wire in default)
   *i2cAddr Chip I2C address (0x57 in default)
   *returns true or false
   */
  while (!oximeter.begin()) 
  {
    Serial.println("MAX30102 was not found");
    delay(1000);
  }

  /*Using Macro definition above to configure sensor
   *ledBrightness=LED brightness, default value: 0x1F（6.4mA), Range: 0~255（0=Off, 255=50mA）
   *sampleAverage=Average multiple samples then draw once, reduce data throughput, default 4 samples average
   *ledMode=LED mode, default to use red light and IR at the same time 
   *sampleRate=Sampling rate, default 400 samples every second 
   *pulseWidth=Pulse width: the longer the pulse width, the wider the detection range. Default to be Max range
   *adcRange=ADC Measurement Range, default 4096 (nA), 15.63(pA) per LSB
   */
  oximeter.sensorConfiguration(/*ledBrightness=*/150, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_3200, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);
}

int32_t SPO2; //SPO2
int8_t SPO2Valid; //Flag to display if SPO2 calculation is valid==1 or invalid==0
int32_t heartRate; //Heart-rate
int8_t heartRateValid; //Flag to display if heart-rate calculation is valid==1 or invalid==0 
int heartRateHold, SPO2Hold; //Variables to hold valid calculations
String tips; //User Interface tips
const byte GROUP_SIZE = 4; //Increase for more averaging. 4 is good.
byte heartRates[GROUP_SIZE]; //Array of heart rates
byte rateSpot = 0; //Cyclic variable to go 0-1-...n-0-1...
byte SPO2s[GROUP_SIZE]; //Array of SPO2 data
byte oxygenSpot = 0; //Cyclic variable
int beatAvg, SPO2Avg; //Averages to be displayed

void loop()
{
  Serial.println(F("Next reading."));
  oximeter.heartrateAndOxygenSaturation(/**SPO2=*/&SPO2, /**SPO2Valid=*/&SPO2Valid, /**heartRate=*/&heartRate, /**heartRateValid=*/&heartRateValid);

  /* Take the rolling average of 
   * four heart rate measurements
   */
  if (heartRate < 130 && heartRate > 40)
  {
    heartRates[rateSpot++] = (byte)heartRate; //Store reading in the array
    rateSpot %= GROUP_SIZE; //Wrap variable

    beatAvg = 0;
    for (byte x = 0 ; x < GROUP_SIZE ; x++)
      beatAvg += heartRates[x];
    beatAvg /= GROUP_SIZE;
  }

  /* Take the rolling average of
   * four SPO2 measurements
   */
  if (SPO2 <= 100 && SPO2 > 86)
  {
    SPO2s[oxygenSpot++] = (byte)SPO2; //Store reading in the array
    oxygenSpot %= GROUP_SIZE; //Wrap variable
    SPO2Avg = 0;

    for (byte y = 0 ; y < GROUP_SIZE ; y++)
      SPO2Avg += SPO2s[y];
    SPO2Avg /= GROUP_SIZE;
  }
  
  //Print result 
  Serial.print(F("heartRate="));
  Serial.print(heartRate, DEC);
  Serial.print(F(", heartRateValid="));
  Serial.print(heartRateValid, DEC);
  Serial.print(F("; SPO2="));
  Serial.print(SPO2, DEC);
  Serial.print(F(", SPO2Valid="));
  Serial.println(SPO2Valid, DEC);
  Serial.print(F("AvgHeartRate="));
  Serial.print(beatAvg, DEC);
  Serial.print(F("; AvgSPO2="));
  Serial.println(SPO2Avg, DEC);

  /* Detect if a finger is absent*/
  if (oximeter.getIR()<5000)
  {
    SPO2Avg = 0;
    beatAvg = 0; //reset to zero since finger is out
    tips="Place your finger on the sensor.\n";
    Serial.println(F("Finger absent."));
  }
  
  /*Print appropriate UI message in tips*/
  if (heartRateValid==1 && SPO2Valid==1) //both SPO2 and heart-rate are valid
  {
    tips="Measuring...\n";
  }
  
  else if (heartRateValid==1 || SPO2Valid==1) //only one value is valid
  {
    tips="Try to stay still and quiet.\n";
  }
  
  else if (oximeter.getIR()>5000)
  {
    tips="Check that your finger is placed well.\n";
  }

  /*Relay values to the blink application
   * UI tips to virtual pin 0 to be displayed on the terminal
   * heart-rate value to virtual pin 1
   * SPO2 value to virtual pin 2
   */
  Blynk.virtualWrite(V0, tips);
  Blynk.virtualWrite(V1, beatAvg);
  Blynk.virtualWrite(V2, SPO2Avg);
}
