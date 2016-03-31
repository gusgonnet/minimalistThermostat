//The MIT License (MIT)
//Copyright (c) 2016 Gustavo Gonnet
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
// OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// github: https://github.com/gusgonnet/minimalistThermostat
// hackster: https://www.hackster.io/gusgonnet/the-minimalist-thermostat-bb0410

#include "application.h"
#include "elapsedMillis.h"
#include "PietteTech_DHT.h"
#include "FiniteStateMachine.h"
#include "blynk.h"
#include "blynkAuthToken.h"

#define APP_NAME "Thermostat"
String VERSION = "Version 0.16";
/*******************************************************************************
 * changes in version 0.09:
       * reorganized code to group functions
       * added minimum time to protect on-off on the fan and the heating element
          in function heatingUpdateFunction()
 * changes in version 0.10:
       * added temperatureCalibration to fix DHT measurements with existing thermostat
       * reduced END_OF_CYCLE_TIMEOUT to one sec since my HVAC controller
          takes care of running the fan for a minute to evacuate the heat/cold
          from the vents
       * added pushbullet notifications for heating on/off
       * added fan on/off setting via a cloud function
 * changes in version 0.11:
      * added more pushbullet notifications and commented out publish() in other cases
 * changes in version 0.12:
           * added blynk support
           * added minimumIdleTimer, to protect fan and heating/cooling elements
              from glitches
 * changes in version 0.13:
           * removing endOfCycleState since my HVAC does not need it
           * adding time in notifications
 * changes in version 0.14:
           * debouncing target temp and fan status
 * changes in version 0.15:
           * taking few samples and averaging the temperature to improve stability
 * changes in version 0.16:
           * discarding samples below 0 celsius for those times when the reading of
              the temperature sensor goes wrong
           * adding date/time in notifications
           * leave only 2 decimals in temp notifications (19.00 instead of 19.000000)
           * improving blynk project
           * fine tunning the testing mode
           * adding Titan test scripts (more info on a hackster article to be written soon)

TODO:
  * add multi thread support for photon: SYSTEM_THREAD(ENABLED);
              source for discussion: https://community.particle.io/t/the-minimalist-thermostat/19436
              source for docs: https://docs.particle.io/reference/firmware/photon/#system-thread
  * add enable/disable setting to be able to switch off the whole system
  * create a function that sets the fan on and contains this code below in function myDigitalWrite()
     if (USE_BLYNK == "yes") {
*******************************************************************************/

#define PUSHBULLET_NOTIF "pushbulletGUST"
const int TIME_ZONE = -4;

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
State idleState = State( idleEnterFunction, idleUpdateFunction, idleExitFunction );
State heatingState = State( heatingEnterFunction, heatingUpdateFunction, heatingExitFunction );

//initialize state machine, start in state: Idle
FSM thermostatStateMachine = FSM(initState);

//milliseconds for the init cycle, so temperature samples get stabilized
//this should be in the order of the 5 minutes: 5*60*1000==300000
//for now, I will use 1 minute
#define INIT_TIMEOUT 60000
elapsedMillis initTimer;

//minimum number of milliseconds to leave the heating element on
// to protect on-off on the fan and the heating/cooling elements
#define MINIMUM_ON_TIMEOUT 60000
elapsedMillis minimumOnTimer;

//minimum number of milliseconds to leave the system in idle state
// to protect the fan and the heating/cooling elements
#define MINIMUM_IDLE_TIMEOUT 60000
elapsedMillis minimumIdleTimer;

/*******************************************************************************
 IO mapping
*******************************************************************************/
// D0 : relay: fan
// D1 : relay: heat
// D2 : relay: cold
// D4 : DHT22
// D3, D5~D7 : unused
// A0~A7 : unused
int fan = D0;
int heat = D1;
int cold = D2;
//TESTING_HACK
int fanOutput;
int heatOutput;
int coldOutput;

/*******************************************************************************
 DHT sensor
*******************************************************************************/
#define DHTTYPE  DHT22                // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   4                    // Digital pin for communications
#define DHT_SAMPLE_INTERVAL   5000    // Sample room temperature every 5 seconds
                                      //  this is then averaged in temperatureAverage
