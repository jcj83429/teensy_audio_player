# teensy_audio_player
SD card audio file player using teensy3.6, SdFat-beta library. Features:
 - MP3, AAC, FLAC (up to 192bit/24kHz), Opus. Output resolution is limited to 16bit
 - Digital volume control with ReplayGain support
 - UI with VFD and push buttons, and basic serial controls for testing
 - Spectrum, peak and RMS visualizations
 - Long file name and ExFAT support through SdFat-beta
 - Files are sorted by name
 - Directory support (up to 4 levels deep, 256 items per directory)

To build this project,
 - Delete play_sd_wav.cpp/h and play_sd_raw.cpp/h from the teensy audio library (arduino-1.8.10/hardware/teensy/avr/libraries/Audio). They require SD.h which conflicts with SdFat.
 - Use my patched Arduino-Teensy-Codec-lib which has the SD.h dependency removed and extra functionality (i.e. Opus) added. (https://github.com/jcj83429/Arduino-Teensy-Codec-lib)
 - Use my fork of OpenAudio_ArduinoLibrary with proper 32 bit output (https://github.com/jcj83429/OpenAudio_ArduinoLibrary)

For best decoding performance (needed to play 192/24 FLAC files smoothly), build with optimization set to Fastest + pure-code with LTO.

The PCB design is at https://github.com/jcj83429/teensy_audio_player_kicad
