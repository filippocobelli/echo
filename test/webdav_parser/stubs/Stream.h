#pragma once
// Host-test stub of the Arduino Stream interface used by WebDavParserStream.
#include "Print.h"

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
};
