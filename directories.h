#ifndef DIRECTORIES_H
#define DIRECTORIES_H

#include "SdFat.h"

#define FILE_NAME_MAX_LEN 256
#define FILE_NAME_CACHE_SIZE 10000
#define FILE_NAME_CACHE_MAX_ENTRIES 256

#define MAX_DIR_STACK 8
#define MAX_DIR_FILES 256

enum FileType {
  UNSUPPORTED,
  DIR,
  MP3,
  AAC,
  FLAC,
  OPUS,
  MODULE,
};

char * getCachedFileName(FsFile *dir, uint16_t fileIdx);
void quicksortFiles(FsFile *dir, uint16_t *idxArr, int len);
FileType getFileType(FsFile *file);

extern unsigned long long cacheTime, cacheSearchTime;

class DirectoryNavigator {
public:
  void openRoot(FsVolume *vol) {
    dirStack[0].open(vol, "/", O_READ);;
    dirStackLevel = 0;
    loadCurDir();
  }

  int lastSelectedItem = -1; // for convenience. we update it when doing upDir.

  FsFile *curDir() { return &dirStack[dirStackLevel]; }
  int curDirFiles() { return numFiles[dirStackLevel]; }
  char *curDirFileName(int index); // fast access through cache
  void printCurDir();
  FsFile selectItem(int index); // For file, open file in ret. For dir, return FsFile()
  FsFile nextFile();
  FsFile prevFile();
  bool upDir();

  void saveCurrentFile(); // save which file is currently selected
  FsFile restoreCurrentFile(); // restore currently selected file and return it

//protected:
  int dirStackLevel; // -1 is uninitialized. 0 is root
  FsFile dirStack[MAX_DIR_STACK];
  uint16_t parentDirIdx[MAX_DIR_STACK]; // what index is each dir in its parent dir?
  int numFiles[MAX_DIR_STACK];
  uint32_t sortedFileIdx[MAX_DIR_STACK][MAX_DIR_FILES];

  void loadCurDir();
};

#endif