void dht_wrapper(); // must be declared before the lib initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
bool bDHTstarted;       // flag to indicate we started acquisition
elapsedMillis dhtSampleInterval;
// how many samples to take and average, more takes longer but measurement is smoother
const int NUMBER_OF_SAMPLES = 10;
//const float DUMMY = -100;
//const float DUMMY_ARRAY[NUMBER_OF_SAMPLES] = { DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY };
#define DUMMY -100
#define DUMMY_ARRAY { DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY };
float temperatureSamples[NUMBER_OF_SAMPLES] = DUMMY_ARRAY;
float averageTemperature;

/*******************************************************************************
 thermostat related declarations
*******************************************************************************/
//temperature related variables - internal
float targetTemp = 19.0;
float currentTemp = 20.0;
float currentHumidity = 0.0;

//you can change this to your liking
// a smaller value will make your temperature more constant at the price of
//  starting the heat more times
// a larger value will reduce the number of times the HVAC comes on but will leave it on a longer time
float margin = 0.25;

//DHT difference with real temperature (if none set to zero)
//use this variable to fix DHT measurements with your existing thermostat
float temperatureCalibration = -1.35;

//temperature related variables - to be exposed in the cloud
String targetTempString = String(targetTemp); //String to store the target temp so it can be exposed and set
String currentTempString = String(currentTemp); //String to store the sensor's temp so it can be exposed
String currentHumidityString = String(currentHumidity); //String to store the sensor's humidity so it can be exposed

//fan status: false=off, true=on
bool fanStatus = false;

#define DEBOUNCE_SETTINGS 5000
float newTargetTemp = 19.0;
elapsedMillis setNewTargetTempTimer;
bool newFanStatus = false;
elapsedMillis setNewFanStatusTimer;

//TESTING_HACK
// this allows me to system test the project
bool testing = false;

/*******************************************************************************
 Here you decide if you want to use Blynk or not
 Your blynk token goes in another file to avoid sharing it by mistake
  (like I just did in my last commit)
 The file containing your token has to be named blynkAuthToken.h and it should contain
 something like this:
  #define BLYNK_AUTH_TOKEN "1234567890123456789012345678901234567890"
 replace with your project auth token (the blynk app will give you one)
*******************************************************************************/
#define USE_BLYNK "yes"
char auth[] = BLYNK_AUTH_TOKEN;
WidgetLED fanStatusLed(V3); //register led to virtual pin 3

//enable the user code (our program below) to run in parallel with cloud connectivity code
// source: https://docs.particle.io/reference/firmware/photon/#system-thread
//SYSTEM_THREAD(ENABLED);

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup() {

  //publish startup message with firmware version
  Particle.publish(APP_NAME, VERSION, 60, PRIVATE);

  //declare and init pins
  pinMode(fan, OUTPUT);
  pinMode(heat, OUTPUT);
  pinMode(cold, OUTPUT);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cold, LOW);

  //declare cloud variables
  //https://docs.particle.io/reference/firmware/photon/#particle-variable-
  //Currently, up to 10 cloud variables may be defined and each variable name is limited to a maximum of 12 characters
  if (Particle.variable("targetTemp", targetTempString)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register variable targetTemp", 60, PRIVATE);
  }
  if (Particle.variable("currentTemp", currentTempString)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register variable currentTemp", 60, PRIVATE);
  }
  if (Particle.variable("humidity", currentHumidityString)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register variable humidity", 60, PRIVATE);
  }

  //declare cloud functions
  //https://docs.particle.io/reference/firmware/photon/#particle-function-
  //Currently the application supports the creation of up to 4 different cloud functions.
  // If you declare a function name longer than 12 characters the function will not be registered.
  //user functions
  if (Particle.function("setTargetTmp", setTargetTemp)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function setTargetTemp", 60, PRIVATE);
  }
  if (Particle.function("setFan", setFanStatus)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function setFan", 60, PRIVATE);
  }
  //TESTING_HACK
  if (Particle.function("setCurrTmp", setCurrentTemp)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function setCurrentTemp", 60, PRIVATE);
  }
  //TESTING_HACK
  if (Particle.function("getOutputs", getOutputs)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function getOutputs", 60, PRIVATE);
  }
  //TESTING_HACK
  if (Particle.function("setTesting", setTesting)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function setTesting", 60, PRIVATE);
  }

  if (USE_BLYNK == "yes") {
    //init Blynk
    Blynk.begin(auth);
  }

  Time.zone(TIME_ZONE);

  //reset samples array to default so we fill it up with new samples
  uint8_t i;
  for (i=0; i<NUMBER_OF_SAMPLES; i++) {
    temperatureSamples[i] = DUMMY;
  }

}

