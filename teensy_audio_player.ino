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
#include "directories.h"

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define LED_PIN 13

#define MAX_FILES 256

//////////////////// I2S clock generator
Si5351 si5351;

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;     //xy=100.25,89.25
AudioPlaySdAac           playAac1;     //xy=110.25,130.25
AudioPlaySdFlac          playFlac1;     //xy=119.25,173.25
AudioSynthWaveformSine   sine1;          //xy=150.25,217.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
AudioOutputI2Sslave      i2sslave1;      //xy=592.25,164.25
AudioMixer4              mixer3;         //xy=723.25,165.25
AudioAnalyzeFFT256       fft256_1;       //xy=860.25,165.25
AudioConnection          patchCord1(playMp31, 0, mixer1, 0);
AudioConnection          patchCord2(playMp31, 1, mixer2, 0);
AudioConnection          patchCord3(playAac1, 0, mixer1, 1);
AudioConnection          patchCord4(playAac1, 1, mixer2, 1);
AudioConnection          patchCord5(playFlac1, 0, mixer1, 2);
AudioConnection          patchCord6(playFlac1, 1, mixer2, 2);
AudioConnection          patchCord7(sine1, 0, mixer1, 3);
AudioConnection          patchCord8(sine1, 0, mixer2, 3);
AudioConnection          patchCord9(mixer1, 0, i2sslave1, 0);
AudioConnection          patchCord10(mixer1, 0, mixer3, 0);
AudioConnection          patchCord11(mixer2, 0, i2sslave1, 1);
AudioConnection          patchCord12(mixer2, 0, mixer3, 3);
AudioConnection          patchCord13(mixer3, fft256_1);
// GUItool: end automatically generated code

SdFatSdioEX sdEx;

SdBaseFile curDir;
int numFiles = 0;
uint16_t curDirFileIdx[MAX_FILES];

SdBaseFile currentFile;
MyCodecFile myCodecFile(NULL);
int currentFileIndex = -1;

enum FileType {
  UNSUPPORTED,
  DIR,
  MP3,
  AAC,
  FLAC,
};

// we can't disable the audio interrupt because otherwise the audio output will glitch.
// so we suspend the decoders' decoding while keeping their outputs running
void suspendDecoding(){
  // for MP3 and AAC, just disable the ISR
  if(playMp31.isPlaying() || playAac1.isPlaying()){
    NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the suspend/resumeDecoding interface
  if(playFlac1.isPlaying()){
    playFlac1.suspendDecoding();
  }
}

void resumeDecoding(){
  // for MP3 and AAC, just enable and trigger the ISR
  if(playMp31.isPlaying() || playAac1.isPlaying()){
    NVIC_ENABLE_IRQ(IRQ_AUDIOCODEC);
    NVIC_TRIGGER_INTERRUPT(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the resumeDecoding interface to safely fill the buffer
  if(playFlac1.isPlaying()){
    playFlac1.resumeDecoding();
  }
}

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

FileType getFileType(SdBaseFile *file) {
  if(file->isDir()){
    return FileType::DIR;
  }

  // file: check extension
  char tmpFileName[256];
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

void loadCurDir() {
  numFiles = 0;

  while(numFiles < MAX_FILES){
    SdBaseFile tmpFile;
    suspendDecoding();
    if(!tmpFile.openNext(&curDir)){
      resumeDecoding();
      break;
    }

    if(getFileType(&tmpFile) == FileType::UNSUPPORTED){
      resumeDecoding();
      continue;
    }
    curDirFileIdx[numFiles] = tmpFile.dirIndex();
    numFiles++;
    resumeDecoding();
  }

  quicksortFiles(&curDir, curDirFileIdx, numFiles);
}

void printCurDir(){
  Serial.print("total files: ");
  Serial.println(numFiles);
  for(int i=0; i<numFiles; i++){
    SdBaseFile tmpFile;
    tmpFile.open(&curDir, curDirFileIdx[i], O_RDONLY);
    Serial.print(getCachedFileName(&curDir, curDirFileIdx[i]));
    if (tmpFile.isDir()) {
      Serial.println("/");
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(tmpFile.fileSize(), DEC);
    }
  }
}

AudioCodec *getPlayingCodec(){
  if(playMp31.isPlaying()){
    return &playMp31;
  }else if(playAac1.isPlaying()){
    return &playAac1;
  }else if(playFlac1.isPlaying()){
    return &playFlac1;
  }
  return NULL;
}

void playFile(SdBaseFile *file) {
  char tmpFileName[256];
  file->getName(tmpFileName, sizeof(tmpFileName));
  Serial.print("play file: ");
  Serial.println(tmpFileName);
  
  AudioCodec *playingCodec = getPlayingCodec();
  if(playingCodec){
    playingCodec->stop();
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
    SdBaseFile tmpFile;
    tmpFile.open(&curDir, curDirFileIdx[fileIndex], O_RDONLY);
    if(tmpFile.isFile()){
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
  currentFile.close();
  currentFile.open(&curDir, curDirFileIdx[currentFileIndex], O_RDONLY);
  playFile(&currentFile);
}

void playPrev(){
  int newFileIndex = findPlayableFile(-1);
  if(newFileIndex < 0){
    Serial.println("no playable file");
  }
  currentFileIndex = newFileIndex;
  currentFile.close();
  currentFile.open(&curDir, curDirFileIdx[currentFileIndex], O_RDONLY);
  playFile(&currentFile);
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
  Serial.print(myCodecFile.fsize());
  
  AudioCodec *playingCodec = getPlayingCodec();
  if(playingCodec){
    Serial.print(" time: ");
    Serial.print(playingCodec->positionMillis());
    Serial.print("/");
    Serial.print(playingCodec->lengthMillis());
  }

  Serial.println();
}

void seekAbsolute(uint32_t timesec){
  AudioCodec *playingCodec = getPlayingCodec();
  if(playingCodec){
    if(timesec > playingCodec->lengthMillis() / 1000){
      return;
    }
    Serial.print("seeking to ");
    Serial.println(timesec);
    bool result = playingCodec->seek(timesec);
    Serial.print("result: ");
    Serial.println(result);
    Serial.print("positionMillis: ");
    Serial.println(playingCodec->positionMillis());
  }
}

void testSeek(){
  AudioCodec *playingCodec = getPlayingCodec();
  if(playingCodec){
    uint32_t seekToTime = random(0, (int)playingCodec->lengthMillis() * 0.9 / 1000);
    seekAbsolute(seekToTime);
  }
}

void seekRelative(int dtsec){
  AudioCodec *playingCodec = getPlayingCodec();
  if(playingCodec){
    seekAbsolute(playingCodec->positionMillis() / 1000 + dtsec);
  }
}

void testDirSort(){
  cacheTime = cacheSearchTime = 0;
  suspendDecoding();
  curDir.close();
  curDir.open("www.saturn-global.com_david 80s music collection");
  resumeDecoding();
  unsigned long long startTime = micros();
  loadCurDir();
  Serial.print("took ");
  Serial.println((int)(micros() - startTime));
  Serial.print("cache took ");
  Serial.println((int)cacheTime);
  Serial.print("cache search took ");
  Serial.println((int)cacheSearchTime);
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

  // sdEx.ls(LS_R | LS_DATE | LS_SIZE);

  // populate curDirFiles with files in root. Directories are not supported yet
  curDir.open("/");
  loadCurDir();

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
      case 'A':
        testDirSort();
        break;
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
      case '>':
        seekRelative(+5);
        break;
      case '<':
        seekRelative(-5);
        break;
      default:
        Serial.print("unknown cmd ");
        Serial.println(c);
        break;
    }
  }
}
