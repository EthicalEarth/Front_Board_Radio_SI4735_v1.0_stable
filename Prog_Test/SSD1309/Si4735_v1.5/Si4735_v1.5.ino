/*
  SI4735 all in one with SSB Support
  It is important to know the SSB support works on SI4735-D60 and SI4732-A10 devices. 

  This sketch uses SPI OLED, buttons and  Encoder.

  This sketch uses the Rotary Encoder Class implementation from Ben Buxton (the source code is included
  together with this sketch). 

  ABOUT SSB PATCH:  
  This sketch will download a SSB patch to your SI4735-D60 or SI4732-A10 devices (patch_init.h). It will take about 8KB of the Arduino memory.

  First of all, it is important to say that the SSB patch content is not part of this library. The paches used here were made available by Mr. 
  Vadim Afonkin on his Dropbox repository. It is important to note that the author of this library does not encourage anyone to use the SSB patches 
  content for commercial purposes. In other words, this library only supports SSB patches, the patches themselves are not part of this library.

  In this context, a patch is a piece of software used to change the behavior of the SI4735 device.
  There is little information available about patching the SI4735. The following information is the understanding of the author of
  this project and it is not necessarily correct. A patch is executed internally (run by internal MCU) of the device.
  Usually, patches are used to fixes bugs or add improvements and new features of the firmware installed in the internal ROM of the device.
  Patches to the SI4735 are distributed in binary form and have to be transferred to the internal RAM of the device by
  the host MCU (in this case Arduino). Since the RAM is volatile memory, the patch stored into the device gets lost when you turn off the system.
  Consequently, the content of the patch has to be transferred again to the device each time after turn on the system or reset the device.

  ATTENTION: The author of this project does not guarantee that procedures shown here will work in your development environment.
  Given this, it is at your own risk to continue with the procedures suggested here.
  This library works with the I2C communication protocol and it is designed to apply a SSB extension PATCH to CI SI4735-D60.
  Once again, the author disclaims any liability for any damage this procedure may cause to your SI4735 or other devices that you are using.

  Features of this sketch:

  1) FM, AM (MW and SW) and SSB (LSB and USB);
  2) Audio bandwidth filter 0.5, 1, 1.2, 2.2, 3 and 4kHz;
  3) 22 commercial and ham radio bands pre configured;
  4) BFO Control; and
  5) Frequency step switch (1, 5 and 10kHz);

  Main Parts:
  Encoder with push button;
  Seven bush buttons;
  oled Display with SPI device;



*/


#include "Rotary.h"
#include <U8g2lib.h>
#include <SPI.h>
#include <SI4735.h>
#include <patch_init.h> // SSB patch for whole SSBRX initialization string

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI oled(U8G2_R0, /* cs=*/ 53, /* dc=*/ 46, /* reset=*/ 48); 

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3



#define RESET_PIN 22

// Enconder PINs
#define ENCODER_PIN_A 19
#define ENCODER_PIN_B 3

// Buttons controllers
#define MODE_SWITCH 33      // Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 34 // Used to select the banddwith. Values: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 kHz
#define VOL_UP 35           // Volume Up
#define VOL_DOWN 36         // Volume Down
#define BAND_BUTTON_UP 37   // Next band
#define BAND_BUTTON_DOWN 38 // Previous band
#define AGC_SWITCH 39      // Switch AGC ON/OF
#define STEP_SWITCH 40     // Used to select the increment or decrement frequency step (1, 5 or 10 kHz)
#define BFO_SWITCH 41      // Used to select the enconder control (BFO or VFO)
//#define BFO_SWITCH 42  // A0 (Alternative to the pin 13). Used to select the enconder control (BFO or VFO)

#define MIN_ELAPSED_TIME 100
#define MIN_ELAPSED_RSSI_TIME 150

#define DEFAULT_VOLUME 45 // change it for your favorite sound volume

#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1

