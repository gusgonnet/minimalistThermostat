#include "application.h"
//#include "finite-state-machine.h"
#include "elapsedMillis.h"
#include "PietteTech_DHT.h"
#include "FiniteStateMachine.h"

#define APP_NAME "Thermostat"
// IO mapping
// D0 : relay: fan
// D1 : relay: heat
// D2 : relay: cold
// D3 : relay:
// D4 : DHT22
// D5 : unused
// D6 : unused
// D7 : unused
// A0~A7 : unused

//initialize states with proper enter, update and exit functions
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
State idleState = State( idleEnterFunction, idleUpdateFunction, idleExitFunction );
State heatingState = State( heatingEnterFunction, heatingUpdateFunction, heatingExitFunction );
State endOfCycleState = State( endOfCycleEnterFunction, endOfCycleUpdateFunction, endOfCycleExitFunction );

//initialize state machine, start in state: Idle
FSM thermostatStateMachine = FSM(initState);

String VERSION = "Version 0.07";
int fan = D0;
int heat = D1;
int cold = D2;
int fanTesting;
int heatTesting;
int coldTesting;

// system defines
#define DHTTYPE  DHT22                // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   4                    // Digital pin for communications
#define DHT_SAMPLE_INTERVAL   30000   // Sample room temperature every 30 seconds
void dht_wrapper(); // must be declared before the lib initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
bool bDHTstarted;       // flag to indicate we started acquisition
elapsedMillis dhtSampleInterval;

//milliseconds for the init cycle, so temperature samples get stabilized
//this should be in the order of the 5 minutes: 5*60*1000==300000
#define INIT_TIMEOUT 30000
elapsedMillis initTimer;

//milliseconds to leave the fan on when the target temp has been reached
//this evacuates the heat or the cold air from vents
#define END_OF_CYCLE_TIMEOUT 15000
elapsedMillis endOfCycleTimer;

//temperature related variables - internal
float targetTemp = 19.0;
float currentTemp = 20.0;
float currentHumidity = 0.0;
float margin = 0.5;
//temperature related variables - to be exposed in the cloud
String targetTempString = String(targetTemp); //String to store the target temp so it can be exposed and set
String currentTempString = String(currentTemp); //String to store the sensor's temp so it can be exposed
String currentHumidityString = String(currentHumidity); //String to store the sensor's humidity so it can be exposed

//testing variables
bool testing = false;

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
 //The length of the funcKey is limited to a max of 12 characters.
 // If you declare a function name longer than 12 characters the function will not be registered.
 //user functions
 if (Particle.function("setTargetTmp", setTargetTemp)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register function setTargetTemp", 60, PRIVATE);
 }
 //testing functions
 if (Particle.function("setTesting", setTesting)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register function setTesting", 60, PRIVATE);
 }
 if (Particle.function("setCurrTmp", setCurrentTemp)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register function setCurrentTemp", 60, PRIVATE);
 }
 if (Particle.function("getOutputs", getOutputs)==false) {
    Particle.publish(APP_NAME, "ERROR: Failed to register function getOutputs", 60, PRIVATE);
 }

}

// This wrapper is in charge of calling the DHT sensor lib
void dht_wrapper() { DHT.isrCallback(); }


void loop() {
  readTemperature();
  thermostatStateMachine.update();
}

/*******************************************************************************
 * Function Name  : readTemperature
 * Description    : reads the temperature of the DHT22 sensor at every DHT_SAMPLE_INTERVAL
                    if testing the app, it returns right away
 * Return         : 0
 *******************************************************************************/
int readTemperature() {

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
    DHT.acquire();
    bDHTstarted = true;
  }

  //still acquiring sample? go away
  if (DHT.acquiring()) {
    return 0;
  }

  //sample acquired - go ahead and store temperature and humidity in internal variables
  publishTemperature( (float)DHT.getCelsius(), (float)DHT.getHumidity() );

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
 Particle.publish(APP_NAME, "Current temperature: " + currentTempString, 60, PRIVATE);
 Particle.publish(APP_NAME, "Current humidity: " + currentHumidityString, 60, PRIVATE);

 return 0;

}

//utility functions
void initEnterFunction(){
 Particle.publish(APP_NAME, "initEnterFunction", 60, PRIVATE);
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
 Particle.publish(APP_NAME, "initExitFunction", 60, PRIVATE);
}

