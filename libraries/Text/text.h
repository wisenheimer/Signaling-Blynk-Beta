#ifndef myText_h
#define myText_h

#include "Arduino.h"
#include "fifo.h"

class TEXT
{
  public:
    fifo_t fifo;
    TEXT(unsigned int);
    ~TEXT();

    uint16_t filling();
    uint16_t free_space();
    uint16_t AddText(char*);
    uint16_t AddText_P(const char*);
    void AddChar(char);
    void AddInt(int);
    void AddFloat(float);
    void Clear();
    char* GetText();
    char GetChar();
    char Peek();
    uint16_t GetBytes(char*, unsigned int, bool);
    char GetByte(uint16_t index);
    
  private:
};

#endif
