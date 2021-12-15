//copy from bilibili BV1ZW411y7zk by jiabs at 2021/12/10

#ifndef DEBUG_NEW_H_
#define DEBUG_NEW_H_
#ifndef NDEBUG
#include "TracerNew.h"


#define new new(__FILE__,__LINE__)

#endif//!NDEBUG

#endif//!DEBUG_NEW_H