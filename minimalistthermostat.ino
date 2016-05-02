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
String VERSION = "Version 0.19";
/*******************************************************************************
 * changes in version 0.09:
       * reorganized code to group functions
       * added minimum time to protect on-off on the fan and the heating element
          in function heatingUpdateFunction()
 * changes in version 0.10:
       * added temperatureCalibration to fix DHT measurements with existing thermostat
       * reduced END_OF_CYCLE_TIMEOUT to one sec since my HVAC controller
          takes care of running the fan for a minute to evacuate the heat/cool air
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
           * adding Titan test scripts
              more info here: https://www.hackster.io/gusgonnet/how-to-test-your-projects-with-titan-a633f2
 * changes in version 0.17:
           * PUSHBULLET_NOTIF renamed to PUSHBULLET_NOTIF_PERSONAL
           * removed yyyy-mm-dd from notifications and left only hh:mm:ss
           * minor changes in temp/humidity reported with Particle.publish()
           * reporting targetTemp when desired temp is reached
 * changes in version 0.18:
           * created a pulse of heating for warming up the house a bit
           * created a function that sets the fan on and contains this code below in function myDigitalWrite()
              if (USE_BLYNK == "yes") {
           * updated the blynk app
           * created a mode variable (heating, cooling, off)
 * changes in version 0.19:
           * created blynk defined variables (for instance: BLYNK_LED_FAN)
           * updates in the fan control, making UI more responsive to user changes
           * add cooling support
           * pulses now are able to cool the house

TODO:
  * add multi thread support for photon: SYSTEM_THREAD(ENABLED);
              source for discussion: https://community.particle.io/t/the-minimalist-thermostat/19436
              source for docs: https://docs.particle.io/reference/firmware/photon/#system-thread
  * store settings in eeprom
  * create a state variable (idle, heating, cooling, off, fan on)

*******************************************************************************/

#define PUSHBULLET_NOTIF_HOME "pushbulletHOME"         //-> family group in pushbullet
#define PUSHBULLET_NOTIF_PERSONAL "pushbulletPERSONAL" //-> only my phone
const int TIME_ZONE = -4;

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
State idleState = State( idleEnterFunction, idleUpdateFunction, idleExitFunction );
State heatingState = State( heatingEnterFunction, heatingUpdateFunction, heatingExitFunction );
State pulseState = State( pulseEnterFunction, pulseUpdateFunction, pulseExitFunction );
State coolingState = State( coolingEnterFunction, coolingUpdateFunction, coolingExitFunction );

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

//milliseconds to pulse on the heating = 600 seconds = 10 minutes
// turns the heating on for a certain time
// comes in handy when you want to warm up the house a little bit
#define PULSE_TIMEOUT 600000
elapsedMillis pulseTimer;

/*******************************************************************************
 IO mapping
*******************************************************************************/
// D0 : relay: fan
// D1 : relay: heat
// D2 : relay: cool
// D4 : DHT22
// D3, D5~D7 : unused
// A0~A7 : unused
int fan = D0;
int heat = D1;
int cool = D2;
//TESTING_HACK
int fanOutput;
int heatOutput;
int coolOutput;

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

#define DEBOUNCE_SETTINGS 3000
float newTargetTemp = 19.0;
elapsedMillis setNewTargetTempTimer;

bool externalFan = false;
bool internalFan = false;
bool fanButtonClick = false;
elapsedMillis fanButtonClickTimer;

bool externalPulse = false;
bool internalPulse = false;
bool pulseButtonClick = false;
elapsedMillis pulseButtonClickTimer;

//here are the possible modes the thermostat can be in: off/heat/cool
#define MODE_OFF "Off"
#define MODE_HEAT "Heating"
#define MODE_COOL "Cooling"
String externalMode = MODE_HEAT;
String internalMode = MODE_HEAT;
bool modeButtonClick = false;
elapsedMillis modeButtonClickTimer;

//TESTING_HACK
// this allows me to system test the project
bool testing = false;

/*******************************************************************************
 Here you decide if you want to use Blynk or not
 Your blynk token goes in another file to avoid sharing it by mistake
  (like I did in one of my commits some time ago)
 The file containing your blynk auth token has to be named blynkAuthToken.h and it should contain
 something like this:
  #define BLYNK_AUTH_TOKEN "1234567890123456789012345678901234567890"
 replace with your project auth token (the blynk app will give you one)
*******************************************************************************/
#define USE_BLYNK "yes"
char auth[] = BLYNK_AUTH_TOKEN;

