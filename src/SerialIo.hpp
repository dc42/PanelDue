/*
 * SerialIo.hpp
 *
 * Created: 09/11/2014 09:20:46
 *  Author: David
 */ 


#ifndef SERIALIO_H_
#define SERIALIO_H_

namespace SerialIo
{
	void init();
	void putChar(char c);
	void checkInput();
}

#endif /* SERIALIO_H_ */