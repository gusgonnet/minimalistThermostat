/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/Users/me/0trabajo/gus/minimalistThermostat2023/src/minimalistthermostat.ino"
// Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)
// This is a human-readable summary of (and not a substitute for) the license.
// Disclaimer
//
// You are free to:
// Share — copy and redistribute the material in any medium or format
// Adapt — remix, transform, and build upon the material
// The licensor cannot revoke these freedoms as long as you follow the license terms.
//
// Under the following terms:
// Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
// NonCommercial — You may not use the material for commercial purposes.
// ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
// No additional restrictions — You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//
// Notices:
// You do not have to comply with the license for elements of the material in the public domain or where your use is permitted by an applicable exception or limitation.
// No warranties are given. The license may not give you all of the permissions necessary for your intended use. For example, other rights such as publicity, privacy, or moral rights may limit how you use the material.
//
// github: https://github.com/gusgonnet/minimalistThermostat
// hackster: https://www.hackster.io/gusgonnet/the-minimalist-thermostat-bb0410
//
// Free for personal use.
//
// https://creativecommons.org/licenses/by-nc-sa/4.0/

/*******************************************************************************
// ncd.io relay support, comment out if you are not using this board:
// https://store.ncd.io/product/4-channel-general-purpose-spdt-relay-shield-4-gpio-with-iot-interface/
*******************************************************************************/
// #define USE_NCD_RELAYS

/*******************************************************************************
 Here you decide if you want to use Blynk or not by
 commenting this line "#define USE_BLYNK" (or not)
*******************************************************************************/
void setup();
void loop();
int setFan(String newFan);
int setMode(String newMode);
int setTargetTemp(String temp);
int setTargetTempInternal(String temp);
void updateTargetTemp();
String float2string(float floatNumber);
void updateFanStatus();
void updatePulseStatus();
void updateMode();
int readTemperature();
int publishTemperature(float temperature, float humidity);
void initEnterFunction();
void initUpdateFunction();
void initExitFunction();
void idleEnterFunction();
void idleUpdateFunction();
void idleExitFunction();
void heatingEnterFunction();
void heatingUpdateFunction();
void heatingExitFunction();
void pulseEnterFunction();
void pulseUpdateFunction();
void pulseExitFunction();
void coolingEnterFunction();
void coolingUpdateFunction();
void coolingExitFunction();
int setTesting(String test);
int getOutputs(String dummy);
int setCurrentTemp(String newCurrentTemp);
void myDigitalWrite(int input, int status);
String getTime();
void setState(String newState);
void publishEvent(String event);
int convertPinToRelay(int pin);
int relayStatus(String relay);
bool firstCharIsNumber4(String string);
void turnOnRelayForSomeMinutes(int relay, int timeOn);
void turnOffRelayAutomatically();
void setupDisplay();
void updateDisplay();
void flagSettingsHaveChanged();
void readFromEeprom();
void saveSettings();
String convertIntToMode(uint8_t mode);
uint8_t convertModeToInt(String mode);
void resetIfNoWifi();
void myTimerEvent();
#line 37 "/Users/me/0trabajo/gus/minimalistThermostat2023/src/minimalistthermostat.ino"
#define USE_BLYNK

/*******************************************************************************
 This activates an ssd1306 display - 128x64 oled
 Most probably this cannot be used with relays on D0 and D1!
*******************************************************************************/
// #define USE_OLED_DISPLAY

#include "elapsedMillis.h"
#include "PietteTech_DHT.h"
#include "FiniteStateMachine.h"
#include "NCD4Relay.h"

SerialLogHandler logHandler(115200, LOG_LEVEL_INFO);

// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial

#ifdef USE_BLYNK
#include "blynkAuthToken.h"
#include <blynk.h>
#endif

#ifdef USE_OLED_DISPLAY
#include "Adafruit_SSD1306.h"
#endif

#define APP_NAME "Thermostat"
String VERSION = "Version 1.01";
// * BREAKING CHANGE in 0.27!!! DHT moved from D4 to D5

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
 * changes in version 0.20:
           * create a state variable (idle, heating, cooling, off, fan on)
           * trying to fix bug where in init both cool and heat leads remain on until idle state is triggered
           * set the debounce timer to double the default for mode changes
 * changes in version 0.21:
           * store settings in eeprom
           * changing defaults to HVAC OFF (from HEATING)
           * updating the blynk cloud periodically
           * added in the blynk app the state of the thermostat (also published in the particle cloud)
           * here is the link for cloning the blynk app http://tinyurl.com/zq9lcef
           * changed all eeprom values to uint8_t to save space and bytes written
             this saves eeprom pages to be written more often than needed
             (for instance a float takes 4 bytes and an uint8_t takes only 1)
 * changes in version 0.22:
           * fixed an issue with targetTempString, when rebooting the photon would not show the
             temperature loaded from the eeprom
 * changes in version 0.23:
           * Swapped pushbullet notifications with google sheets on thermostat activity
                 source: https://www.hackster.io/gusgonnet/pushing-data-to-google-docs-02f9c4
 * changes in version 0.24:
           * Reverting to Heating/Cooling from Winter/Summer modes
 * changes in version 0.25:
           * updated D0/D1/D2 to D1/D2/D3 since my photon's D0 is not behaving
           * Renaming to Heat/Cool from Heating/Cooling modes
 * changes in version 0.26:
           * Particle build share link: https://go.particle.io/shared_apps/5a30567d31ef4463730008ad
           * add multi thread support for photon: SYSTEM_THREAD(ENABLED);
           * adding support for ncd.io 4 relays board:
              https://store.ncd.io/product/4-channel-general-purpose-spdt-relay-shield-4-gpio-with-iot-interface/
           * reducing DEBOUNCE_SETTINGS to 2000
           * reducing DEBOUNCE_SETTINGS_MODE to 4000
           * changed and simplified code with
              if (USE_BLYNK == "yes") to #define USE_BLYNK
           * adding oled 128*64 support
 * changes in version 0.27:
           * Particle build share link: https://go.particle.io/shared_apps/5af213e3d51e93abd20003b0
           * adding setFan(), setMode() and setTargetTemp() cloud functions
           * BREAKING CHANGE!!! DHT moved from D4 to D5
 * changes in version 0.28:
           * adding watchdog
           * reset if no wifi for 2 minutes
 * changes in version 0.29:
           * decreasing temp threshold from 0.10 to 0.05
 * changes in version 0.30:
           * increasing temp threshold from 0.05 to 0.20, otherwise the heat starts and stops too soon and often
 * changes in version 1.01:
           * update blynk to new blynk cloud


TODO:
  * set max time for heating or cooling in 5 hours (alarm) or 6 hours (auto-shut-off)
  * #define STATE_FAN_ON "Fan On" -> the fan status should show up in the status
  * refloat BLYNK_CONNECTED()?
  * the fan goes off few seconds after the cooling is on

