#include "SdFat.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "si5351.h"
#include <play_sd_mp3.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>

#include "mycodecfile.cpp"

//////////////////// I2S clock generator
Si5351 si5351;

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;     //xy=135.25,237.25
AudioPlaySdAac           playAac1;     //xy=136.25,186.25
AudioPlaySdFlac          playFlac1;     //xy=137.25,90.25
AudioSynthWaveformSine   sine1;          //xy=139.25,135.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
AudioOutputI2Sslave      i2sslave1;      //xy=531.25,160.25
AudioConnection          patchCord5(playMp31, 0, mixer1, 0);
AudioConnection          patchCord6(playMp31, 1, mixer2, 0);
AudioConnection          patchCord3(playAac1, 0, mixer1, 2);
AudioConnection          patchCord4(playAac1, 1, mixer2, 2);
AudioConnection          patchCord1(playFlac1, 0, mixer1, 3);
AudioConnection          patchCord2(playFlac1, 1, mixer2, 3);
AudioConnection          patchCord7(sine1, 0, mixer1, 1);
AudioConnection          patchCord8(sine1, 0, mixer2, 1);
AudioConnection          patchCord9(mixer1, 0, i2sslave1, 0);
AudioConnection          patchCord10(mixer2, 0, i2sslave1, 1);
// GUItool: end automatically generated code

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD

#define LED_PIN 13

SdFatSdioEX sdEx;
SdBaseFile sdRoot;

char tmpFileName[256];
#define MAX_FILES 100
int numFiles = 0;
SdBaseFile curDirFiles[MAX_FILES];
SdBaseFile *sortedCurDirFiles[MAX_FILES];
MyCodecFile myCodecFile(NULL);
int currentFileIndex = -1;

enum FileType {
  UNSUPPORTED,
  DIR,
  MP3,
  AAC,
  FLAC,
};