const char *bandModeDesc[] = {"FM ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;

bool bfoOn = false;
bool disableAgc = true;
bool ssbLoaded = false;
bool fmStereo = true;

int currentBFO = 0;

long elapsedRSSI = millis();
long elapsedButton = millis();

long rdsElapsed = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint16_t previousFrequency;
uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

uint8_t seekDirection = 1;

uint8_t bwIdxSSB = 2;
const char *bandwidthSSB[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

uint8_t bwIdxAM = 1;
const char *bandwidthAM[] = {"6", "4", "3", "2", "1", "1.8", "2.5"};

int8_t bwIdxFM = 0;
const char *bandwidthFM[] = {"AUT","110","84","60","40"};


char *stationName;
char bufferStatioName[20];


char oldBuffer[15];




/*
   Band data structure
*/
typedef struct
{
  uint8_t bandType;     // Band type (FM, MW or SW)
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Defeult step (increment and decrement)
} Band;

/*
   Band table
*/
Band band[] = {
  {FM_BAND_TYPE, 8400, 10800, 10570, 10},
  {LW_BAND_TYPE, 100, 510, 300, 1},
  {MW_BAND_TYPE, 520, 1720, 810, 10},
  {SW_BAND_TYPE, 1800, 3500, 1900, 1}, // 160 meters
  {SW_BAND_TYPE, 3500, 4500, 3700, 1}, // 80 meters
  {SW_BAND_TYPE, 4500, 5500, 4850, 5},
  {SW_BAND_TYPE, 5600, 6300, 6000, 5},
  {SW_BAND_TYPE, 6800, 7800, 7200, 5}, // 40 meters
  {SW_BAND_TYPE, 9200, 10000, 9600, 5},
  {SW_BAND_TYPE, 10000, 11000, 10100, 1}, // 30 meters
  {SW_BAND_TYPE, 11200, 12500, 11940, 5},
  {SW_BAND_TYPE, 13400, 13900, 13600, 5},
  {SW_BAND_TYPE, 14000, 14500, 14200, 1}, // 20 meters
  {SW_BAND_TYPE, 15000, 15900, 15300, 5},
  {SW_BAND_TYPE, 17200, 17900, 17600, 5},
  {SW_BAND_TYPE, 18000, 18300, 18100, 1},  // 17 meters
  {SW_BAND_TYPE, 21000, 21900, 21200, 1},  // 15 mters
  {SW_BAND_TYPE, 24890, 26200, 24940, 1},  // 12 meters
  {SW_BAND_TYPE, 26200, 27900, 27500, 1},  // CB band (11 meters)
  {SW_BAND_TYPE, 28000, 30000, 28400, 1}
}; // 10 meters

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;

uint8_t rssi = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735 si4735;

void u8g2_prepare(void) {
oled.setFont(u8g2_font_5x7_mf);
oled.setFontRefHeightExtendedText();
oled.setDrawColor(1);
oled.setFontPosTop();
oled.setFontDirection(0);
}
void setup()
{
  Serial.begin(9600); // Activate Console
  u8g2_prepare();
  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  pinMode(BANDWIDTH_BUTTON, INPUT_PULLUP);
  pinMode(BAND_BUTTON_UP, INPUT_PULLUP);
  pinMode(BAND_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(VOL_UP, INPUT_PULLUP);
  pinMode(VOL_DOWN, INPUT_PULLUP);
  pinMode(BFO_SWITCH, INPUT_PULLUP);
  pinMode(AGC_SWITCH, INPUT_PULLUP);
  pinMode(STEP_SWITCH, INPUT_PULLUP);
  pinMode(MODE_SWITCH, INPUT_PULLUP);
  oled.begin();
  oled.clear();
  //oled.on();
  //oled.setFont(FONT6X8);
  
  // Splash - Change it for your introduction text.
  oled.setFont(u8g2_font_6x10_tf); 
  oled.drawStr( 10, 0, "Welcome to Si4735");
  oled.drawStr( 45, 8, "radio!");
  oled.drawStr( 30, 45, "Designed by ");
  oled.drawStr( 25, 54, "Andrey Ivanov.");
  oled.drawStr( 25, 30, "Start in");
  oled.drawStr( 86, 30, "s.");
  for (int timer=5; timer>0; timer--){  /* Update timer variable and appears value.*/
  oled.setCursor(80,30);
  oled.print (timer);
  delay(1000);
  oled.sendBuffer();
 }; 
  // end Splash
  oled.sendBuffer();
  delay(1000);
  oled.clear();
  oled.setFont(u8g2_font_5x7_mf);
  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);


  si4735.getDeviceI2CAddress(RESET_PIN); // Looks for the I2C bus address and set it.  Returns 0 if error

  // Uncomment the lines below if you experience some unstable behaviour.
  // si4735.setMaxDelayPowerUp(500);      // Time to the external crystal become stable (default is 10ms).
  // si4735.setMaxDelaySetFrequency(100); // Time needed to process the next frequency setup (default is 30 ms)

  si4735.setup(RESET_PIN, FM_BAND_TYPE);

  delay(300);  
  // Set up the radio for the current band (see index table variable bandIdx )
  useBand();
  currentFrequency = previousFrequency = si4735.getFrequency();
  si4735.setVolume(volume);
  oled.clear();
  showStatus();
}

// Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
  {
    if (encoderStatus == DIR_CW)
    {
      encoderCount = 1;
    }
    else
    {
      encoderCount = -1;
    }
  }
}



// Show current frequency

void showFrequency()
{
  String freqDisplay;
  String unit;
  String bandMode;
  int divider = 1;
  int decimals = 3;
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    divider = 100;
    decimals = 1;
    unit = "MHz";
  }
  else if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE)
  {
    divider = 1;
    decimals = 0;
    unit = "kHz";
  }
  else
  {
    divider = 1000;
    decimals = 3;
    unit = "kHz";
  }

  if ( !bfoOn )
    freqDisplay = String((float)currentFrequency / divider, decimals);
  else
    freqDisplay = "[" + String((float)currentFrequency / divider, decimals) + "]";
   oled.setFont(u8g2_font_7x13B_mf);
   oled.setCursor(30, 25);
   oled.print("                 ");
   oled.setCursor(30, 25);
   oled.print((String)freqDisplay+unit);
   //oled.print(freqDisplay);
   oled.sendBuffer();
   oled.setFont(u8g2_font_5x7_mf);

                                                  

  if (currentFrequency < 520 )
    bandMode = "LW  ";
  else
    bandMode = bandModeDesc[currentMode];
                                                  
                                                  
    oled.setCursor(0, 2); // SEND LW MW AM FM to screen.. Position 0.0
    oled.print("    ");
    oled.setCursor(0, 2);
    oled.print(bandMode);
    oled.sendBuffer();

                                               
  
                                                 
}