void idleEnterFunction(){
 Particle.publish(APP_NAME, "idleEnterFunction", 60, PRIVATE);
 myDigitalWrite(fan, LOW);
 myDigitalWrite(heat, LOW);
 myDigitalWrite(cold, LOW);
}
void idleUpdateFunction(){
 if ( currentTemp <= (targetTemp - margin) ) {
  Particle.publish(APP_NAME, "Starting to heat", 60, PRIVATE);
  thermostatStateMachine.transitionTo(heatingState);
 }
}
void idleExitFunction(){
 Particle.publish(APP_NAME, "idleExitFunction", 60, PRIVATE);
}

void heatingEnterFunction(){
 Particle.publish(APP_NAME, "heatingEnterFunction", 60, PRIVATE);
 myDigitalWrite(fan, HIGH);
 myDigitalWrite(heat, HIGH);
 myDigitalWrite(cold, LOW);
}
void heatingUpdateFunction(){
 if ( currentTemp >= (targetTemp + margin) ) {
  Particle.publish(APP_NAME, "Desired temperature reached", 60, PRIVATE);
  thermostatStateMachine.transitionTo(endOfCycleState);
 }
}
void heatingExitFunction(){
 Particle.publish(APP_NAME, "heatingExitFunction", 60, PRIVATE);
 myDigitalWrite(fan, HIGH);
 myDigitalWrite(heat, LOW);
 myDigitalWrite(cold, LOW);
}

void endOfCycleEnterFunction(){
 Particle.publish(APP_NAME, "endOfCycleEnterFunction", 60, PRIVATE);
 myDigitalWrite(fan, HIGH);
 myDigitalWrite(heat, LOW);
 myDigitalWrite(cold, LOW);

 //start the timer of this cycle
 endOfCycleTimer = 0;

}
void endOfCycleUpdateFunction(){
 //time is up?
 if (endOfCycleTimer > END_OF_CYCLE_TIMEOUT) {
  thermostatStateMachine.transitionTo(idleState);
 }
}
void endOfCycleExitFunction(){
 Particle.publish(APP_NAME, "endOfCycleExitFunction", 60, PRIVATE);
}

/*******************************************************************************
 * Function Name  : setTesting
 * Description    : sets the testing variable to true
 * Return         : 0
 *******************************************************************************/
int setTesting(String dummy)
{
  testing = true;
  return 0;
}

/*******************************************************************************
 * Function Name  : getOutputs
 * Description    : returns the outputs
 * Return         : returns the outputs
 *******************************************************************************/
int getOutputs(String dummy)
{
  // int fan = D0;
  // int heat = D1;
  // int cold = D2;
  return coldTesting*4 + heatTesting*2 + fanTesting*1;
}

/*******************************************************************************
 * Function Name  : setCurrentTemp
 * Description    : sets the current temperature of the thermostat
                    newCurrentTemp has to be a valid float value, or no new current temp will be set
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
    Particle.publish(APP_NAME, "New current temp: " + currentTempString, 60, PRIVATE);
    return 0;
  } else {
    Particle.publish(APP_NAME, "ERROR: Failed to set new current temp to " + newCurrentTemp, 60, PRIVATE);
    return -1;
  }
}

/*******************************************************************************
 * Function Name  : setTargetTemp
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0, or -1 if it fails to convert the temp to float
 *******************************************************************************/
int setTargetTemp(String newTargetTemp)
{
  float tmpFloat = newTargetTemp.toFloat();
  //update the target temp only in the case the conversion to float works
  // (toFloat returns 0 if there is a problem in the conversion)
  // sorry, if you wanted to set 0 as the target temp, you can't :)
  if ( tmpFloat > 0 ) {
    targetTemp = tmpFloat;
    targetTempString = String(targetTemp);
    Particle.publish(APP_NAME, "New target temp: " + targetTempString, 60, PRIVATE);
    return 0;
  } else {
    Particle.publish(APP_NAME, "ERROR: Failed to set new target temp to " + newTargetTemp, 60, PRIVATE);
    return -1;
  }
}

void myDigitalWrite( int input, int status){
  digitalWrite(input, status);
  if (input == fan){
    fanTesting = status;
  }
  if (input == heat){
    heatTesting = status;
  }
  if (input == cold){
    coldTesting = status;
  }
}