*******************************************************************************/

// use google sheets?
// source: https://www.hackster.io/gusgonnet/pushing-data-to-google-docs-02f9c4
// #define USE_GOOGLE_SHEETS

// use pushbullet for notifications?
// source: https://www.hackster.io/gusgonnet/sharing-the-push-notifications-of-your-hardware-b558ae
#define USE_PUSHBULLET

#ifdef USE_PUSHBULLET
#define PUSHBULLET_NOTIF_HOME "pushbulletHOME"         //-> family group in pushbullet
#define PUSHBULLET_NOTIF_PERSONAL "pushbulletPERSONAL" //-> only my phone
#endif

const int TIME_ZONE = -4;

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State initState = State(initEnterFunction, initUpdateFunction, initExitFunction);
State idleState = State(idleEnterFunction, idleUpdateFunction, idleExitFunction);
State heatingState = State(heatingEnterFunction, heatingUpdateFunction, heatingExitFunction);
State pulseState = State(pulseEnterFunction, pulseUpdateFunction, pulseExitFunction);
State coolingState = State(coolingEnterFunction, coolingUpdateFunction, coolingExitFunction);

// initialize state machine, start in state: Idle
FSM thermostatStateMachine = FSM(initState);

// milliseconds for the init cycle, so temperature samples get stabilized
// this should be in the order of the 5 minutes: 5*60*1000==300000
// for now, I will use 1 minute
#define INIT_TIMEOUT 60000
elapsedMillis initTimer;

// minimum number of milliseconds to leave the heating element on
//  to protect on-off on the fan and the heating/cooling elements
#define MINIMUM_ON_TIMEOUT 60000
elapsedMillis minimumOnTimer;

// minimum number of milliseconds to leave the system in idle state
//  to protect the fan and the heating/cooling elements
#define MINIMUM_IDLE_TIMEOUT 60000
elapsedMillis minimumIdleTimer;

// milliseconds to pulse on the heating = 600 seconds = 10 minutes
//  turns the heating on for a certain time
//  comes in handy when you want to warm up the house a little bit
#define PULSE_TIMEOUT 600000
elapsedMillis pulseTimer;

/*******************************************************************************
 IO mapping
*******************************************************************************/
// D1 : relay1: fan
// D2 : relay2: heat
// D3 : relay3: cool
// D5 : DHT22
// D0, D4, D6, D7 : unused
// A0~A7 : unused
int fan = D1;
int heat = D2;
int cool = D3;
// TESTING_HACK
int fanOutput;
int heatOutput;
int coolOutput;

/*******************************************************************************
 DHT sensor
*******************************************************************************/
#define DHTTYPE DHT22            // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN 5                 // Digital pin for communications
#define DHT_SAMPLE_INTERVAL 5000 // Sample room temperature every 5 seconds \
                                 //  this is then averaged in temperatureAverage
void dht_wrapper();              // must be declared before the lib initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
bool bDHTstarted; // flag to indicate we started acquisition
elapsedMillis dhtSampleInterval;
// how many samples to take and average, more takes longer but measurement is smoother
const int NUMBER_OF_SAMPLES = 10;
// const float DUMMY = -100;
// const float DUMMY_ARRAY[NUMBER_OF_SAMPLES] = { DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY };
#define DUMMY -100
#define DUMMY_ARRAY {DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY};
float temperatureSamples[NUMBER_OF_SAMPLES] = DUMMY_ARRAY;
float averageTemperature;

/*******************************************************************************
 thermostat related declarations
*******************************************************************************/
// temperature related variables - internal
float desiredTemp = 19.0; // coming from blynk
float targetTemp = 19.0;
float currentTemp = 20.0;
float currentHumidity = 0.0;

// you can change this to your liking
//  a smaller value will make your temperature more constant at the price of
//   starting the heat more times
//  a larger value will reduce the number of times the HVAC comes on but will leave it on a longer time
float margin = 0.20;

// sensor difference with real temperature (if none set to zero)
// use this variable to align measurements with your existing thermostat
float temperatureCalibration = -1.35;

// temperature related variables - to be exposed in the cloud
String targetTempString = String(targetTemp);           // String to store the target temp so it can be exposed and set
String currentTempString = String(currentTemp);         // String to store the sensor's temp so it can be exposed
String currentHumidityString = String(currentHumidity); // String to store the sensor's humidity so it can be exposed

#define DEBOUNCE_SETTINGS 2000
#define DEBOUNCE_SETTINGS_MODE 4000 // give more time to the MODE change
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

// here are the possible modes the thermostat can be in: off/heat/cool
#define MODE_OFF "Off"
#define MODE_HEAT "Heat"
#define MODE_COOL "Cool"
String externalMode = MODE_OFF;
String internalMode = MODE_OFF;
bool modeButtonClick = false;
elapsedMillis modeButtonClickTimer;

// here are the possible states of the thermostat
#define STATE_INIT "Initializing"
#define STATE_IDLE "Idle"
#define STATE_HEATING "Heating"
#define STATE_COOLING "Cooling"
#define STATE_FAN_ON "Fan On"
#define STATE_OFF "Off"
#define STATE_PULSE_HEAT "Pulse Heat"
#define STATE_PULSE_COOL "Pulse Cool"
String state = STATE_INIT;

// timers work on millis, so we adjust the value with this constant
#define MILLISECONDS_TO_MINUTES 60000
#define MILLISECONDS_TO_SECONDS 1000

// TESTING_HACK
//  this allows me to system test the project
bool testing = false;

/*******************************************************************************
 Your blynk token goes in another file to avoid sharing it by mistake
  (like I did in one of my commits some time ago)
 The file containing your blynk auth token has to be named blynkAuthToken.h and it should
 contain something like this:
  #define BLYNK_AUTH_TOKEN "1234567890123456789012345678901234567890"
 replace with your project auth token (the blynk app will give you one)
*******************************************************************************/
#ifdef USE_BLYNK
char auth[] = BLYNK_AUTH_TOKEN;

BlynkTimer timer;

#define BLYNK_DISPLAY_CURRENT_TEMP V5
#define BLYNK_DISPLAY_HUMIDITY V6
#define BLYNK_DISPLAY_TARGET_TEMP V7

// #define BLYNK_SLIDER_TEMP V10 // no more slider
#define BLYNK_BUTTON_TEMP_LESS V1
#define BLYNK_BUTTON_TEMP_PLUS V2

#define BLYNK_LED_FAN V13
// #define BLYNK_LED_HEAT V4
// #define BLYNK_LED_COOL V5
// #define BLYNK_LED_PULSE V6

#define BLYNK_DISPLAY_MODE V9
#define BLYNK_BUTTON_MODE V3
#define BLYNK_BUTTON_PULSE V4
#define BLYNK_BUTTON_FAN V0
#define BLYNK_DISPLAY_STATE V10