// Will be used by seekStationProgress
void showFrequencySeek(uint16_t freq)
{
  previousFrequency = currentFrequency = freq;
  showFrequency();
}

/////////////////////////////////////////////////////


///////////////////////////////////////////////
/*
    Show some basic information on display
*/




void showSeparator(){

  oled.drawLine(0, 0, 128, 0);  //--------------------------------
                                //
  oled.drawLine(0, 12, 128, 12);//--------------------------------
                                //
  oled.drawLine(0, 53, 128, 53);//--------------------------------
                                //
  oled.drawLine(0, 63, 128, 63);//--------------------------------
  
}

void showStatus()
{
  //showSeparator();
  showFrequency();

  oled.setCursor(30, 15);
  oled.print("             ");
  oled.setCursor(30, 15);
  oled.print("Fr.St:");
  oled.print("["+(String)currentStep+"kHz]");
  oled.sendBuffer();

  if (currentMode == LSB || currentMode == USB)
  { oled.setCursor(0, 54);
    oled.print("            ");
    oled.setCursor(0, 54);
    oled.print("BW:");
    oled.print(String(bandwidthSSB[bwIdxSSB]));
    oled.print("kHz");
    oled.sendBuffer();                                                  
    showBFO();
                                                     
    
  }
  else if (currentMode == AM)
  { oled.setCursor(0, 54);/// Cleans BW Info
    oled.print("            ");
    oled.setCursor(68, 54); /// Cleans BFO Info
    oled.print("            ");
    oled.setCursor(68, 44); /// Cleans BFO Info
    oled.print("            ");
    oled.setCursor(0, 54);
    oled.print("BW:");
    oled.print(String(bandwidthAM[bwIdxAM]));
    oled.print("kHz");
    oled.sendBuffer();
  }

  // Show AGC Information
    si4735.getAutomaticGainControl();
    oled.setCursor(25, 2);
    oled.print((si4735.isAgcEnabled()) ? "AGC ON " : "AGC OFF");
    oled.sendBuffer();
    showRSSI();
    showVolume();
    oled.sendBuffer();
                                                  
}