// This wrapper is in charge of calling the DHT sensor lib
void dht_wrapper() { DHT.isrCallback(); }


/*******************************************************************************
 * Function Name  : loop
 * Description    : this function runs continuously while the project is running
 *******************************************************************************/
void loop() {

  //this function reads the temperature of the DHT sensor
  readTemperature();

  //this function updates the FSM
  // the FSM is the heart of the thermostat - all actions are defined by its states
  thermostatStateMachine.update();

  if (USE_BLYNK == "yes") {
    //all the Blynk magic happens here
    Blynk.run();
  }

  updateTargetTemp();
  updateFanStatus();

}

/*******************************************************************************
 * Function Name  : setTargetTemp
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
 * Return         : 0, or -1 if it fails to convert the temp to float
 *******************************************************************************/
int setTargetTemp(String temp)
{
  float tmpFloat = temp.toFloat();
  //update the target temp only in the case the conversion to float works
  // (toFloat returns 0 if there is a problem in the conversion)
  // sorry, if you wanted to set 0 as the target temp, you can't :)
  if ( tmpFloat > 0 ) {
    //newTargetTemp will be copied to targetTemp moments after in function updateTargetTemp()
    // this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    newTargetTemp = tmpFloat;
    //start timer to debounce this new setting
    setNewTargetTempTimer = 0;
    return 0;
  }

  //show only 2 decimals in notifications
  // Example: show 19.00 instead of 19.000000
  temp = temp.substring(0, temp.length()-4);

  //if the execution reaches here then the value was invalid
  //Particle.publish(APP_NAME, "ERROR: Failed to set new target temp to " + temp, 60, PRIVATE);
  Particle.publish(PUSHBULLET_NOTIF, "ERROR: Failed to set new target temp to " + temp + getTime(), 60, PRIVATE);
  return -1;
}

/*******************************************************************************
 * Function Name  : updateTargetTemp
 * Description    : updates the value of target temperature of the thermostat
                    moments after it was set with setTargetTemp
 * Return         : none
 *******************************************************************************/
void updateTargetTemp()
{
  //debounce the new setting
  if (setNewTargetTempTimer < DEBOUNCE_SETTINGS) {
    return;
  }
  //is there anything to update?
  if (targetTemp == newTargetTemp) {
    return;
  }

  targetTemp = newTargetTemp;
  targetTempString = String(targetTemp);

  //show only 2 decimals in notifications
  // Example: show 19.00 instead of 19.000000
  targetTempString = targetTempString.substring(0, targetTempString.length()-4);

  //Particle.publish(APP_NAME, "New target temp: " + targetTempString, 60, PRIVATE);
  Particle.publish(PUSHBULLET_NOTIF, "New target temp: " + targetTempString + getTime(), 60, PRIVATE);
}

/*******************************************************************************
 * Function Name  : setFanStatus
 * Description    : sets the status of the Fan on or off
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
 * Return         : 0, or -1 if the parameter does not match on or off
 *******************************************************************************/