// this is the remote temperature sensor
#define BLYNK_DISPLAY_CURRENT_TEMP_UPSTAIRS V9

// this defines how often the readings are sent to the blynk cloud (millisecs)
#define BLYNK_STORE_INTERVAL 5000
elapsedMillis blynkStoreInterval;

// WidgetLED fanLed(BLYNK_LED_FAN);     //register led to virtual pin 3
// WidgetLED heatLed(BLYNK_LED_HEAT);   //register led to virtual pin 4
// WidgetLED coolLed(BLYNK_LED_COOL);   //register led to virtual pin 5
// WidgetLED pulseLed(BLYNK_LED_PULSE); //register led to virtual pin 6
#endif

// enable the user code (our program below) to run in parallel with cloud connectivity code
//  source: https://docs.particle.io/reference/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

#ifdef USE_NCD_RELAYS
NCD4Relay relayController;
int triggerRelay(String command);

// timers for switching off the relays - the only one supported here is relay 4
// suince all others are used by the thermostat
elapsedMillis timerOnRelay4;
int turnOffRelay4AfterTime = 0;
#endif

#ifdef USE_OLED_DISPLAY
Adafruit_SSD1306 display(D4);

// this defines how often the display is updated
#define OLED_UPDATE_INTERVAL 5000
elapsedMillis oledUpdateInterval;
int oledStep = 0;

#endif

/*******************************************************************************
 structure for writing thresholds in eeprom
 https://docs.particle.io/reference/firmware/photon/#eeprom
*******************************************************************************/
// randomly chosen value here. The only thing that matters is that it's not 255
//  since 255 is the default value for uninitialized eeprom
//  I used 137 and 138 in version 0.21 already
#define EEPROM_VERSION 139
#define EEPROM_ADDRESS 0

struct EepromMemoryStructure
{
  uint8_t version = EEPROM_VERSION;
  uint8_t targetTemp;
  uint8_t internalFan;
  uint8_t internalMode;
};
EepromMemoryStructure eepromMemory;

bool settingsHaveChanged = false;
elapsedMillis settingsHaveChanged_timer;
#define SAVE_SETTINGS_INTERVAL 10000

// reset the system after 120 seconds if the application is unresponsive
// https://docs.particle.io/reference/device-os/firmware/photon/#application-watchdog
ApplicationWatchdog wd(120000, System.reset);

#define RESET_IF_NO_WIFI 120000
elapsedMillis resetIfNoWifiInterval;

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup()
{

  // publish startup message with firmware version
  Particle.publish(APP_NAME, VERSION, 60, PRIVATE);

#ifdef USE_NCD_RELAYS
  Serial.begin(115200);
  relayController.setAddress(0, 0, 0);
  triggerRelay("turnoffallrelays");

  Particle.function("controlRelay", triggerRelay);
  Particle.function("relayStatus", relayStatus);

#else
  // declare and init pins
  pinMode(fan, OUTPUT);
  pinMode(heat, OUTPUT);
  pinMode(cool, OUTPUT);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
#endif

  // declare cloud variables
  // https://docs.particle.io/reference/firmware/photon/#particle-variable-
  // Up to 20 cloud variables may be registered and each variable name is limited
  // to a maximum of 12 characters.
  Particle.variable("targetTemp", targetTempString);
  Particle.variable("currentTemp", currentTempString);
  Particle.variable("humidity", currentHumidityString);
  Particle.variable("mode", externalMode);
  Particle.variable("state", state);

  // declare cloud functions
  // https://docs.particle.io/reference/firmware/photon/#particle-function-
  // Up to 15 cloud functions may be registered and each function name is limited
  // to a maximum of 12 characters.
  Particle.function("setTargetTmp", setTargetTemp);
  Particle.function("setMode", setMode);
  Particle.function("setFan", setFan);
  // Particle.function("setPulse", setPulse);

  // TESTING_HACK
  Particle.function("getOutputs", getOutputs);
  // Particle.function("setCurrTmp", setCurrentTemp);
  // Particle.function("setTesting", setTesting);

#ifdef USE_BLYNK
  Blynk.begin(auth);
  timer.setInterval(1000L, myTimerEvent);
#endif

  Time.zone(TIME_ZONE);

  // reset samples array to default so we fill it up with new samples
  uint8_t i;
  for (i = 0; i < NUMBER_OF_SAMPLES; i++)
  {
    temperatureSamples[i] = DUMMY;
  }

#ifdef USE_OLED_DISPLAY
  setupDisplay();
#endif

  // restore settings from eeprom, if there were any saved before
  readFromEeprom();
}

// This wrapper is in charge of calling the DHT sensor lib
void dht_wrapper() { DHT.isrCallback(); }

/*******************************************************************************
 * Function Name  : loop
 * Description    : this function runs continuously while the project is running
 *******************************************************************************/
void loop()
{

  // this function reads the temperature of the DHT sensor
  readTemperature();

#ifdef USE_BLYNK
  // all the Blynk magic happens here
  Blynk.run();
#endif

  updateTargetTemp();
  updateFanStatus();
  updatePulseStatus();
  updateMode();

#ifdef USE_OLED_DISPLAY
  updateDisplay();
#endif

  // this function updates the FSM
  //  the FSM is the heart of the thermostat - all actions are defined by its states
  thermostatStateMachine.update();

  // every now and then we save the settings
  saveSettings();

#if PLATFORM_ID == PLATFORM_PHOTON_PRODUCTION
  resetIfNoWifi();
#endif
}

/*******************************************************************************/
/*******************************************************************************/
/*******************          CLOUD FUNCTIONS         *************************/
/*******************************************************************************/
/*******************************************************************************/