/* *******************************
   Shows RSSI status
*/
void showRSSI()
{


  int bars = (rssi / 20.0) + 1;                                                        

  oled.setCursor(88, 2); // Show Signal streng Indicator. Position Should be ~70,0
  oled.print(rssi);

  oled.setCursor(100, 2);
  oled.print("           "); // Update String.
  oled.setCursor(100, 2); // Indicator String
  oled.print(".");
  oled.sendBuffer();
  for (int i = 0; i < bars; i++)
  oled.print('_'); // Signal streng Indicator
  oled.print('|');
  oled.sendBuffer();
                                                                                 

  if (currentMode == FM)
  {
  oled.setCursor(90, 54);
  oled.print("      ");
  oled.setCursor(90, 54);
  // oled.invertOutput(true);
  oled.sendBuffer();
                                                                                      
  if (si4735.getCurrentPilot())
  {
   oled.setCursor(90, 54);
   oled.print("          ");
   oled.setCursor(90, 54);  
   oled.print("Stereo");
   oled.sendBuffer();

    }
    else{
    oled.setCursor(88, 54);                                                                                 
    oled.print("          ");
    oled.setCursor(90, 54);
    oled.print("Mono");
    oled.sendBuffer();

    }
                                                                                       
                                                                                          
  }


}






/*
   Shows the volume level on LCD
*/
void showVolume()
{
  oled.setCursor(65, 2);
  oled.print("  ");
  oled.setCursor(65, 2);
  oled.print("["+(String)si4735.getCurrentVolume()+"]");
  oled.sendBuffer();
}

/*
   Shows the BFO current status.
   Must be called only on SSB mode (LSB or USB)
*/
void showBFO()
{

  String bfo;

  if (currentBFO > 0)
    bfo = "+" + String(currentBFO);
  else
    bfo = String(currentBFO);

    oled.setCursor(68, 54);
    oled.print("            ");
    oled.setCursor(68, 54);
    oled.print("BFO:");
    oled.print((String)bfo+"Hz");
                                                  
    oled.setCursor(68, 45);
    oled.print("             ");
    oled.setCursor(68, 45);
    oled.print("BFO St:");
    oled.print((String)currentBFOStep+"Hz");
    oled.sendBuffer();
}

/*
   Goes to the next band (see Band table)
*/
void bandUp()
{
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;

  if (bandIdx < lastBand)
  {
    bandIdx++;
  }
  else
  {
    bandIdx = 0;
  }
  useBand();
}

/*
   Goes to the previous band (see Band table)
*/
void bandDown()
{
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;
  if (bandIdx > 0)
  {
    bandIdx--;
  }
  else
  {
    bandIdx = lastBand;
  }
  useBand();
}

//////////////////////////////////////////////////////////////////////
/*
 * Show the Station Name. 
 */


void cleanBfoRdsInfo(void)
{
                                                                                       
  oled.setCursor(0, 54);  // Clear RDS String
  oled.print("                  ");
  oled.sendBuffer();
}

 
void showRDSStation()
{
  char *po, *pc;
  int col = 0;

  po = oldBuffer;
  pc = stationName;
  while (*pc)
  {
    if (*po != *pc)
    { 
      oled.setCursor(col, 54);
      oled.print(*pc);
      oled.sendBuffer();
    }
    *po = *pc;
    po++;
    pc++;
    col += 10;
  }
  // strcpy(oldBuffer, stationName);
  delay(100);
}