//definitions for the blynk interface
#define BLYNK_DISPLAY_CURRENT_TEMP V0
#define BLYNK_DISPLAY_HUMIDITY V1
#define BLYNK_DISPLAY_TARGET_TEMP V2
#define BLYNK_SLIDER_TEMP V10

#define BLYNK_BUTTON_FAN V11
#define BLYNK_LED_FAN V3
#define BLYNK_LED_HEAT V4
#define BLYNK_LED_COOL V5

#define BLYNK_DISPLAY_MODE V7
#define BLYNK_BUTTON_MODE V8
#define BLYNK_LED_PULSE V6
#define BLYNK_BUTTON_PULSE V12


WidgetLED fanLed(BLYNK_LED_FAN); //register led to virtual pin 3
WidgetLED heatLed(BLYNK_LED_HEAT); //register led to virtual pin 4
WidgetLED coolLed(BLYNK_LED_COOL); //register led to virtual pin 5
WidgetLED pulseLed(BLYNK_LED_PULSE); //register led to virtual pin 6

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
  pinMode(cool, OUTPUT);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

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
  if (Particle.variable("mode", externalMode)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register variable mode", 60, PRIVATE);
  }

  //declare cloud functions
  //https://docs.particle.io/reference/firmware/photon/#particle-function-
  //Currently the application supports the creation of up to 4 different cloud functions.
  // If you declare a function name longer than 12 characters the function will not be registered.
  //user functions
  if (Particle.function("setTargetTmp", setTargetTemp)==false) {
     Particle.publish(APP_NAME, "ERROR: Failed to register function setTargetTemp", 60, PRIVATE);
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

  if (USE_BLYNK == "yes") {
    //all the Blynk magic happens here
    Blynk.run();
  }

  updateTargetTemp();
  updateFanStatus();
  updatePulseStatus();
  updateMode();

  //this function updates the FSM
  // the FSM is the heart of the thermostat - all actions are defined by its states
  thermostatStateMachine.update();

}

/*******************************************************************************
 * Function Name  : setTargetTemp
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
* Return         : 0 if all is good, or -1 if the parameter does not match on or off
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
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "ERROR: Failed to set new target temp to " + temp + getTime(), 60, PRIVATE);
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
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "New target temp: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
}

/*******************************************************************************
 * Function Name  : updateFanStatus
 * Description    : updates the status of the fan moments after it was set
 * Return         : none
 *******************************************************************************/
void updateFanStatus()
{
  //if the button was not pressed, get out
  if ( not fanButtonClick ){
    return;
  }

  //debounce the new setting
  if (fanButtonClickTimer < DEBOUNCE_SETTINGS) {
    return;
  }

  //reset flag of button pressed
  fanButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if ( internalFan == externalFan ) {
    return;
  }

  //update the new setting from the external to the internal variable
  internalFan = externalFan;

  if ( internalFan ) {
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Fan on" + getTime(), 60, PRIVATE);
  } else {
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Fan off" + getTime(), 60, PRIVATE);
  }
}

/*******************************************************************************
 * Function Name  : updatePulseStatus
 * Description    : updates the status of the pulse of the thermostat
                    moments after it was set
 * Return         : none
 *******************************************************************************/
void updatePulseStatus()
{
  //if the button was not pressed, get out
  if ( not pulseButtonClick ){
    return;
  }

  //debounce the new setting
  if (pulseButtonClickTimer < DEBOUNCE_SETTINGS) {
    return;
  }

  //reset flag of button pressed
  pulseButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if ( internalPulse == externalPulse ) {
    return;
  }

  //update only in the case the FSM state is idleState (the thermostat is doing nothing)
  // or pulseState (a pulse is already running and the user wants to abort it)
  if ( not ( thermostatStateMachine.isInState(idleState) or thermostatStateMachine.isInState(pulseState) ) ) {
    Particle.publish(PUSHBULLET_NOTIF_HOME, "ERROR: You can only start a pulse in idle state" + getTime(), 60, PRIVATE);
    pulseLed.off();
    return;
  }

  //update the new setting from the external to the internal variable
  internalPulse = externalPulse;

}

/*******************************************************************************
 * Function Name  : updateMode
 * Description    : check if the mode has changed
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
 * Return         : none
 *******************************************************************************/
void updateMode()
{
  //if the mode button was not pressed, get out
  if ( not modeButtonClick ){
    return;
  }

  //debounce the new setting
  if (modeButtonClickTimer < DEBOUNCE_SETTINGS) {
    return;
  }

  //reset flag of button pressed
  modeButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if ( internalMode == externalMode ) {
    return;
  }

  //update the new mode from the external to the internal variable
  internalMode = externalMode;
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Mode set to " + internalMode + getTime(), 60, PRIVATE);

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
  Particle.publish(APP_NAME, "Home: " + currentTempString + "°C " + currentHumidityString + "%", 60, PRIVATE);

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
  Particle.publish(APP_NAME, "Initialization done", 60, PRIVATE);
}