/*******************************************************************************
 * Function Name  : setFan
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setFan(String newFan)
{
  if (newFan.equalsIgnoreCase("on"))
  {
    internalFan = true;
    flagSettingsHaveChanged();
    return 0;
  }

  if (newFan.equalsIgnoreCase("off"))
  {
    internalFan = false;
    flagSettingsHaveChanged();
    return 0;
  }

  // else parameter was invalid
  return -1;
}

/*******************************************************************************
 * Function Name  : setMode
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setMode(String newMode)
{
  // mode: cycle through off->heating->cooling
  //  do this only when blynk sends a 1
  //  background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  //  source: http://docs.blynk.cc/#widgets-controllers-button
  if ((newMode != MODE_OFF) && (newMode != MODE_HEAT) && (newMode != MODE_COOL))
  {
    return -1;
  }

  externalMode = newMode;
  flagSettingsHaveChanged();
  return 0;
}

/*******************************************************************************
 * Function Name  : setTargetTemp
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setTargetTemp(String temp)
{
  float tmpFloat = temp.toFloat();
  // update the target temp only in the case the conversion to float works
  //  (toFloat returns 0 if there is a problem in the conversion)
  //  sorry, if you wanted to set 0 as the target temp, you can't :)
  if ((tmpFloat > 0) && (tmpFloat > 14.9) && (tmpFloat < 31))
  {
    // newTargetTemp will be copied to targetTemp moments after in function updateTargetTemp()
    //  this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    targetTemp = tmpFloat;
    targetTempString = float2string(targetTemp);
    flagSettingsHaveChanged();
    return 0;
  }

  // show only 2 decimals in notifications
  //  Example: show 19.00 instead of 19.000000
  temp = temp.substring(0, temp.length() - 4);

  // if the execution reaches here then the value was invalid
  // Particle.publish(APP_NAME, "ERROR: Failed to set new target temp to " + temp, 60, PRIVATE);
  //   Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "ERROR: Failed to set new target temp to " + temp + getTime(), 60, PRIVATE);
  String tempStatus = "ERROR: Failed to set new target temp to " + temp + getTime();
  publishEvent(tempStatus);
  return -1;
}

/*******************************************************************************
 * Function Name  : setTargetTempInternal
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
* Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setTargetTempInternal(String temp)
{
  float tmpFloat = temp.toFloat();
  // update the target temp only in the case the conversion to float works
  //  (toFloat returns 0 if there is a problem in the conversion)
  //  sorry, if you wanted to set 0 as the target temp, you can't :)
  if ((tmpFloat > 0) && (tmpFloat > 14.9) && (tmpFloat < 31))
  {
    // newTargetTemp will be copied to targetTemp moments after in function updateTargetTemp()
    //  this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    newTargetTemp = tmpFloat;
    // start timer to debounce this new setting
    setNewTargetTempTimer = 0;
    return 0;
  }

  // show only 2 decimals in notifications
  //  Example: show 19.00 instead of 19.000000
  temp = temp.substring(0, temp.length() - 4);

  // if the execution reaches here then the value was invalid
  // Particle.publish(APP_NAME, "ERROR: Failed to set new target temp to " + temp, 60, PRIVATE);
  //   Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "ERROR: Failed to set new target temp to " + temp + getTime(), 60, PRIVATE);
  String tempStatus = "ERROR: Failed to set new target temp to " + temp + getTime();
  publishEvent(tempStatus);
  return -1;
}

/*******************************************************************************
 * Function Name  : updateTargetTemp
 * Description    : updates the value of target temperature of the thermostat
                    moments after it was set with setTargetTempInternal
 * Return         : none
 *******************************************************************************/
void updateTargetTemp()
{
  // debounce the new setting
  if (setNewTargetTempTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }
  // is there anything to update?
  if (targetTemp == newTargetTemp)
  {
    return;
  }

  targetTemp = newTargetTemp;
  targetTempString = float2string(targetTemp);

  // Particle.publish(APP_NAME, "New target temp: " + targetTempString, 60, PRIVATE);
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "New target temp: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
  String tempStatus = "New target temp: " + targetTempString + "°C" + getTime();
  publishEvent(tempStatus);
}

/*******************************************************************************
 * Function Name  : float2string
 * Description    : return the string representation of the float number
                     passed as parameter with 2 decimals
 * Return         : the string
 *******************************************************************************/
String float2string(float floatNumber)
{
  String stringNumber = String(floatNumber);

  // return only 2 decimals
  //  Example: show 19.00 instead of 19.000000
  stringNumber = stringNumber.substring(0, stringNumber.length() - 4);

  return stringNumber;
}

/*******************************************************************************
 * Function Name  : updateFanStatus
 * Description    : updates the status of the fan moments after it was set
 * Return         : none
 *******************************************************************************/
