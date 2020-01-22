#include "directories.h"
#include <Audio.h>

char fileNameCache[FILE_NAME_CACHE_SIZE];

typedef struct FileNameCacheEntryInfo {
  uint32_t dirFirstCluster;
  uint16_t fileIdx;
  uint16_t position;
} FileNameCacheEntryInfo;

FileNameCacheEntryInfo fileNameCacheEntryInfo[FILE_NAME_CACHE_MAX_ENTRIES];

int fileNameCacheLastEntry = 0; // newest entry index + 1
int fileNameCacheFirstEntry = 0; // oldest entry index

unsigned long long cacheTime = 0;
unsigned long long cacheSearchTime = 0;

extern void suspendDecoding();
extern void resumeDecoding();

char * getCachedFileName(SdBaseFile *dir, uint16_t fileIdx){
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

  SdBaseFile tmpFile;
  suspendDecoding();
  tmpFile.open(dir, fileIdx, O_RDONLY);
  tmpFile.getName(&fileNameCache[newEntryPos], FILE_NAME_MAX_LEN);
  resumeDecoding();
  struct FileNameCacheEntryInfo *newEntryInfo = &fileNameCacheEntryInfo[fileNameCacheLastEntry];
  newEntryInfo->dirFirstCluster = dirFirstCluster;
  newEntryInfo->fileIdx = fileIdx;
  newEntryInfo->position = newEntryPos;
  fileNameCacheLastEntry = (fileNameCacheLastEntry + 1) % FILE_NAME_CACHE_MAX_ENTRIES;

  cacheTime += micros() - startTime;
  return fileNameCache + newEntryPos;
}

void quicksortFiles(SdBaseFile *dir, uint16_t *idxArr, int len) {
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