void idleEnterFunction(){
  //turn off the fan only if fan was not set on manually by the user
  if ( internalFan == false ) {
    myDigitalWrite(fan, LOW);
  }
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

  //start the minimum timer of this cycle
  minimumIdleTimer = 0;
}
void idleUpdateFunction(){
  //set the fan output to the internalFan ONLY in this state of the FSM
  // since other states might need the fan on
  //set it off only if it was on and internalFan changed to false
  if ( internalFan == false and fanOutput == HIGH ) {
    myDigitalWrite(fan, LOW);
  }
  //set it on only if it was off and internalFan changed to true
  if ( internalFan == true and fanOutput == LOW ) {
    myDigitalWrite(fan, HIGH);
  }

  //is minimum time up? not yet, so get out of here
  if (minimumIdleTimer < MINIMUM_IDLE_TIMEOUT) {
    return;
  }

  //if the thermostat is OFF, there is not much to do
  if ( internalMode == MODE_OFF ){
    if ( internalPulse ) {
      Particle.publish(PUSHBULLET_NOTIF_HOME, "ERROR: You cannot start a pulse when the system is OFF" + getTime(), 60, PRIVATE);
      internalPulse = false;
    }
    return;
  }

  //are we heating?
  if ( internalMode == MODE_HEAT ){
    //if the temperature is lower than the target, transition to heatingState
    if ( currentTemp <= (targetTemp - margin) ) {
      thermostatStateMachine.transitionTo(heatingState);
    }
    if ( internalPulse ) {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }

  //are we cooling?
  if ( internalMode == MODE_COOL ){
    //if the temperature is higher than the target, transition to coolingState
    if ( currentTemp > (targetTemp + margin) ) {
      thermostatStateMachine.transitionTo(coolingState);
    }
    if ( internalPulse ) {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }

}
void idleExitFunction(){
}

void heatingEnterFunction(){
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Heat on" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, HIGH);
  myDigitalWrite(cool, LOW);

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
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Desired temperature reached: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
    thermostatStateMachine.transitionTo(idleState);
  }

  //was the mode changed by the user? if so, go back to idleState
  if ( internalMode != MODE_HEAT ){
    thermostatStateMachine.transitionTo(idleState);
  }

}
void heatingExitFunction(){
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Heat off" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
}

/*******************************************************************************
 * FSM state Name : pulseState
 * Description    : turns the HVAC on for a certain time
                    comes in handy when you want to warm up/cool down the house a little bit
 *******************************************************************************/
void pulseEnterFunction(){
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Pulse on" + getTime(), 60, PRIVATE);
  if ( internalMode == MODE_HEAT ){
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, HIGH);
    myDigitalWrite(cool, LOW);
  } else if ( internalMode == MODE_COOL ){
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, LOW);
    myDigitalWrite(cool, HIGH);
  }
  //start the timer of this cycle
  pulseTimer = 0;

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void pulseUpdateFunction(){
  //is minimum time up? if not, get out of here
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT) {
    return;
  }

  //if the pulse was canceled by the user, transition to idleState
  if (not internalPulse) {
    thermostatStateMachine.transitionTo(idleState);
  }

  //is the time up for the pulse? if not, get out of here
  if (pulseTimer < PULSE_TIMEOUT) {
    return;
  }

  thermostatStateMachine.transitionTo(idleState);
}
void pulseExitFunction(){
  internalPulse = false;
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Pulse off" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

  if (USE_BLYNK == "yes") {
    pulseLed.off();
  }

}

/*******************************************************************************
 * FSM state Name : coolingState
 * Description    : turns the cooling element on until the desired temperature is reached
 *******************************************************************************/
void coolingEnterFunction(){
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Cool on" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, HIGH);

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void coolingUpdateFunction(){
  //is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT) {
    //not yet, so get out of here
    return;
  }

  if ( currentTemp <= (targetTemp - margin) ) {
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Desired temperature reached: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
    thermostatStateMachine.transitionTo(idleState);
  }

  //was the mode changed by the user? if so, go back to idleState
  if ( internalMode != MODE_COOL ){
   thermostatStateMachine.transitionTo(idleState);
  }

 }
 void coolingExitFunction(){
  Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Cool off" + getTime(), 60, PRIVATE);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
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
  if ( test.equalsIgnoreCase("on") ) {
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
  // int cool = D2;
  return coolOutput*4 + heatOutput*2 + fanOutput*1;
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
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "New current temp: " + currentTempString + getTime(), 60, PRIVATE);
    return 0;
  } else {
    //Particle.publish(APP_NAME, "ERROR: Failed to set new current temp to " + newCurrentTemp, 60, PRIVATE);
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "ERROR: Failed to set new current temp to " + newCurrentTemp + getTime(), 60, PRIVATE);
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
    BLYNK_setFanLed(status);
  }

  if (input == heat){
    heatOutput = status;
    BLYNK_setHeatLed(status);
  }

  if (input == cool){
    coolOutput = status;
    BLYNK_setCoolLed(status);
  }
}