int setFanStatus(String status)
{
  //update the fan status only in the case the status is on or off
  if ( status == "on" ) {
    //newFanStatus will be copied to fanStatus moments after in function updateTargetTemp()
    // this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    newFanStatus = true;
    //start timer to debounce this new setting
    setNewFanStatusTimer = 0;
    return 0;
  }
  if ( status == "off" ) {
    //newFanStatus will be copied to fanStatus moments after in function updateTargetTemp()
    // this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    newFanStatus = false;
    //start timer to debounce this new setting
    setNewFanStatusTimer = 0;
    return 0;
  }

  Particle.publish(APP_NAME, "ERROR: Failed to set fan status to " + status, 60, PRIVATE);
  return -1;
}

/*******************************************************************************
 * Function Name  : updateFanStatus
 * Description    : updates the status of the fan of the thermostat
                    moments after it was set with setFan
 * Return         : none
 *******************************************************************************/
void updateFanStatus()
{
  //debounce the new setting
  if (setNewFanStatusTimer < DEBOUNCE_SETTINGS) {
    return;
  }
  //is there anything to update?
  if (fanStatus == newFanStatus) {
    return;
  }

  //update the fan status only in the case the status is on or off
  if (newFanStatus) {
    fanStatus = true;
    Particle.publish(PUSHBULLET_NOTIF, "Fan on" + getTime(), 60, PRIVATE);
//    if (USE_BLYNK == "yes") {
//      fanStatusLed.on();
//    }
    return;
  } else {
    fanStatus = false;
    Particle.publish(PUSHBULLET_NOTIF, "Fan off" + getTime(), 60, PRIVATE);
//    if (USE_BLYNK == "yes") {
//      fanStatusLed.off();
//    }
    return;
  }

}


/*******************************************************************************
 * Function Name  : readTemperature
 * Description    : reads the temperature of the DHT22 sensor at every DHT_SAMPLE_INTERVAL
                    if testing the app, it returns right away
 * Return         : 0
 *******************************************************************************/
int readTemperature() {

  //TESTING_HACK
  //are we testing the app? then no need to acquire from the sensor
  if (testing) {
   return 0;
  }

  //time is up? no, then come back later
  if (dhtSampleInterval < DHT_SAMPLE_INTERVAL) {
   return 0;
  }

  //time is up, reset timer
  dhtSampleInterval = 0;

  // start the sample
  if (!bDHTstarted) {
    DHT.acquireAndWait(5);
    bDHTstarted = true;
  }

  //still acquiring sample? go away and come back later
  if (DHT.acquiring()) {
    return 0;
  }

  //I observed my dht22 measuring below 0 from time to time, so let's discard that sample
  if ( DHT.getCelsius() < 0 ) {
    //reset the sample flag so we can take another
    bDHTstarted = false;
    return 0;
  }

  //valid sample acquired, adjust DHT difference if any
  float tmpTemperature = (float)DHT.getCelsius();
  tmpTemperature = tmpTemperature + temperatureCalibration;

  //------------------------------------------------------------------
  //let's make an average of the measured temperature
  // by taking N samples
  uint8_t i;
  for (i=0; i< NUMBER_OF_SAMPLES; i++) {
    //store the sample in the next available 'slot' in the array of samples
    if ( temperatureSamples[i] == DUMMY ) {
      temperatureSamples[i] = tmpTemperature;
      break;
    }
  }

  //is the samples array full? if not, exit and get a new sample
  if ( temperatureSamples[NUMBER_OF_SAMPLES-1] == DUMMY ) {
    return 0;
  }

  // average all the samples out
  averageTemperature = 0;
  for (i=0; i<NUMBER_OF_SAMPLES; i++) {
    averageTemperature += temperatureSamples[i];
  }
  averageTemperature /= NUMBER_OF_SAMPLES;

  //reset samples array to default so we fill it up again with new samples
  for (i=0; i<NUMBER_OF_SAMPLES; i++) {
    temperatureSamples[i] = DUMMY;
  }
  //------------------------------------------------------------------

  //sample acquired and averaged - go ahead and store temperature and humidity in internal variables
  publishTemperature( averageTemperature, (float)DHT.getHumidity() );

  //reset the sample flag so we can take another
  bDHTstarted = false;

  return 0;
}

