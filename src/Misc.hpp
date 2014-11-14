/*
 * Misc.h
 *
 * Created: 14/11/2014 19:56:03
 *  Author: David
 */ 


#ifndef MISC_H_
#define MISC_H_

#include <cstddef>

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

void safeStrncpy(char* array dst, const char* array src, size_t n)
pre(n != 0; _ecv_isNullTerminated(src); dst.upb >= n);

#endif /* MISC_H_ */