void setSampleRate(unsigned long long sampleRate){
  uint8_t error;
  // sometimes the sample rate setting doens't go through, so repeate 3 times
  for(int i=0; i<3; i++){
    // clk0 = LRCLK
    error = si5351.set_freq(sampleRate * SI5351_FREQ_MULT, SI5351_CLK0);
    if(error){
      Serial.print("error clk0 ");
      Serial.println(error);
      return;
    }
    // clk1 = BCLK
    error = si5351.set_freq(64 * sampleRate * SI5351_FREQ_MULT, SI5351_CLK1);
    if(error){
      Serial.print("error clk1 ");
      Serial.println(error);
      return;
    }
  }
  Serial.print("sample rate changed to ");
  Serial.println((int)sampleRate);
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

FileType getFileType(SdBaseFile *file) {
  if(file->isDir()){
    return FileType::DIR;
  }

  // file: check extension
  file->getName(tmpFileName, sizeof(tmpFileName));
  int fnlen = strlen(tmpFileName);
  
  if(strcasecmp("mp3", tmpFileName + fnlen - 3) == 0){
    return FileType::MP3;
  }
  if(strcasecmp("aac", tmpFileName + fnlen - 3) == 0 ||
     strcasecmp("mp4", tmpFileName + fnlen - 3) == 0 ||
     strcasecmp("m4a", tmpFileName + fnlen - 3) == 0){
    return FileType::AAC;
  }
  if(strcasecmp("flac", tmpFileName + fnlen - 4) == 0){
    return FileType::FLAC;
  }

  return FileType::UNSUPPORTED;
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

    if(getFileType(&curDirFiles[numFiles]) == FileType::UNSUPPORTED){
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

void playFile(SdBaseFile *file) {
  file->getName(tmpFileName, sizeof(tmpFileName));
  Serial.print("play file: ");
  Serial.println(tmpFileName);
  
  if(playMp31.isPlaying()){
    playMp31.stop();
  }
  if(playAac1.isPlaying()){
    playAac1.stop();
  }
  if(playFlac1.isPlaying()){
    playFlac1.stop();
  }
  
  file->rewind();
  myCodecFile = MyCodecFile(file);
  int error = 0;
  switch(getFileType(file)){
    case FileType::MP3:
      playMp31.load(&myCodecFile);
      error = playMp31.play();
      break;
    case FileType::AAC:
      playAac1.load(&myCodecFile);
      error = playAac1.play();
      break;
    case FileType::FLAC:
      playFlac1.load(&myCodecFile);
      error = playFlac1.play();
      break;
    default:
      Serial.println("WTF attempting to play dir or unsupported file?");
      break;
  }

  Serial.print("error: 0x");
  Serial.println(error, HEX);
}

int findPlayableFile(int step){
  int filesTried = 0;
  int fileIndex = currentFileIndex;
  while(filesTried < numFiles){
    fileIndex = (fileIndex + numFiles + step) % numFiles;
    SdBaseFile *f = sortedCurDirFiles[fileIndex];
    if(f->isFile()){
      return fileIndex;
    }
    filesTried++;
  }
  return -1;
}

void playNext(){
  int newFileIndex = findPlayableFile(1);
  if(newFileIndex < 0){
    Serial.println("no playable file");
  }
  currentFileIndex = newFileIndex;
  playFile(sortedCurDirFiles[currentFileIndex]);
}

void playPrev(){
  int newFileIndex = findPlayableFile(-1);
  if(newFileIndex < 0){
    Serial.println("no playable file");
  }
  currentFileIndex = newFileIndex;
  playFile(sortedCurDirFiles[currentFileIndex]);
}

void flashError(int error){
  for(int i=0; i<error; i++){
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
  delay(1000);
}

void printStatus(){
  Serial.print("playing: ");
  Serial.print(playMp31.isPlaying());
  Serial.print(playAac1.isPlaying());
  Serial.print(playFlac1.isPlaying());
  Serial.print(", pos: ");
  Serial.print(myCodecFile.fposition());
  Serial.print("/");
  Serial.println(myCodecFile.fsize());
}

void testSeek(){
  // don't seek too close to end of file or playback may end
  uint32_t seekToTime = random(0, (int)playAac1.lengthMillis() * 0.9 / 1000);
  Serial.print("seeking to ");
  Serial.println(seekToTime);
  bool result = playAac1.seek(seekToTime);
  Serial.print("result: ");
  Serial.println(result);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);

  // put your setup code here, to run once:
  AudioMemory(20);

  // FOR DAC SANITY
  // sine1.amplitude(0.20);
  // sine1.frequency(1000);

  // si5351 setup
  bool si5351_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if(si5351_found){
    Serial.println("si5351 found");
  }else{
    while(1){
      Serial.println("si5351 not found");
      flashError(1);
    }
  }
  setSampleRate(44100);

  if (!sdEx.begin()) {
    while(1){
      Serial.println("SdFatSdioEX begin() failed");
      flashError(2);
    }
  } else {
    Serial.println("SD init success");
  }

  Serial.println("ALL INIT DONE!");
  
  // make sdEx the current volume.
  sdEx.chvol();

  sdEx.ls(LS_R | LS_DATE | LS_SIZE);

  // populate curDirFiles with files in root. Directories are not supported yet
  sdRoot.open("/");
  loadDirectory(&sdRoot);
  if(numFiles){
    playNext();
  }else{
    while(true){
      Serial.println("NO FILES");
      delay(1000);
    }
  }
}

int sampleRates[] = {44100, 88200, 22050};
int sampleRateIndex = 0;


unsigned long lastStatusTime = 0;
void loop() {
  // put your main code here, to run repeatedly:
  if(Serial.available()){
    char c = Serial.read();
    switch(c){
      case 'D':
        printCurDir();
        break;
      case 'N':
        playNext();
        break;
      case 'P':
        playPrev();
        break;
      case 'R':
        sampleRateIndex = (sampleRateIndex + 1) % 3;
        setSampleRate(sampleRates[sampleRateIndex]);
        break;
      case 'S':
        printStatus();
        break;
      case 'T':
        testSeek();
        break;
      default:
        Serial.print("unknown cmd ");
        Serial.println(c);
        break;
    }
  }
}
