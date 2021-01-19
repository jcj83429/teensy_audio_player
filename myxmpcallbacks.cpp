#if defined(__IMXRT1062__)

#include <SdFat.h>

// Do my_xmp_read in two steps:
// first read from SD card to DMAMEM (malloc) then copy from DMAMEM to PSRAM
// This slows down the reads but seems to improve the reliability of SD card reads
// Maybe the SD card and PSRAM are interfering with each other because they're so close
// This is no longer necessary with the SdFat drive strength fix.
#define TWO_STEP_XMP_FILE_READ 0

size_t my_xmp_read(void *user_data, void *buf, size_t size, size_t num){
/*
  Serial.print(__FUNCTION__);
  Serial.print(" ");
  Serial.print((uint32_t) buf, HEX);
  Serial.print(" ");
  Serial.print(size);
  Serial.print(" ");
  Serial.println(num);
*/
  
#if TWO_STEP_XMP_FILE_READ
  // All of libxmp's memory allocations are in the PSRAM,
  // so there is plenty of malloc memory available
  const int XMP_READ_BUF_SIZE = 128*1024;
  char *xmp_read_buf = (char*) malloc(XMP_READ_BUF_SIZE);
  if(!xmp_read_buf){
    return 0;
  }
  size_t total_bytes_to_read = size * num;
  size_t total_bytes_read = 0;
  char *charBuf = (char*)buf;
  FsFile *file = (FsFile *)user_data;
  while(total_bytes_read < total_bytes_to_read){
    size_t bytes_to_read = min(total_bytes_to_read - total_bytes_read, sizeof(xmp_read_buf));
    size_t bytes_read = file->read(xmp_read_buf, bytes_to_read);
    if(bytes_read != bytes_to_read){
      Serial.print("my_xmp_read: tried to read ");
      Serial.print(bytes_to_read);
      Serial.print(" bytes but got ");
      Serial.print(bytes_read);
      Serial.print(" bytes. read a total of ");
      Serial.print(total_bytes_read);
      Serial.print("/");
      Serial.print(total_bytes_to_read);
      Serial.println("bytes");
      break;
    }else{
      memcpy(charBuf, xmp_read_buf, bytes_read);
      arm_dcache_flush_delete(charBuf, bytes_read);
      charBuf += bytes_read;
      total_bytes_read += bytes_read;
    }
  }
  free(xmp_read_buf);
#else
  FsFile *file = (FsFile *)user_data;
  int total_bytes_read = file->read(buf, size * num);
  if(total_bytes_read != size * num){
    Serial.print("my_xmp_read: tried to read ");
    Serial.print(size * num);
    Serial.print(" bytes but got ");
    Serial.print(total_bytes_read);
    Serial.println(" bytes. ");
  }
#endif
  /*
  Serial.print("return ");
  Serial.println(bytes_read / size);
  */
  return total_bytes_read / size;
}

int my_xmp_seek(void *user_data, long offset, int whence){
/*
  Serial.print(__FUNCTION__);
  Serial.print(" ");
  Serial.print(offset);
  Serial.print(" ");
  Serial.println(whence);
*/
  FsFile *file = (FsFile *)user_data;
  bool success;
  if(whence == SEEK_SET){
    success = file->seek(offset);
  }else if(whence == SEEK_CUR){
    success = file->seekCur(offset);
  }else if(whence == SEEK_END){
    success = file->seekEnd(offset);
  }else{
	/*
    Serial.print("unsupported whence for seeking: ");
    Serial.print(whence);
	*/
    return -1;
  }
  return success ? 0 : -1;
}

long my_xmp_tell(void *user_data){
/*
  Serial.println(__FUNCTION__);
*/
  FsFile *file = (FsFile *)user_data;
  return file->curPosition();
}

int my_xmp_eof(void *user_data){
/*
  Serial.println(__FUNCTION__);
*/
  FsFile *file = (FsFile *)user_data;
  return !file->available();
}

long my_xmp_size(void *user_data){
/*
  Serial.print(__FUNCTION__);
*/
  FsFile *file = (FsFile *)user_data;
  return !file->fileSize();
}

#endif