/*
 * Checks the station name is available
 */
void checkRDS()
{
  si4735.getRdsStatus();
  if (si4735.getRdsReceived())
  {
    if (si4735.getRdsSync() && si4735.getRdsSyncFound() && !si4735.getRdsSyncLost() && !si4735.getGroupLost())
    {
      stationName = si4735.getRdsText0A();
      if (stationName != NULL /* && si4735.getEndGroupB()  && (millis() - rdsElapsed) > 10 */)
      {
        showRDSStation();
        // si4735.resetEndGroupB();
        rdsElapsed = millis();
      }
      ///////////////////////
    } 
    ///////////////////////////
    } else {cleanBfoRdsInfo();};
  
}

//////////////////////////////////////////////////////////////////////


/*
   This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
   SSB mode.
*/
void loadSSB()
{
  oled.clearBuffer();
  oled.setCursor(0, 2);
  oled.print("  Switching to SSB  ");
  oled.sendBuffer();

  si4735.reset();
  si4735.queryLibraryId(); // Is it really necessary here?  Just powerDown() maigh work!
  si4735.patchPowerUp();
  delay(50);
  si4735.setI2CFastMode(); // Recommended
  // si4735.setI2CFastModeCustom(500000); // It is a test and may crash.
  si4735.downloadPatch(ssb_patch_content, size_content);
  si4735.setI2CStandardMode(); // goes back to default (100kHz)
  cleanBfoRdsInfo();

  // delay(50);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bwIdxSSB, 1, 0, 0, 0, 1);
  delay(25);
  ssbLoaded = true;
  oled.clear();
}

/*
   Switch the radio to current band.
   The bandIdx variable points to the current band. 
   This function change to the band referenced by bandIdx (see table band).
*/
void useBand()
{

  cleanBfoRdsInfo();
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentMode = FM;
    si4735.setTuneFrequencyAntennaCapacitor(0);
    si4735.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
    bfoOn = ssbLoaded = false;
    si4735.setFmBandwidth(bandwidthFM[bwIdxFM]);
    si4735.setSeekFmSpacing(1);
    si4735.setRdsConfig(1, 2, 2, 2, 2);
    si4735.setFifoCount(1);
    oled.setCursor(68, 45); // Clean BFO ST string
    oled.print("             ");
  }
  else
  {
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE)
      si4735.setTuneFrequencyAntennaCapacitor(0);
    else
      si4735.setTuneFrequencyAntennaCapacitor(1);

    if (ssbLoaded)
    {
      si4735.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      si4735.setSSBAutomaticVolumeControl(1);
    }
    else
    {
      currentMode = AM;
      si4735.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      si4735.setAutomaticGainControl(1, 0);
      bfoOn = false;
    }
    // Change this value (between 0 to 8); If 0, no Softmute;
    si4735.setSsbSoftMuteMaxAttenuation(4); // Work on AM and SSB -> 0 = Disable Soft Mute for SSB; 1 to 8 Softmute
    // Sets the seeking limits and space.
    si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);               // Consider the range all defined current band
    si4735.setSeekAmSpacing((band[bandIdx].currentStep > 10) ? 10 : band[bandIdx].currentStep); // Max 10kHz for spacing

  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;
  
  showStatus();
}