void updateFanStatus()
{
  // if the button was not pressed, get out
  if (not fanButtonClick)
  {
    return;
  }

  // debounce the new setting
  if (fanButtonClickTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }

  // reset flag of button pressed
  fanButtonClick = false;

  // is there anything to update?
  //  this code here takes care of the users having cycled the mode to the same original value
  if (internalFan == externalFan)
  {
    return;
  }

  // update the new setting from the external to the internal variable
  internalFan = externalFan;

  if (internalFan)
  {
    // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Fan on" + getTime(), 60, PRIVATE);
    String tempStatus = "Fan on" + getTime();
    publishEvent(tempStatus);
  }
  else
  {
    // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Fan off" + getTime(), 60, PRIVATE);
    String tempStatus = "Fan off" + getTime();
    publishEvent(tempStatus);
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
  // if the button was not pressed, get out
  if (not pulseButtonClick)
  {
    return;
  }

  // debounce the new setting
  if (pulseButtonClickTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }

  // reset flag of button pressed
  pulseButtonClick = false;

  // is there anything to update?
  //  this code here takes care of the users having cycled the mode to the same original value
  if (internalPulse == externalPulse)
  {
    return;
  }

  // update only in the case the FSM state is idleState (the thermostat is doing nothing)
  //  or pulseState (a pulse is already running and the user wants to abort it)
  if (not(thermostatStateMachine.isInState(idleState) or thermostatStateMachine.isInState(pulseState)))
  {
    Particle.publish(PUSHBULLET_NOTIF_HOME, "ERROR: You can only start a pulse in idle state" + getTime(), 60, PRIVATE);
#ifdef USE_BLYNK
    // pulseLed.off();
#endif
    return;
  }

  // update the new setting from the external to the internal variable
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
  // if the mode button was not pressed, get out
  if (not modeButtonClick)
  {
    return;
  }

  // debounce the new setting
  if (modeButtonClickTimer < DEBOUNCE_SETTINGS_MODE)
  {
    return;
  }

  // reset flag of button pressed
  modeButtonClick = false;

  // is there anything to update?
  //  this code here takes care of the users having cycled the mode to the same original value
  if (internalMode == externalMode)
  {
    return;
  }

  // update the new mode from the external to the internal variable
  internalMode = externalMode;
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Mode set to " + internalMode + getTime(), 60, PRIVATE);
  String tempStatus = "Mode set to " + internalMode + getTime();
  publishEvent(tempStatus);
}

/*******************************************************************************
 * Function Name  : readTemperature
 * Description    : reads the temperature of the DHT22 sensor at every DHT_SAMPLE_INTERVAL
                    if testing the app, it returns right away
 * Return         : 0
 *******************************************************************************/
int readTemperature()
{

  // TESTING_HACK
  // are we testing the app? then no need to acquire from the sensor
  if (testing)
  {
    return 0;
  }

  // time is up? no, then come back later
  if (dhtSampleInterval < DHT_SAMPLE_INTERVAL)
  {
    return 0;
  }

  // time is up, reset timer
  dhtSampleInterval = 0;

  // start the sample
  if (!bDHTstarted)
  {
    DHT.acquireAndWait(5);
    bDHTstarted = true;
  }

  // still acquiring sample? go away and come back later
  if (DHT.acquiring())
  {
    return 0;
  }

  // I observed my dht22 measuring below 0 from time to time, so let's discard that sample
  if (DHT.getCelsius() < 0)
  {
    // reset the sample flag so we can take another
    bDHTstarted = false;
    return 0;
  }

  // valid sample acquired, adjust DHT difference if any
  float tmpTemperature = (float)DHT.getCelsius();
  tmpTemperature = tmpTemperature + temperatureCalibration;

  //------------------------------------------------------------------
  // let's make an average of the measured temperature
  // by taking N samples
  uint8_t i;
  for (i = 0; i < NUMBER_OF_SAMPLES; i++)
  {
    // store the sample in the next available 'slot' in the array of samples
    if (temperatureSamples[i] == DUMMY)
    {
      temperatureSamples[i] = tmpTemperature;
      break;
    }
  }

  // is the samples array full? if not, exit and get a new sample
  if (temperatureSamples[NUMBER_OF_SAMPLES - 1] == DUMMY)
  {
    return 0;
  }

  // average all the samples out
  averageTemperature = 0;
  for (i = 0; i < NUMBER_OF_SAMPLES; i++)
  {
    averageTemperature += temperatureSamples[i];
  }
  averageTemperature /= NUMBER_OF_SAMPLES;

  // reset samples array to default so we fill it up again with new samples
  for (i = 0; i < NUMBER_OF_SAMPLES; i++)
  {
    temperatureSamples[i] = DUMMY;
  }
  //------------------------------------------------------------------

  // sample acquired and averaged - go ahead and store temperature and humidity in internal variables
  publishTemperature(averageTemperature, (float)DHT.getHumidity());

  // reset the sample flag so we can take another
  bDHTstarted = false;

  return 0;
}

/*******************************************************************************
 * Function Name  : publishTemperature
 * Description    : the temperature/humidity passed as parameters get stored in internal variables
                    and then published
 * Return         : 0
 *******************************************************************************/
int publishTemperature(float temperature, float humidity)
{

  char currentTempChar[32];
  currentTemp = temperature;
  int currentTempDecimals = (currentTemp - (int)currentTemp) * 100;
  sprintf(currentTempChar, "%0d.%d", (int)currentTemp, currentTempDecimals);

  char currentHumidityChar[32];
  currentHumidity = humidity;
  int currentHumidityDecimals = (currentHumidity - (int)currentHumidity) * 100;
  sprintf(currentHumidityChar, "%0d.%d", (int)currentHumidity, currentHumidityDecimals);

  // publish readings into exposed variables
  currentTempString = String(currentTempChar);
  currentHumidityString = String(currentHumidityChar);

  // publish readings
  Particle.publish(APP_NAME, currentTempString + "°C " + currentHumidityString + "%", 60, PRIVATE);

  return 0;
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/
void initEnterFunction()
{
  // start the timer of this cycle
  initTimer = 0;
  // set the state
  setState(STATE_INIT);
}
void initUpdateFunction()
{
  // time is up?
  if (initTimer > INIT_TIMEOUT)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void initExitFunction()
{
  Particle.publish(APP_NAME, "Initialization done", 60, PRIVATE);
}

void idleEnterFunction()
{
  // set the state
  setState(STATE_IDLE);

  // turn off the fan only if fan was not set on manually by the user
  if (internalFan == false)
  {
    myDigitalWrite(fan, LOW);
  }
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

  // start the minimum timer of this cycle
  minimumIdleTimer = 0;
}
void idleUpdateFunction()
{
  // set the fan output to the internalFan ONLY in this state of the FSM
  //  since other states might need the fan on
  // set it off only if it was on and internalFan changed to false
  if (internalFan == false and fanOutput == HIGH)
  {
    myDigitalWrite(fan, LOW);
  }
  // set it on only if it was off and internalFan changed to true
  if (internalFan == true and fanOutput == LOW)
  {
    myDigitalWrite(fan, HIGH);
  }

  // is minimum time up? not yet, so get out of here
  if (minimumIdleTimer < MINIMUM_IDLE_TIMEOUT)
  {
    return;
  }

  // if the thermostat is OFF, there is not much to do
  if (internalMode == MODE_OFF)
  {
    if (internalPulse)
    {
      Particle.publish(PUSHBULLET_NOTIF_HOME, "ERROR: You cannot start a pulse when the system is OFF" + getTime(), 60, PRIVATE);
      internalPulse = false;
    }
    return;
  }

  // are we heating?
  if (internalMode == MODE_HEAT)
  {
    // if the temperature is lower than the target, transition to heatingState
    if (currentTemp <= (targetTemp - margin))
    {
      thermostatStateMachine.transitionTo(heatingState);
    }
    if (internalPulse)
    {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }

  // are we cooling?
  if (internalMode == MODE_COOL)
  {
    // if the temperature is higher than the target, transition to coolingState
    if (currentTemp > (targetTemp + margin))
    {
      thermostatStateMachine.transitionTo(coolingState);
    }
    if (internalPulse)
    {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }
}
void idleExitFunction()
{
}

void heatingEnterFunction()
{
  // set the state
  setState(STATE_HEATING);

  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Heat on" + getTime(), 60, PRIVATE);
  String tempStatus = "Heat on" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, HIGH);
  myDigitalWrite(cool, LOW);

  // start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void heatingUpdateFunction()
{
  // is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    // not yet, so get out of here
    return;
  }

  if (currentTemp >= (targetTemp + margin))
  {
    // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Desired temperature reached: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
    String tempStatus = "Desired temperature reached: " + targetTempString + "°C" + getTime();
    publishEvent(tempStatus);
    thermostatStateMachine.transitionTo(idleState);
  }

  // was the mode changed by the user? if so, go back to idleState
  if (internalMode != MODE_HEAT)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void heatingExitFunction()
{
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Heat off" + getTime(), 60, PRIVATE);
  String tempStatus = "Heat off" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
}

/*******************************************************************************
 * FSM state Name : pulseState
 * Description    : turns the HVAC on for a certain time
                    comes in handy when you want to warm up/cool down the house a little bit
 *******************************************************************************/
void pulseEnterFunction()
{
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Pulse on" + getTime(), 60, PRIVATE);
  String tempStatus = "Pulse on" + getTime();
  publishEvent(tempStatus);
  if (internalMode == MODE_HEAT)
  {
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, HIGH);
    myDigitalWrite(cool, LOW);
    // set the state
    setState(STATE_PULSE_HEAT);
  }
  else if (internalMode == MODE_COOL)
  {
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, LOW);
    myDigitalWrite(cool, HIGH);
    // set the state
    setState(STATE_PULSE_COOL);
  }
  // start the timer of this cycle
  pulseTimer = 0;

  // start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void pulseUpdateFunction()
{
  // is minimum time up? if not, get out of here
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    return;
  }

  // if the pulse was canceled by the user, transition to idleState
  if (not internalPulse)
  {
    thermostatStateMachine.transitionTo(idleState);
  }

  // is the time up for the pulse? if not, get out of here
  if (pulseTimer < PULSE_TIMEOUT)
  {
    return;
  }

  thermostatStateMachine.transitionTo(idleState);
}
void pulseExitFunction()
{
  internalPulse = false;
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Pulse off" + getTime(), 60, PRIVATE);
  String tempStatus = "Pulse off" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

#ifdef USE_BLYNK
  // pulseLed.off();
#endif
}

/*******************************************************************************
 * FSM state Name : coolingState
 * Description    : turns the cooling element on until the desired temperature is reached
 *******************************************************************************/
void coolingEnterFunction()
{
  // set the state
  setState(STATE_COOLING);

  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Cool on" + getTime(), 60, PRIVATE);
  String tempStatus = "Cool on" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, HIGH);

  // start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void coolingUpdateFunction()
{
  // is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    // not yet, so get out of here
    return;
  }

  if (currentTemp <= (targetTemp - margin))
  {
    // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Desired temperature reached: " + targetTempString + "°C" + getTime(), 60, PRIVATE);
    String tempStatus = "Desired temperature reached: " + targetTempString + "°C" + getTime();
    publishEvent(tempStatus);
    thermostatStateMachine.transitionTo(idleState);
  }

  // was the mode changed by the user? if so, go back to idleState
  if (internalMode != MODE_COOL)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void coolingExitFunction()
{
  // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "Cool off" + getTime(), 60, PRIVATE);
  String tempStatus = "Cool off" + getTime();
  publishEvent(tempStatus);
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
  if (test.equalsIgnoreCase("on"))
  {
    testing = true;
  }
  else
  {
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
  return coolOutput * 4 + heatOutput * 2 + fanOutput * 1;
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

  // update the current temp only in the case the conversion to float works
  //  (toFloat returns 0 if there is a problem in the conversion)
  //  sorry, if you wanted to set 0 as the current temp, you can't :)
  if (tmpFloat > 0)
  {
    currentTemp = tmpFloat;
    currentTempString = String(currentTemp);

    // show only 2 decimals in notifications
    //  Example: show 19.00 instead of 19.000000
    currentTempString = currentTempString.substring(0, currentTempString.length() - 4);

    // Particle.publish(APP_NAME, "New current temp: " + currentTempString, 60, PRIVATE);
    // Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "New current temp: " + currentTempString + getTime(), 60, PRIVATE);
    String tempStatus = "New current temp: " + currentTempString + getTime();
    publishEvent(tempStatus);
    return 0;
  }
  else
  {
    // Particle.publish(APP_NAME, "ERROR: Failed to set new current temp to " + newCurrentTemp, 60, PRIVATE);
    Particle.publish(PUSHBULLET_NOTIF_PERSONAL, "ERROR: Failed to set new current temp to " + newCurrentTemp + getTime(), 60, PRIVATE);
    return -1;
  }
}

/*******************************************************************************
 * Function Name  : myDigitalWrite
 * Description    : writes to the pin or the relayController and sets a variable to keep track
                    this is a hack that allows me to system test the project
                    and know what is the status of the outputs
 * Return         : void
 *******************************************************************************/
void myDigitalWrite(int input, int status)
{

#ifdef USE_NCD_RELAYS
  if (status == LOW)
  {
    // relayController.turnOffRelay(convertPinToRelay(input));
    relayController.turnOffRelay(input);
  }
  else
  {
    relayController.turnOnRelay(input);
  }
#else
  digitalWrite(input, status);
#endif

  if (input == fan)
  {
    fanOutput = status;

    Particle.publish("DEBUG fan", String(status), 60, PRIVATE);

#ifdef USE_BLYNK
    // BLYNK_setFanLed(status);
#endif
  }

  if (input == heat)
  {
    heatOutput = status;
#ifdef USE_BLYNK
    // BLYNK_setHeatLed(status);
#endif
  }

  if (input == cool)
  {
    coolOutput = status;
#ifdef USE_BLYNK
    // BLYNK_setCoolLed(status);
#endif
  }
}

/*******************************************************************************
 * Function Name  : getTime
 * Description    : returns the time in the following format: 14:42:31
 TIME_FORMAT_ISO8601_FULL example: 2016-03-23T14:42:31-04:00
 * Return         : the time
 *******************************************************************************/
String getTime()
{
  String timeNow = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
  timeNow = timeNow.substring(11, timeNow.length() - 6);
  return " " + timeNow;
}

/*******************************************************************************
 * Function Name  : setState
 * Description    : sets the state of the system
 * Return         : none
 *******************************************************************************/
void setState(String newState)
{
  state = newState;
#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
#endif
}

/*******************************************************************************
 * Function Name  : publishEvent
 * Description    : publishes an event, to the particle console or google sheets
 * Return         : none
 *******************************************************************************/
void publishEvent(String event)
{

#ifdef USE_GOOGLE_SHEETS
  Particle.publish("googleDocs", "{\"my-name\":\"" + event + "\"}", 60, PRIVATE);
#else
  Particle.publish("event", event, 60, PRIVATE);
#endif
}

/*******************************************************************************
 * Function Name  : convertPinToRelay
 * Description    : send in a pin, get a relay number
 * Return         : the relay number
 *******************************************************************************/
int convertPinToRelay(int pin)
{
  Particle.publish("DEBUG convertPinToRelay", String(pin), 60, PRIVATE);

  switch (pin)
  {
  case 1: // fan:
    return 1;
    break;
  case 2: // heat:
    return 2;
    break;
  case 3: // cool:
    return 3;
    break;
  }
  return 0;
}

/*******************************************************************************/
/*******************************************************************************/
/*******************        NCD4 RELAYS FUNCTIONS      *************************/
/*******************************************************************************/
/*******************************************************************************/
#ifdef USE_NCD_RELAYS

/*******************************************************************************
 * Function Name  : relayStatus
 * Description    : this function returns 1 if the relay is on, 0 if the relay is off
                    and -1 if there is an error
 *******************************************************************************/
int relayStatus(String relay)
{
  int relayNumber = relay.substring(0, 1).toInt();
  if ((relayNumber >= 1) && (relayNumber <= 4))
  {
    return relayController.readRelayStatus(relayNumber);
  }

  return -1;
}

/*******************************************************************************
 * Function Name  : triggerRelay
 * Description    : commands accepted:
                        - turnonallrelays
                        - turnoffallrelays
                        - setBankStatus
                        - "3on", "1off" to set on/off one relay at a time
                        - "34on", "12off", "13on", "13off" to set on/off more than one relay at a time
                        - "34on5", "1on3", "13on40" to turn on relay(s) for a number of minutes
                          example: "34on5" turns relays 3 and 4 on for 5 minutes
 * Returns        : returns 1 if the command was accepted, 0 if not
 *******************************************************************************/
int triggerRelay(String command)
{
  if (command.equalsIgnoreCase("turnoffallrelays"))
  {
    relayController.turnOffAllRelays();
    return 1;
  }

  // Relay Specific Command
  // this supports "4on", "4off" for example
  // this supports "4on5", "4on40" to turn on relay 4 for a number of minutes
  String tempCommand = command.toLowerCase();
  // this var will store the number of minutes for a relay to be on
  int timeOn = 0;
  int relayNumber1 = 0;

  // parse the first relay number
  if (firstCharIsNumber4(tempCommand))
  {
    relayNumber1 = tempCommand.substring(0, 1).toInt();
    Serial.print("relayNumber1: ");
    Serial.println(relayNumber1);
    // then remove the first
    tempCommand = tempCommand.substring(1);
  }

  // check if next chars are equal to on, if so, check if there was a specific ON time sent
  //  it would be after the on
  //  example: 4on50 for turning relay 4 on for 50 minutes
  //  when the program reaches this point, 4 have already been parsed
  //  so here we would end up with on50
  if (tempCommand.startsWith("on"))
  {
    // parse the digits after the on command
    timeOn = tempCommand.substring(2).toInt();
    Serial.print("timeOn: ");
    Serial.println(timeOn);
    // set the command to be on, so the digits that came after are removed
    tempCommand = "on";
  }

  Serial.print("tempCommand:");
  Serial.print(tempCommand);
  Serial.println(".");

  int returnValue = 0;
  int relayNumber;
  int i;

  // analize this particular relay
  relayNumber = relayNumber1;

  if (relayNumber = 4)
  {

    Particle.publish("command/relay/minutes", tempCommand + "/" + String(relayNumber) + "/" + String(timeOn), 60, PRIVATE);

    if (tempCommand.equalsIgnoreCase("on"))
    {
      Serial.println("Turning on relay");
      relayController.turnOnRelay(relayNumber);

      // if timeOn is not zero, then turn the relay off after timeOn minutes
      if (timeOn != 0)
      {
        turnOnRelayForSomeMinutes(relayNumber, timeOn);
      }

      Serial.println("returning");
      returnValue = 1;
    }
    if (tempCommand.equalsIgnoreCase("off"))
    {
      relayController.turnOffRelay(relayNumber);
      returnValue = 1;
    }
    if (tempCommand.equalsIgnoreCase("toggle"))
    {
      relayController.toggleRelay(relayNumber);
      returnValue = 1;
    }
    if (tempCommand.equalsIgnoreCase("momentary"))
    {
      relayController.turnOnRelay(relayNumber);
      delay(300);
      relayController.turnOffRelay(relayNumber);
      returnValue = 1;
    }
  }

  return returnValue;
}

/*******************************************************************************
 * Function Name  : firstCharIsNumber4
 * Description    : returns true if the parameter starts with a number 4
 *******************************************************************************/
bool firstCharIsNumber4(String string)
{
  if (string.startsWith("4"))
  {
    return true;
  }
  // not a 4 digit
  return false;
}

/*******************************************************************************
 * Function Name  : turnOnRelayForSomeMinutes
 * Description    : turn a relay on for a user defined time in minutes
 *******************************************************************************/
void turnOnRelayForSomeMinutes(int relay, int timeOn)
{

  switch (relay)
  {
  case 4:
    relayController.turnOnRelay(relay);
    timerOnRelay4 = 0;
    turnOffRelay4AfterTime = timeOn * MILLISECONDS_TO_MINUTES;
    break;
  }
}

/*******************************************************************************
 * Function Name  : turnOffRelayAutomatically
 * Description    : this function checks if there is a relay to turn off automatically
 *******************************************************************************/
void turnOffRelayAutomatically()
{

  // is time up for relay4?
  if ((turnOffRelay4AfterTime != 0) && (timerOnRelay4 > turnOffRelay4AfterTime))
  {
    turnOffRelay4AfterTime = 0;
    relayController.turnOffRelay(4);
  }
}

#endif

/*******************************************************************************/
/*******************************************************************************/
/*******************           OLED FUNCTIONS          *************************/
/*******************************************************************************/
/*******************************************************************************/
#ifdef USE_OLED_DISPLAY

/*******************************************************************************
 * Function Name  : setupDisplay
 * Description    : init the display
 * Return         : none
 *******************************************************************************/
void setupDisplay()
{

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3c (for the 128x64)

  display.setTextSize(4);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.clearDisplay();
  display.display();
}

/*******************************************************************************
 * Function Name  : updateDisplay
 * Description    : update what is shown in the OLED display
 * Return         : none
 *******************************************************************************/
void updateDisplay()
{

  // is it time to store in the blynk cloud? if so, do it
  if (oledUpdateInterval > OLED_UPDATE_INTERVAL)
  {

    // reset timer
    oledUpdateInterval = 0;

    display.clearDisplay();
    display.setTextSize(4);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);

    switch (oledStep)
    {
    // show target temp
    case 0:
      display.setTextSize(1);
      display.println("Target temp");
      display.setTextSize(3);
      display.print(targetTempString);
      break;
    // show current temp
    case 1:
      display.setTextSize(1);
      display.println("Current temp");
      display.setTextSize(3);
      display.print(currentTempString);
      break;
      // show humidity
    case 2:
      display.setTextSize(1);
      display.println("Humidity");
      display.setTextSize(3);
      display.print(currentHumidityString);
      break;
    }

    display.display();

    oledStep = oledStep + 1;
    if (oledStep == 3)
    {
      oledStep = 0;
    }
  }
}

#endif

/*******************************************************************************/
/*******************************************************************************/
/*******************          EEPROM FUNCTIONS         *************************/
/********  https://docs.particle.io/reference/firmware/photon/#eeprom  *********/
/*******************************************************************************/
/*******************************************************************************/

/*******************************************************************************
 * Function Name  : flagSettingsHaveChanged
 * Description    : this function gets called when the user of the blynk app
                    changes a setting. The blynk app calls the blynk cloud and in turn
                    it calls the functions BLYNK_WRITE()
 * Return         : none
 *******************************************************************************/
void flagSettingsHaveChanged()
{
  settingsHaveChanged = true;
  settingsHaveChanged_timer = 0;
}

/*******************************************************************************
 * Function Name  : readFromEeprom
 * Description    : retrieves the settings from the EEPROM memory
 * Return         : none
 *******************************************************************************/
void readFromEeprom()
{

  EepromMemoryStructure myObj;
  EEPROM.get(EEPROM_ADDRESS, myObj);

  // verify this eeprom was written before
  //  if version is 255 it means the eeprom was never written in the first place, hence the
  //  data just read with the previous EEPROM.get() is invalid and we will ignore it
  if (myObj.version == EEPROM_VERSION)
  {

    targetTemp = float(myObj.targetTemp);
    newTargetTemp = targetTemp;
    targetTempString = float2string(targetTemp);

    internalMode = convertIntToMode(myObj.internalMode);
    externalMode = internalMode;

    // these variables are false at boot
    if (myObj.internalFan == 1)
    {
      internalFan = true;
      externalFan = true;
    }

    // Particle.publish(APP_NAME, "DEBUG: read settings from EEPROM: " + String(myObj.targetTemp)
    Particle.publish(APP_NAME, "read:" + internalMode + "-" + String(internalFan) + "-" + String(targetTemp), 60, PRIVATE);
  }
}

/*******************************************************************************
 * Function Name  : saveSettings
 * Description    : in this function we wait a bit to give the user time
                    to adjust the right value for them and in this way we try not
                    to save in EEPROM at every little change.
                    Remember that each eeprom writing cycle is a precious and finite resource
 * Return         : none
 *******************************************************************************/
void saveSettings()
{

  // if the thermostat is initializing, get out of here
  if (thermostatStateMachine.isInState(initState))
  {
    return;
  }

  // if no settings were changed, get out of here
  if (not settingsHaveChanged)
  {
    return;
  }

  // if settings have changed, is it time to store them?
  if (settingsHaveChanged_timer < SAVE_SETTINGS_INTERVAL)
  {
    return;
  }

  // reset timer
  settingsHaveChanged_timer = 0;
  settingsHaveChanged = false;

  // store thresholds in the struct type that will be saved in the eeprom
  eepromMemory.version = EEPROM_VERSION;
  eepromMemory.targetTemp = uint8_t(targetTemp);
  eepromMemory.internalMode = convertModeToInt(internalMode);

  eepromMemory.internalFan = 0;
  if (internalFan)
  {
    eepromMemory.internalFan = 1;
  }

  // then save
  EEPROM.put(EEPROM_ADDRESS, eepromMemory);

  // Particle.publish(APP_NAME, "stored:" + eepromMemory.internalMode + "-" + String(eepromMemory.internalFan) + "-" + String(eepromMemory.targetTemp) , 60, PRIVATE);
  Particle.publish(APP_NAME, "stored:" + internalMode + "-" + String(internalFan) + "-" + String(targetTemp), 60, PRIVATE);
}

/*******************************************************************************
 * Function Name  : convertIntToMode
 * Description    : converts the int mode (saved in the eeprom) into the String mode
 * Return         : String
 *******************************************************************************/
String convertIntToMode(uint8_t mode)
{
  if (mode == 1)
  {
    return MODE_HEAT;
  }
  if (mode == 2)
  {
    return MODE_COOL;
  }

  // in all other cases
  return MODE_OFF;
}

/*******************************************************************************
 * Function Name  : convertModeToInt
 * Description    : converts the String mode into the int mode (to be saved in the eeprom)
 * Return         : String
 *******************************************************************************/
uint8_t convertModeToInt(String mode)
{
  if (mode == MODE_HEAT)
  {
    return 1;
  }
  if (mode == MODE_COOL)
  {
    return 2;
  }

  // in all other cases
  return 0;
}

/*******************************************************************************
 * Function Name  : resetIfNoWifi
 * Description    : this function resets the device if there is no wifi for more than 2 minutes
 *******************************************************************************/
#if PLATFORM_ID == PLATFORM_PHOTON_PRODUCTION
void resetIfNoWifi()
{

  // never reset if it is connected to wifi
  if (WiFi.ready())
  {
    Serial.println("wifi detected");
    resetIfNoWifiInterval = 0;
    return;
  }

  Serial.println("wifi not detected");

  // is time up? no, then come back later
  if (resetIfNoWifiInterval < RESET_IF_NO_WIFI)
  {
    return;
  }

  // it comes here if it was not connected to wifi for RESET_IF_NO_WIFI
  Serial.println("Resetting device");
  delay(1000);
  System.reset();
}
#endif

void myTimerEvent()
{
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP, currentTemp);
  Blynk.virtualWrite(BLYNK_DISPLAY_HUMIDITY, currentHumidity);
  Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
  Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, desiredTemp);
  Blynk.virtualWrite(BLYNK_LED_FAN, externalFan);
  Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
}

// desired temp --
BLYNK_WRITE(BLYNK_BUTTON_TEMP_LESS) // this command is listening when something is written to V1
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  if (pinValue == 1)
  {
    desiredTemp = desiredTemp - 0.2;
    if ((desiredTemp > 14.9) && (desiredTemp < 31))
    {
      newTargetTemp = desiredTemp;
      // start timer to debounce this new setting
      setNewTargetTempTimer = 0;
      flagSettingsHaveChanged();
    }
    Log.info("decrease temp");
    Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, desiredTemp);
  }
}
// desired temp ++
BLYNK_WRITE(BLYNK_BUTTON_TEMP_PLUS)
{
  int pinValue = param.asInt();
  if (pinValue == 1)
  {
    desiredTemp = desiredTemp + 0.2;
    if ((desiredTemp > 14.9) && (desiredTemp < 31))
    {
      newTargetTemp = desiredTemp;
      // start timer to debounce this new setting
      setNewTargetTempTimer = 0;
      flagSettingsHaveChanged();
    }
    Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, desiredTemp);
    Log.info("increase temp");
  }
}

