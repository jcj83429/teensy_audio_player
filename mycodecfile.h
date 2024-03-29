#ifndef MYCODECFILE_H
#define MYCODECFILE_H

#include <codecs.h>
#include <SdFat.h>

class MyCodecFile : public CodecFileBase {
public:
  MyCodecFile(FsFile *file) {
    sdFile = file;
  }

  bool f_eof(void) {
    return !sdFile->available();
  }
  bool fseek(const size_t position) {
    return sdFile->seekSet(position);
  }
  size_t fposition(void) {
    return sdFile->curPosition();
  }
  size_t fsize(void) {
    return sdFile->fileSize();
  }
  size_t fread(uint8_t buffer[],size_t bytes) {
    int readBytes = sdFile->read(buffer, bytes);
    if(readBytes < 0){
      Serial.print("SD read failed with ");
      Serial.println(readBytes);
      return 0;
    } else {
      return readBytes;
    }
  }

protected:
  FsFile *sdFile;
};

#endif