void loop()
{   

  if (band[bandIdx].bandType == FM_BAND_TYPE){

    if (currentFrequency == previousFrequency)
    {
        checkRDS();   
    }

  }
  
  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn)
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else
    {
      if (encoderCount == 1) {
        si4735.frequencyUp();
        seekDirection = 1;
      }
      else {
        si4735.frequencyDown();
        seekDirection = 0;
      }

      // Show the current frequency only if it has changed
      currentFrequency = si4735.getFrequency();
    }
    encoderCount = 0;

    
  }

  // Check button commands
  if ((millis() - elapsedButton) > MIN_ELAPSED_TIME)
  {
    // check if some button is pressed
    if (digitalRead(BANDWIDTH_BUTTON) == LOW)
    {
      if (currentMode == LSB || currentMode == USB)
      {
        bwIdxSSB++;
        if (bwIdxSSB > 5)
          bwIdxSSB = 0;
        si4735.setSSBAudioBandwidth(bwIdxSSB);
        // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
        if (bwIdxSSB == 0 || bwIdxSSB == 4 || bwIdxSSB == 5)
          si4735.setSSBSidebandCutoffFilter(0);
        else
          si4735.setSSBSidebandCutoffFilter(1);
      }
      else if (currentMode == AM)
      {
        bwIdxAM++;
        if (bwIdxAM > 6)
          bwIdxAM = 0;
        si4735.setBandwidth(bwIdxAM, 1);
      }
      showStatus();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    }
    else if (digitalRead(BAND_BUTTON_UP) == LOW)
      bandUp();
    else if (digitalRead(BAND_BUTTON_DOWN) == LOW)
      bandDown();
    else if (digitalRead(VOL_UP) == LOW)
    {
      si4735.volumeUp();
      volume = si4735.getVolume();
      showVolume();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    }
    else if (digitalRead(VOL_DOWN) == LOW)
    {
      si4735.volumeDown();
      volume = si4735.getVolume();
      showVolume();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    }
    else if (digitalRead(BFO_SWITCH) == LOW)
    {
      if (currentMode == LSB || currentMode == USB) {
        bfoOn = !bfoOn;
        if (bfoOn)
          showBFO();
        showStatus();
      } else {
        si4735.seekStationProgress(showFrequencySeek, seekDirection);
        delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
        currentFrequency = si4735.getFrequency();
        showStatus();
      }
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    }
    else if (digitalRead(AGC_SWITCH) == LOW)
    {
      disableAgc = !disableAgc;
      // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
      si4735.setAutomaticGainControl(disableAgc, 1);
      showStatus();
    }
    else if (digitalRead(STEP_SWITCH) == LOW)
    {
      if ( currentMode == FM) {
        fmStereo = !fmStereo;
        if ( fmStereo )
          si4735.setFmStereoOn();
        else
          si4735.setFmStereoOff(); // It is not working so far.
      } else {

        // This command should work only for SSB mode
        if (bfoOn && (currentMode == LSB || currentMode == USB))
        {
          currentBFOStep = (currentBFOStep == 25) ? 10 : 25;
          showBFO();
        }
        else
        {
          if (currentStep == 1)
            currentStep = 5;
          else if (currentStep == 5)
            currentStep = 10;
          else
            currentStep = 1;
          si4735.setFrequencyStep(currentStep);
          band[bandIdx].currentStep = currentStep;
          showStatus();
        }
        delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      }
    }
    else if (digitalRead(MODE_SWITCH) == LOW)
    {
      if (currentMode != FM ) {
        if (currentMode == AM)
        {
          // If you were in AM mode, it is necessary to load SSB patch (avery time)
          loadSSB();
          currentMode = LSB;
        }
        else if (currentMode == LSB)
        {
          currentMode = USB;
        }
        else if (currentMode == USB)
        {
          currentMode = AM;
          ssbLoaded = false;
          bfoOn = false;
        }
        // Nothing to do if you are in FM mode
        band[bandIdx].currentFreq = currentFrequency;
        band[bandIdx].currentStep = currentStep;
        useBand();
      }
    }
    elapsedButton = millis();
  }

    // Show the current frequency only if it has changed
  if (currentFrequency != previousFrequency)
  {
    previousFrequency = currentFrequency;
    showFrequency();
  };

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 9)
  {
    si4735.getCurrentReceivedSignalQuality();
    int aux = si4735.getCurrentRSSI();
    if (rssi != aux)
    {
      rssi = aux;
      showRSSI();
    }
    elapsedRSSI = millis();
  };

  delay(10);
}
