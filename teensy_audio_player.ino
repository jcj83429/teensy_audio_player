#include "SdFat.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "si5351.h"
#include <play_sd_mp3.h>

#include "mycodecfile.cpp"

//////////////////// I2S clock generator
Si5351 si5351;

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;     //xy=137.25,90.25
AudioSynthWaveformSine   sine1;          //xy=145.25,162.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
AudioOutputI2Sslave      i2sslave1;      //xy=531.25,160.25
AudioConnection          patchCord1(playMp31, 0, mixer1, 0);
AudioConnection          patchCord2(playMp31, 1, mixer2, 0);
AudioConnection          patchCord3(sine1, 0, mixer1, 1);
AudioConnection          patchCord4(sine1, 0, mixer2, 1);
AudioConnection          patchCord5(mixer1, 0, i2sslave1, 0);
AudioConnection          patchCord6(mixer2, 0, i2sslave1, 1);
// GUItool: end automatically generated code

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD

SdFatSdioEX sdEx;
SdBaseFile sdRoot;

char tmpFileName[256];
#define MAX_FILES 100
int numFiles = 0;
SdBaseFile curDirFiles[MAX_FILES];
SdBaseFile *sortedCurDirFiles[MAX_FILES];
MyCodecFile myCodecFile(NULL);

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

void quicksortFiles(SdBaseFile **A, int len) {
  if (len < 2) return;
 
  SdBaseFile *pivot = A[len / 2];
  char pivotFileName[256];
  pivot->getName(pivotFileName, sizeof(pivotFileName));

  int i, j;
  for (i = 0, j = len - 1; ; i++, j--) {
    while(true){
      A[i]->getName(tmpFileName, sizeof(tmpFileName));
      if(strcmp(tmpFileName, pivotFileName) < 0){
        i++;
      }else{
        break;
      }
    }
    while(true){
      A[j]->getName(tmpFileName, sizeof(tmpFileName));
      if(strcmp(tmpFileName, pivotFileName) > 0){
        j--;
      }else{
        break;
      }
    }
 
    if (i >= j) break;
 
    SdBaseFile *temp = A[i];
    A[i]     = A[j];
    A[j]     = temp;
  }

  quicksortFiles(A, i);
  quicksortFiles(A + i, len - i);
}

bool isSupportedFile(SdBaseFile *file) {
  // directory = OK
  if(file->isDir()){
    return true;
  }

  // file: check extension
  file->getName(tmpFileName, sizeof(tmpFileName));
  int fnlen = strlen(tmpFileName);
  if(strcasecmp("mp3", tmpFileName + fnlen - 3) == 0){
    return true;
  }

  return false;
}

void loadDirectory(SdBaseFile *dir) {
  // close all old curDirFiles
  for(int i=0; i<numFiles; i++){
    curDirFiles[i].close();
  }
  
  numFiles = 0;

  while(numFiles < MAX_FILES){
    if(!curDirFiles[numFiles].openNext(dir)){
      break;
    }

    if(!isSupportedFile(&curDirFiles[numFiles])){
      curDirFiles[numFiles].close();
      continue;
    }

    sortedCurDirFiles[numFiles] = &curDirFiles[numFiles];
    numFiles++;
  }

  quicksortFiles(sortedCurDirFiles, numFiles);
}

void printCurDir(){
  for(int i=0; i<numFiles; i++){
    SdBaseFile *f = sortedCurDirFiles[i];
    f->getName(tmpFileName, sizeof(tmpFileName));
    Serial.print(tmpFileName);
    if (f->isDir()) {
      Serial.println("/");
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(f->fileSize(), DEC);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // put your setup code here, to run once:
  AudioMemory(20);

  // FOR DAC SANITY
  // sine1.amplitude(0.25);
  // sine1.frequency(1000);

  // si5351 setup
  bool i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if(i2c_found){
    Serial.println("si5351 found");
  }else{
    while(1){
      Serial.println("si5351 not found");
      delay(1000);
    }
  }
  setSampleRate(44100);

  if (!sdEx.begin()) {
    sdEx.initErrorHalt("SdFatSdioEX begin() failed");
  } else {
    Serial.println("SD init success");
  }

  Serial.println("ALL INIT DONE!");
  
  // make sdEx the current volume.
  sdEx.chvol();

  //sdEx.ls(LS_R | LS_DATE | LS_SIZE);

  // populate curDirFiles with files in root. Directories are not supported yet
  sdRoot.open("/");
  loadDirectory(&sdRoot);
  if(numFiles){
    playFile(sortedCurDirFiles[0]);
  }else{
    while(true){
      Serial.println("NO FILES");
      delay(1000);
    }
  }
}

void playFile(SdBaseFile *file) {
  file->getName(tmpFileName, sizeof(tmpFileName));
  Serial.print("play file: ");
  Serial.println(tmpFileName);
  playMp31.stop();
  file->rewind();
  myCodecFile = MyCodecFile(file);
  playMp31.load(&myCodecFile);
  playMp31.play();
}

int currentFileIndex = 0;
void loop() {
  // put your main code here, to run repeatedly:
  if(Serial.available()){
    char c = Serial.read();
    switch(c){
      case 'D':
        printCurDir();
        break;
      case 'N':
        currentFileIndex = (currentFileIndex + 1) % numFiles;
        playFile(sortedCurDirFiles[currentFileIndex]);
        break;
      case 'P':
        currentFileIndex = (currentFileIndex + numFiles - 1) % numFiles;
        playFile(sortedCurDirFiles[currentFileIndex]);
        break;
      default:
        Serial.print("unknown cmd ");
        Serial.println(c);
        break;
    }
  }
}
