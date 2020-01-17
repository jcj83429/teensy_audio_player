#include "SdFat.h"

#define FILE_NAME_MAX_LEN 256
#define FILE_NAME_CACHE_SIZE 10000
#define FILE_NAME_CACHE_MAX_ENTRIES 256

char * getCachedFileName(SdBaseFile *dir, uint16_t fileIdx);

extern unsigned long long cacheTime, cacheSearchTime;
