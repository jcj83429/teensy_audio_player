#if defined(__IMXRT1062__)
size_t my_xmp_read(void *user_data, void *buf, size_t size, size_t num);
int my_xmp_seek(void *user_data, long offset, int whence);
long my_xmp_tell(void *user_data);
int my_xmp_eof(void *user_data);
long my_xmp_size(void *user_data);
#endif
