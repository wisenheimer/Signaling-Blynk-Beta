#include "text.h"


TEXT::TEXT(unsigned int size)
{
  fifo = new_fifo(size);
  memset(fifo->buffer, 0, fifo->size);
}

TEXT::~TEXT()
{
  delete_fifo(fifo);
}

uint16_t TEXT::AddText(char* str)
{
  return fifo_write(fifo, str, strlen(str));
}

uint16_t TEXT::AddText_P(const char* str)
{
  char ch;
  uint16_t i = 0;

  while((ch = (char)pgm_read_byte(str+i)))
  {
    AddChar(ch);
    i++;
  }

  return i;
}

void TEXT::AddChar(char ch)
{
  if(fifo->filling < fifo->size) FIFO_PUT_BYTE(fifo, ch);
}

char TEXT::GetChar()
{
  char ch = 0;

  FIFO_GET_BYTE(fifo, ch);  
  
  return ch;
}

uint16_t TEXT::GetBytes(char* buf, unsigned int len, bool flag_copy)
{
  return fifo_read(fifo, buf, len, flag_copy);
}

void TEXT::AddInt(int i)
{
  char buf[17];
  AddText(itoa(i,buf,10));
}

void TEXT::AddFloat(float value)
{
  int i;
  AddInt(value);
  AddChar('.');
  i = value*100;
  AddInt(abs(i%100));  
}

uint16_t TEXT::filling()
{
  return FIFO_FILLING(fifo);
}

uint16_t TEXT::free_space()
{
  return FIFO_SIZE(fifo) - FIFO_FILLING(fifo);
}

void TEXT::Clear()
{
  FIFO_CLEAR(fifo);
  memset(fifo->buffer, 0, fifo->size);
}

char* TEXT::GetText()
{
  return (char*)(fifo)->buffer+(fifo)->head;
  //return (char*)fifo->buffer+10;
}

char TEXT::Peek()
{
  unsigned int tail = fifo->tail;
  if(!tail) tail = fifo->size;
  else tail--;
  return ((char*)fifo->buffer)[tail];
}

char TEXT::GetByte(uint16_t index)
{
  if(index >= fifo->filling) return 0;
  uint16_t addr = (fifo->head + index) % fifo->size;
  return ((char*)fifo->buffer)[addr];
}