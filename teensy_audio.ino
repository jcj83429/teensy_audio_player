#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "si5351.h"

//////////////////// I2S clock generator
Si5351 si5351;

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdWav           playSdWav1;     //xy=137.25,90.25
AudioSynthWaveformSine   sine1;          //xy=145.25,162.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
AudioOutputI2Sslave      i2sslave1;      //xy=531.25,160.25
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer2, 0);
AudioConnection          patchCord3(sine1, 0, mixer1, 1);
AudioConnection          patchCord4(sine1, 0, mixer2, 1);
AudioConnection          patchCord5(mixer1, 0, i2sslave1, 0);
AudioConnection          patchCord6(mixer2, 0, i2sslave1, 1);
// GUItool: end automatically generated code

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD

void setSampleRate(unsigned long long sampleRate){
  uint8_t error;
  // clk0 = LRCLK
  error = si5351.set_freq(sampleRate * SI5351_FREQ_MULT, SI5351_CLK0);
  if(error){
    Serial.print("error clk0 ");
    Serial.println(error);
  }
  // clk1 = BCLK
  error = si5351.set_freq(64 * sampleRate * SI5351_FREQ_MULT, SI5351_CLK1);
  if(error){
    Serial.print("error clk1 ");
    Serial.println(error);
  }
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

File sdRoot;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // put your setup code here, to run once:
  AudioMemory(10);
  sine1.amplitude(0.25);
  sine1.frequency(1000);

  // si5351 setup
  bool i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if(i2c_found){
    Serial.print("si5351 found");
  }else{
    while(1){
      Serial.print("si5351 not found");
      delay(1000);
    }
  }
  setSampleRate(44100);

  Serial.print("Initializing SD card...");

  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  Serial.println("initialization done.");

  sdRoot = SD.open("/");
  printDirectory(sdRoot, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
//  Serial.println("start playback");
//  playSdWav1.play("SWEEP4~1.WAV");
//  delay(5);
//  while(playSdWav1.isPlaying());
}