BLYNK_WRITE(BLYNK_BUTTON_PULSE)
{
  // flip pulse status, if it's on switch it off and viceversa
  //  do this only when blynk sends a 1
  if (param.asInt() == 1)
  {
    externalPulse = not externalPulse;
    // start timer to debounce this new setting
    pulseButtonClickTimer = 0;
    // flag that the button was clicked
    pulseButtonClick = true;
  }
}

BLYNK_WRITE(BLYNK_BUTTON_FAN)
{
  if (param.asInt() == 1)
  {
    Log.info("fan button clicked");
    externalFan = not externalFan;
    // start timer to debounce this new setting
    fanButtonClickTimer = 0;
    // flag that the button was clicked
    fanButtonClick = true;
    // update the led
    Blynk.virtualWrite(BLYNK_LED_FAN, externalFan);
    flagSettingsHaveChanged();
  }
}

BLYNK_WRITE(BLYNK_BUTTON_MODE)
{
  // mode: cycle through off->heating->cooling
  //  do this only when blynk sends a 1
  //  background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  //  source: http://docs.blynk.cc/#widgets-controllers-button
  if (param.asInt() == 1)
  {
    if (externalMode == MODE_OFF)
    {
      externalMode = MODE_HEAT;
    }
    else if (externalMode == MODE_HEAT)
    {
      externalMode = MODE_COOL;
    }
    else if (externalMode == MODE_COOL)
    {
      externalMode = MODE_OFF;
    }
    else
    {
      externalMode = MODE_OFF;
    }

    // start timer to debounce this new setting
    modeButtonClickTimer = 0;
    // flag that the button was clicked
    modeButtonClick = true;
    // update the mode indicator
    Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);

    flagSettingsHaveChanged();
  }
}
