#include "directories.h"
#include <Audio.h>
#include <EEPROM.h>
#include "common.h"
#include "ui.h"
#include "vfd.h"

char fileNameCache[FILE_NAME_CACHE_SIZE];

typedef struct FileNameCacheEntryInfo {
  uint32_t dirFirstCluster;
  uint32_t fileIdx;
  uint16_t position;
} FileNameCacheEntryInfo;

FileNameCacheEntryInfo fileNameCacheEntryInfo[FILE_NAME_CACHE_MAX_ENTRIES];

int fileNameCacheLastEntry = 0; // newest entry index + 1
int fileNameCacheFirstEntry = 0; // oldest entry index

unsigned long long cacheTime = 0;
unsigned long long cacheSearchTime = 0;

extern void suspendDecoding();
extern void resumeDecoding();

char * getCachedFileName(FsFile *dir, uint16_t fileIdx){
  unsigned long long startTime = micros();
  // first search for the file in the cache
  uint32_t dirFirstCluster = dir->firstCluster();
//  Serial.print("(");
//  Serial.print(dirFirstCluster);
//  Serial.print(", ");
//  Serial.print(fileIdx);
//  Serial.println(")");

  int cacheEntry = fileNameCacheFirstEntry;
  while(cacheEntry != fileNameCacheLastEntry){
    FileNameCacheEntryInfo *entryInfo = &fileNameCacheEntryInfo[cacheEntry];
    if(entryInfo->dirFirstCluster == dirFirstCluster &&
       entryInfo->fileIdx == fileIdx){
      // found it
//      Serial.print("found entry at ");
//      Serial.println(entryInfo->position);
      cacheTime += micros() - startTime;
      cacheSearchTime += micros() - startTime;
      return fileNameCache + entryInfo->position;
    }
    cacheEntry = (cacheEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES;
  }
  cacheSearchTime += micros() - startTime;
  
  // not found, clear space in cache for new entry

  int newEntryPos = 0;
  // if last entry == first entry this is the first time. just start writing at 0
  if(fileNameCacheLastEntry != fileNameCacheFirstEntry){
    int lastEntryPos = fileNameCacheEntryInfo[(fileNameCacheLastEntry + FILE_NAME_CACHE_MAX_ENTRIES - 1) % FILE_NAME_CACHE_MAX_ENTRIES].position;
    // try to start writing after last entry, but if that's too close to the end, then start from 0 again
    newEntryPos = lastEntryPos + strlen(&fileNameCache[lastEntryPos]) + 1;
    if(newEntryPos > FILE_NAME_CACHE_SIZE - FILE_NAME_MAX_LEN){
      newEntryPos = 0;
    }

    // if buffer has no space, kick out old entries
    while(fileNameCacheFirstEntry != fileNameCacheLastEntry &&
          fileNameCacheEntryInfo[fileNameCacheFirstEntry].position >= newEntryPos &&
          fileNameCacheEntryInfo[fileNameCacheFirstEntry].position < newEntryPos + FILE_NAME_MAX_LEN){
      fileNameCacheFirstEntry = (fileNameCacheFirstEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES;
    }

    // if entry table is full, kick out old entries
    if(fileNameCacheFirstEntry == (fileNameCacheLastEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES) {
      fileNameCacheFirstEntry = (fileNameCacheFirstEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES;
    }
  }

//  Serial.print("new entry at ");
//  Serial.println(newEntryPos);

  FsFile tmpFile;
  suspendDecoding();
  tmpFile.open(dir, fileIdx, O_RDONLY);
  if(!tmpFile.isOpen()){
    Serial.println("getCachedFileName file open failed");
    displayError("SD CARD ERROR       ", "REBOOTING...        ", 10000);
    delay(2000);
    softReset();
  }
  tmpFile.getName(&fileNameCache[newEntryPos], FILE_NAME_MAX_LEN - 1); // leave 1 byte for "/" in case of dir
  resumeDecoding();
  if(tmpFile.isDir()){
    int fnLen = strlen(&fileNameCache[newEntryPos]);
    fileNameCache[newEntryPos + fnLen] = '/';
    fileNameCache[newEntryPos + fnLen + 1] = 0;
  }
  
  struct FileNameCacheEntryInfo *newEntryInfo = &fileNameCacheEntryInfo[fileNameCacheLastEntry];
  newEntryInfo->dirFirstCluster = dirFirstCluster;
  newEntryInfo->fileIdx = fileIdx;
  newEntryInfo->position = newEntryPos;
  fileNameCacheLastEntry = (fileNameCacheLastEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES;

  cacheTime += micros() - startTime;
  return fileNameCache + newEntryPos;
}

void quicksortFiles(FsFile *dir, uint32_t *idxArr, int len) {
  if (len < 2) return;

  uint16_t pivot = idxArr[len / 2];
  // the pointer returned by the file name cache may become invalid when we access another file name through the cache
  // so make a copy of the pivot file name.
  char pivotFileName[256];
  strcpy(pivotFileName, getCachedFileName(dir, pivot));

  int i, j;
  for (i = 0, j = len - 1; ; i++, j--) {
    while(true){
      if(strcmp(getCachedFileName(dir, idxArr[i]), pivotFileName) < 0){
        i++;
      }else{
        break;
      }
    }
    while(true){
      if(strcmp(getCachedFileName(dir, idxArr[j]), pivotFileName) > 0){
        j--;
      }else{
        break;
      }
    }
 
    if (i >= j) break;
 
    uint16_t temp = idxArr[i];
    idxArr[i] = idxArr[j];
    idxArr[j] = temp;
  }

  quicksortFiles(dir, idxArr, i);
  quicksortFiles(dir, idxArr + i, len - i);
}

FileType getFileType(FsFile *file) {
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

  if(strcasecmp("opus", tmpFileName + fnlen - 4) == 0){
    return FileType::OPUS;
  }

  return FileType::UNSUPPORTED;
}

char * DirectoryNavigator::curDirFileName(int index) {
  if(index >= numFiles[dirStackLevel]){
    return NULL;
  }
  return getCachedFileName(curDir(), sortedFileIdx[dirStackLevel][index]);
}

void DirectoryNavigator::printCurDir() {
  Serial.print("total files: ");
  Serial.println(curDirFiles());
  for(int i=0; i<curDirFiles(); i++){
    FsFile tmpFile;
    uint16_t dirFileIdx = sortedFileIdx[dirStackLevel][i];
    suspendDecoding();
    tmpFile.open(curDir(), dirFileIdx, O_RDONLY);
    resumeDecoding();
    if(i == lastSelectedItem){
      Serial.print("*");
    }
    Serial.print(i);
    Serial.print("\t");
    Serial.print(getCachedFileName(curDir(), dirFileIdx));
    if (tmpFile.isDir()) {
      Serial.println("/");
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println((uint32_t)tmpFile.fileSize(), DEC);
    }
  }
}

FsFile DirectoryNavigator::selectItem(int index) {
  if(index >= curDirFiles()){
    return FsFile();
  }
  lastSelectedItem = index;
  FsFile tmpFile;
  suspendDecoding();
  tmpFile.open(curDir(), sortedFileIdx[dirStackLevel][index], O_RDONLY);
  resumeDecoding();
  if(!tmpFile.isOpen()){
    Serial.println("selectItem failed to open item");
    displayError("SD CARD ERROR       ", "REBOOTING...        ", 10000);
    delay(2000);
    softReset();
  }
  if(tmpFile.isDir()){
    if(dirStackLevel < MAX_DIR_STACK -1){
      dirStackLevel++;
      parentDirIdx[dirStackLevel] = index;
      dirStack[dirStackLevel] = tmpFile;
      loadCurDir();
      lastSelectedItem = -1;
    }else{
      displayError("DIR DEPTH LIMIT (4) ", "REACHED             ", 5000);
    }
    return FsFile();
  }else{
    return tmpFile;
  }
}

FsFile DirectoryNavigator::nextFile() {
  FsFile ret;
  int laps = 0;
  while(!ret.isOpen()){
    if(lastSelectedItem >= curDirFiles() - 1){
      // end of current dir
      if(dirStackLevel > 0){
        upDir();
        continue;
      }else{
        if(curDirFiles()){
          if(laps){
            return ret;
          }else{
            laps++;
          }
          // if we're at root, we can't go up. so start from beginning again.
          lastSelectedItem = -1;
        }else{
          // no files at root
          return ret;
        }
      }
    }
    ret = selectItem(lastSelectedItem + 1);
  }
  return ret;
}

FsFile DirectoryNavigator::prevFile() {
  FsFile ret;
  int laps = 0;
  while(!ret.isOpen()){
    if(lastSelectedItem <= 0){
      if(dirStackLevel > 0){
        upDir();
        continue;
      }else{
        if(curDirFiles()){
          if(laps){
            return ret;
          }else{
            laps++;
          }
          lastSelectedItem = curDirFiles();
        }else{
          // no files at root
          return ret;
        }
      }
    }
    int oldDSL = dirStackLevel;
    ret = selectItem(lastSelectedItem - 1);
    if(!ret.isOpen()){
      // normally we set lastSelectedItem when we go into a dir
      // but we want to go in reverse so set it to curDirFiles()
      // The exception is when we were already at max directory depth.
      // In that case, we just skip the directory.
      if(oldDSL < MAX_DIR_STACK -1){
        lastSelectedItem = curDirFiles();
      }
    }
  }
  return ret;
}

bool DirectoryNavigator::upDir() {
  if(dirStackLevel <= 0){
    return false;
  }
  lastSelectedItem = parentDirIdx[dirStackLevel];
  // it's probably not necessary to suspend decoding but whatever
  suspendDecoding();
  dirStack[dirStackLevel].close();
  resumeDecoding();
  dirStackLevel--;
  return true;
}

// TODO: also save crc of the filenames in case files were added or deleted
void DirectoryNavigator::saveCurrentFile(){
  if(lastSelectedItem == -1){
    // this dirnav is never used
    return;
  }
  // level 0 is root, so don't save it
  int offset = 0;
  for(int i = 0; i < dirStackLevel; i++){
    EEPROM.write(EEPROM_OFFSET_DIRSTACK + offset, parentDirIdx[i + 1]);
    offset++;
  }
  EEPROM.write(EEPROM_OFFSET_DIRSTACK + offset, lastSelectedItem);
}

FsFile DirectoryNavigator::restoreCurrentFile(){
  FsFile ret;
  while(upDir()); // go to root dir
  for(int i = 0; i < MAX_DIR_STACK; i++){
    int itemIdx = EEPROM.read(EEPROM_OFFSET_DIRSTACK + i);
    if(itemIdx >= curDirFiles()){
      return FsFile(); // error
    }
    ret = selectItem(itemIdx);
    if(ret.isOpen()){
      return ret;
    }
  }
  return FsFile(); // error
}

void DirectoryNavigator::loadCurDir() {
  unsigned long long startTime = micros();
  int nf = 0;
  while(nf < MAX_DIR_FILES){
    FsFile tmpFile;
    suspendDecoding();
    if(!tmpFile.openNext(curDir())){
      resumeDecoding();
      break;
    }

    if(getFileType(&tmpFile) == FileType::UNSUPPORTED){
      resumeDecoding();
      continue;
    }
    sortedFileIdx[dirStackLevel][nf] = tmpFile.dirIndex();
    nf++;
    resumeDecoding();
  }
  if(nf == MAX_DIR_FILES){
    displayError("DIR CHILD LIMIT     ", "(256) REACHED       ", 5000);
  }
  numFiles[dirStackLevel] = nf;
  
  quicksortFiles(curDir(), sortedFileIdx[dirStackLevel], nf);
  Serial.print("dir load took ");
  Serial.println((int)(micros() - startTime));
}
