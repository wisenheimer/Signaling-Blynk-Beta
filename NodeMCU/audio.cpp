 /**************************************************************
 * https://github.com/earlephilhower/ESP8266Audio
 * // To run, set your ESP8266 build to 160MHz, and include a SPIFFS of 512KB or greater.
 * // Use the "Tools->ESP8266/ESP32 Sketch Data Upload" menu to write the MP3 to SPIFFS
 * // Then upload the sketch normally.  
 * // pno_cs from https://ccrma.stanford.edu/~jos/pasp/Sound_Examples.html
 *************************************************************/

#include "audio.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#include "main_type.h"

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2SNoDAC *out;
AudioFileSourceID3 *id3;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  (void)cbData;
  Serial.printf("ID3 callback for: %s = '", type);

  if (isUnicode) {
    string += 2;
  }
  
  while (*string) {
    char a = *(string++);
    if (isUnicode) {
      string++;
    }
    Serial.printf("%c", a);
  }
  Serial.printf("'\n");
  Serial.flush();
}

// инициализация проигрывание mp3 на пине RX
void mp3_init()
{
	if (SPIFFS.begin()) {
    	DEBUG_PRINTLN(F("mounted file system"));

    	Dir dir = SPIFFS.openDir("/");
    	while (dir.next()) {
    		DEBUG_PRINT(dir.fileName());
    		File f = dir.openFile("r");
    		DEBUG_PRINTLN(f.size());
    		f.close();
    	}    	
    } else {
    	DEBUG_PRINTLN(F("failed to mount FS"));
  	}

	audioLogger = &Serial;
	out = new AudioOutputI2SNoDAC();
	mp3 = new AudioGeneratorMP3();
}

void mp3_file_load(const char* filename)
{
	if (SPIFFS.exists(filename)) {
      //file exists, reading and loading
      // инициализация проигрывание mp3 на пине RX
 		DEBUG_PRINT(F("Sample "));
		DEBUG_PRINT(filename);
		DEBUG_PRINTLN(F(" playback begins..."));
		DEBUG_PRINTF("file=%d, id3=%d\n", file, id3);

		mp3_file_stop();

		if((file = new AudioFileSourceSPIFFS(filename)) == NULL)
		{
			DEBUG_PRINTLN(F("Exit"));
		}
		else DEBUG_PRINTF("file=%d\n", file);

		if((id3 = new AudioFileSourceID3(file)) == NULL)
		{
			DEBUG_PRINTLN(F("Exit2"));
		}
		else DEBUG_PRINTF("id3=%d\n", id3);

		id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
		mp3->begin(id3, out);
	}
	else DEBUG_PRINTF("Error: file %s not found\n", filename);
}

void mp3_run()
{
	if (mp3->isRunning())
	{
		if (!mp3->loop())
			mp3_file_stop();
	}
}

void mp3_file_stop()
{
	if (mp3->isRunning())
	{
		//while(!mp3->loop());
		DEBUG_PRINTLN(F("Sample MP3 stop..."));
			
		mp3->stop();

		delete file;
		delete id3;
	}
}

