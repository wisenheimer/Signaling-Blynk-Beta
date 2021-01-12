
const char* audio_files[] = {
	"/pno-cs.mp3",
	"/sim800_detected.mp3",
	"/modem_on.mp3",
	"/guard_on.mp3",
	"/guard_off.mp3",
};
/*

BLYNK_WRITE(MUSIC_PIN) {
  
	Serial.printf("Item %d selected\n", param.asInt());
  int count = sizeof(audio_files)/sizeof(char*);

  if(count >= param.asInt())
  {
    mp3_file_load(audio_files[param.asInt()-1]);
  }
	else mp3_file_stop();
}
*/