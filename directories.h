#ifndef DIRECTORIES_H
#define DIRECTORIES_H

#include "SdFat.h"

#define FILE_NAME_MAX_LEN 256
#define FILE_NAME_CACHE_SIZE 10000
#define FILE_NAME_CACHE_MAX_ENTRIES 256

#define MAX_DIR_STACK 4
#define MAX_DIR_FILES 256

enum FileType {
  UNSUPPORTED,
  DIR,
  MP3,
  AAC,
  FLAC,
};

char * getCachedFileName(SdBaseFile *dir, uint16_t fileIdx);
void quicksortFiles(SdBaseFile *dir, uint16_t *idxArr, int len);
FileType getFileType(SdBaseFile *file);

extern unsigned long long cacheTime, cacheSearchTime;

class DirectoryNavigator {
public:
  void openRoot(const FatVolume *vol) {
    dirStack[0].openRoot(const_cast<FatVolume*>(vol));
    dirStackLevel = 0;
    loadCurDir();
  }

  int lastSelectedItem = -1; // for convenience. we update it when doing upDir.

  SdBaseFile *curDir() { return &dirStack[dirStackLevel]; }
  int curDirFiles() { return numFiles[dirStackLevel]; }
  char *curDirFileName(int index); // fast access through cache
  void printCurDir();
  SdBaseFile selectItem(int index); // For file, open file in ret. For dir, return SdBaseFile()
  SdBaseFile nextFile();
  SdBaseFile prevFile();
  bool upDir();

  void saveCurrentFile(); // save which file is currently selected
  SdBaseFile restoreCurrentFile(); // restore currently selected file and return it

//protected:
  int dirStackLevel; // -1 is uninitialized. 0 is root
  SdBaseFile dirStack[MAX_DIR_STACK];
  uint16_t parentDirIdx[MAX_DIR_STACK]; // what index is each dir in its parent dir?
  int numFiles[MAX_DIR_STACK];
  uint16_t sortedFileIdx[MAX_DIR_STACK][MAX_DIR_FILES];

  void loadCurDir();
};

#endif
