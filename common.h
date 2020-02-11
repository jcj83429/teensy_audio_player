static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}
