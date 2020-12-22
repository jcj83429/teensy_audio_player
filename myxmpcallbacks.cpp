#include <SdFat.h>

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
  FsFile *file = (FsFile *)user_data;
  int bytes_read = file->read(buf, size * num);
  /*
  Serial.print("return ");
  Serial.println(bytes_read / size);
  */
  return bytes_read / size;
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