/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP, currentTemp);
}
BLYNK_READ(BLYNK_DISPLAY_HUMIDITY) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_HUMIDITY, currentHumidity);
}
BLYNK_READ(BLYNK_DISPLAY_TARGET_TEMP) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, targetTemp);
}
BLYNK_READ(BLYNK_LED_FAN) {
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if ( externalFan ) {
    fanLed.on();
  } else {
    fanLed.off();
  }
}
BLYNK_READ(BLYNK_LED_PULSE) {
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if ( externalPulse ) {
    pulseLed.on();
  } else {
    pulseLed.off();
  }
}
BLYNK_READ(BLYNK_DISPLAY_MODE) {
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
}

BLYNK_WRITE(BLYNK_SLIDER_TEMP) {
  //this is the blynk slider
  // source: http://docs.blynk.cc/#widgets-controllers-slider
  setTargetTemp(param.asStr());
}
BLYNK_WRITE(BLYNK_BUTTON_FAN) {
  //flip fan status, if it's on switch it off and viceversa
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if ( param.asInt() == 1 ) {
    externalFan = not externalFan;
    //start timer to debounce this new setting
    fanButtonClickTimer = 0;
    //flag that the button was clicked
    fanButtonClick = true;
    //update the led
    if ( externalFan ) {
      fanLed.on();
    } else {
      fanLed.off();
    }
  }
}
BLYNK_WRITE(BLYNK_BUTTON_PULSE) {
  //flip pulse status, if it's on switch it off and viceversa
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if ( param.asInt() == 1 ) {
    externalPulse = not externalPulse;
    //start timer to debounce this new setting
    pulseButtonClickTimer = 0;
    //flag that the button was clicked
    pulseButtonClick = true;
    //update the pulse led
    if ( externalPulse ) {
      pulseLed.on();
    } else {
      pulseLed.off();
    }
  }
}
BLYNK_WRITE(BLYNK_BUTTON_MODE) {
  //mode: cycle through off->heating->cooling
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if ( param.asInt() == 1 ) {
    if ( externalMode == MODE_OFF ){
      externalMode = MODE_HEAT;
    } else if ( externalMode == MODE_HEAT ){
      externalMode = MODE_COOL;
    } else if ( externalMode == MODE_COOL ){
      externalMode = MODE_OFF;
    }
    //start timer to debounce this new setting
    modeButtonClickTimer = 0;
    //flag that the button was clicked
    modeButtonClick = true;
    //update the mode indicator
    Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
  }
}

void BLYNK_setFanLed(int status) {
  if (USE_BLYNK == "yes") {
    if ( status ) {
      fanLed.on();
    } else {
      fanLed.off();
    }
  }
}

void BLYNK_setHeatLed(int status) {
  if (USE_BLYNK == "yes") {
    if ( status ) {
      heatLed.on();
    } else {
      heatLed.off();
    }
  }
}

void BLYNK_setCoolLed(int status) {
  if (USE_BLYNK == "yes") {
    if ( status ) {
      coolLed.on();
    } else {
      coolLed.off();
    }
  }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(BLYNK_DISPLAY_CURRENT_TEMP);
  Blynk.syncVirtual(BLYNK_DISPLAY_HUMIDITY);
  Blynk.syncVirtual(BLYNK_DISPLAY_TARGET_TEMP);
  Blynk.syncVirtual(BLYNK_LED_FAN);
  Blynk.syncVirtual(BLYNK_LED_HEAT);
  Blynk.syncVirtual(BLYNK_LED_COOL);
  Blynk.syncVirtual(BLYNK_LED_PULSE);
  Blynk.syncVirtual(BLYNK_DISPLAY_MODE);
  BLYNK_setFanLed(fan);
  BLYNK_setHeatLed(heat);
  BLYNK_setCoolLed(cool);
}


//example: Heat on @2016-03-23T14:42:31-04:00
String getTime() {
  String timeNow = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
  timeNow = timeNow.substring(11, timeNow.length()-6);
  return " " + timeNow;
}