/*******************************************************************************
 * Function Name  : publishTemperature
 * Description    : the temperature/humidity passed as parameters get stored in internal variables
                    and then published
 * Return         : 0
 *******************************************************************************/
int publishTemperature( float temperature, float humidity ) {

  char currentTempChar[32];
  currentTemp = temperature;
  int currentTempDecimals = (currentTemp - (int)currentTemp) * 100;
  sprintf(currentTempChar,"%0d.%d", (int)currentTemp, currentTempDecimals);

  char currentHumidityChar[32];
  currentHumidity = humidity;
  int currentHumidityDecimals = (currentHumidity - (int)currentHumidity) * 100;
  sprintf(currentHumidityChar,"%0d.%d", (int)currentHumidity, currentHumidityDecimals);

  //publish readings into exposed variables
  currentTempString = String(currentTempChar);
  currentHumidityString = String(currentHumidityChar);

  //publish readings
  Particle.publish(APP_NAME, "Home temperature: " + currentTempString, 60, PRIVATE);
  Particle.publish(APP_NAME, "Home humidity: " + currentHumidityString, 60, PRIVATE);

  return 0;
}


/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/
void initEnterFunction(){
  //Particle.publish(APP_NAME, "initEnterFunction", 60, PRIVATE);
  //start the timer of this cycle
  initTimer = 0;
}
void initUpdateFunction(){
  //time is up?
  if (initTimer > INIT_TIMEOUT) {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void initExitFunction(){
  //Particle.publish(APP_NAME, "initExitFunction", 60, PRIVATE);
}

void idleEnterFunction(){
  //Particle.publish(APP_NAME, "idleEnterFunction", 60, PRIVATE);
  //turn off the fan only if fan was not set on manually with setFan(on)
  if ( fanStatus == false ) {
    myDigitalWrite(fan, LOW);
  }
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cold, LOW);

  //start the minimum timer of this cycle
  minimumIdleTimer = 0;
}
void idleUpdateFunction(){
  //set the fan output to the fanStatus ONLY in this state of the FSM
  // since other states might need the fan on
  //set it off only if it was on and fanStatus changed to false
  if ( fanStatus == false and fanOutput == HIGH ) {
    myDigitalWrite(fan, LOW);
  }
  //set it on only if it was off and fanStatus changed to true
  if ( fanStatus == true and fanOutput == LOW ) {
    myDigitalWrite(fan, HIGH);
  }

  //is minimum time up?
  if (minimumIdleTimer < MINIMUM_IDLE_TIMEOUT) {
    //not yet, so get out of here
    return;
  }

  if ( currentTemp <= (targetTemp - margin) ) {
    //Particle.publish(APP_NAME, "Starting to heat", 60, PRIVATE);
    thermostatStateMachine.transitionTo(heatingState);
  }
}
void idleExitFunction(){
  //Particle.publish(APP_NAME, "idleExitFunction", 60, PRIVATE);
}

void heatingEnterFunction(){
  //Particle.publish(APP_NAME, "heatingEnterFunction", 60, PRIVATE);
  Particle.publish(PUSHBULLET_NOTIF, "Heat on" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, HIGH);
  myDigitalWrite(cold, LOW);

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void heatingUpdateFunction(){
  //is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT) {
    //not yet, so get out of here
    return;
  }

  if ( currentTemp >= (targetTemp + margin) ) {
    //Particle.publish(APP_NAME, "Desired temperature reached", 60, PRIVATE);
    Particle.publish(PUSHBULLET_NOTIF, "Desired temperature reached" + getTime(), 60, PRIVATE);
    thermostatStateMachine.transitionTo(idleState);
  }
}
void heatingExitFunction(){
  //Particle.publish(APP_NAME, "heatingExitFunction", 60, PRIVATE);
  Particle.publish(PUSHBULLET_NOTIF, "Heat off" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cold, LOW);
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 TESTING HACKS
********************************************************************************
********************************************************************************
*******************************************************************************/

/*******************************************************************************
 * Function Name  : setTesting
 * Description    : allows to start testing mode
                    testing mode enables an override of the temperature read
                     by the temperature sensor.
                    this is a hack that allows system testing the project
 * Return         : 0
 *******************************************************************************/
int setTesting(String test)
{
  if ( test == "on" ) {
    testing = true;
  } else {
    testing = false;
  }
  return 0;
}

/*******************************************************************************
 * Function Name  : getOutputs
 * Description    : returns the outputs so we can test the program
                    this is a hack that allows me to system test the project
 * Return         : returns the outputs
 *******************************************************************************/
int getOutputs(String dummy)
{
  // int fan = D0;
  // int heat = D1;
  // int cold = D2;
  return coldOutput*4 + heatOutput*2 + fanOutput*1;
}

/*******************************************************************************
 * Function Name  : setCurrentTemp
 * Description    : sets the current temperature of the thermostat
                    newCurrentTemp has to be a valid float value, or no new current temp will be set
                    this is a hack that allows me to system test the project
* Return         : 0, or -1 if it fails to convert the temp to float
 *******************************************************************************/
int setCurrentTemp(String newCurrentTemp)
{
  float tmpFloat = newCurrentTemp.toFloat();

  //update the current temp only in the case the conversion to float works
  // (toFloat returns 0 if there is a problem in the conversion)
  // sorry, if you wanted to set 0 as the current temp, you can't :)
  if ( tmpFloat > 0 ) {
    currentTemp = tmpFloat;
    currentTempString = String(currentTemp);

    //show only 2 decimals in notifications
    // Example: show 19.00 instead of 19.000000
    currentTempString = currentTempString.substring(0, currentTempString.length()-4);

    //Particle.publish(APP_NAME, "New current temp: " + currentTempString, 60, PRIVATE);
    Particle.publish(PUSHBULLET_NOTIF, "New current temp: " + currentTempString + getTime(), 60, PRIVATE);
    return 0;
  } else {
    //Particle.publish(APP_NAME, "ERROR: Failed to set new current temp to " + newCurrentTemp, 60, PRIVATE);
    Particle.publish(PUSHBULLET_NOTIF, "ERROR: Failed to set new current temp to " + newCurrentTemp + getTime(), 60, PRIVATE);
    return -1;
  }
}

/*******************************************************************************
 * Function Name  : myDigitalWrite
 * Description    : writes to the pin and sets a variable to keep track
                    this is a hack that allows me to system test the project
                    and know what is the status of the outputs
 * Return         : void
 *******************************************************************************/
void myDigitalWrite(int input, int status){
  digitalWrite(input, status);

  if (input == fan){
    fanOutput = status;
    //TODO: create a function that sets fan on and contains this code below
    if (USE_BLYNK == "yes") {
      if ( status ) {
        fanStatusLed.on();
      } else {
        fanStatusLed.off();
      }
    }
  }

  if (input == heat){
    heatOutput = status;
  }
  if (input == cold){
    coldOutput = status;
  }
}

/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
BLYNK_READ(V0) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(V0, currentTemp);
}
BLYNK_READ(V1) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(V1, currentHumidity);
}
BLYNK_READ(V2) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(V2, targetTemp);
}
BLYNK_READ(V3) {
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if ( fanStatus ) {
    fanStatusLed.on();
  } else {
    fanStatusLed.off();
  }
  //Blynk.virtualWrite(V3, fanStatus);
}

BLYNK_WRITE(V10) {
  //this is the blynk slider
  // source: http://docs.blynk.cc/#widgets-controllers-slider
  setTargetTemp(param.asStr());
}
BLYNK_WRITE(V11) {
  //flip fan status, if it's on switch it off and viceversa
  // do this only when blynk sends a 1
  // background: in a push button as my project is using, blynk sends 0 then 1
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if ( param.asInt() == 1 ) {
    if ( fanStatus ){
      setFanStatus("off");
    } else {
      setFanStatus("on");
    }
  }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
}


//example: Heat on @2016-03-23T14:42:31-04:00
String getTime() {
  String timeNow = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
  timeNow = timeNow.substring(0, timeNow.length()-6);
  // return "\n@" + timeNow;
  return " " + timeNow;
}
