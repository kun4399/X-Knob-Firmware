#pragma once
#include <stddef.h>
#include <stdint.h>
class Preferences {
public:
 bool begin(const char*, bool=false){return true;} void end(){}
 size_t getBytesLength(const char*){return 0;}
 size_t getBytes(const char*,void*,size_t){return 0;}
 size_t putBytes(const char*,const void*,size_t n){return n;}
 uint8_t getUChar(const char*,uint8_t d=0){return d;}
 size_t putUChar(const char*,uint8_t){return 1;}
};
