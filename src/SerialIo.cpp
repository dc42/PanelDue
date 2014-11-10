/*
 * SerialIo.cpp
 *
 * Created: 09/11/2014 09:20:26
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "SerialIo.hpp"
#include "Vector.hpp"

typedef String<20> FieldId;
typedef String<50> StringValue;

extern void processIntData(const char * array id, int index, int data);
extern void processFloatData(const char * array id, int index, float data);
extern void processStringData(const char * array id, int index, const char * array data);

namespace SerialIo
{
	// Initialize the serial I/O subsystem
	void init()
	{
		pmc_enable_periph_clk(ID_UART1);
		pio_configure(PIOB, PIO_PERIPH_A, PIO_PB2 | PIO_PB3, 0);	// enable UART 1 pins
	
		sam_uart_opt uartOptions;
		uartOptions.ul_mck = sysclk_get_main_hz()/2;	// master clock is PLL clock divided by 2
		uartOptions.ul_baudrate = 115200;
		uartOptions.ul_mode = US_MR_PAR_NO;				// mode = normal, no parity
		uart_init(UART1, &uartOptions);
		irq_register_handler(UART1_IRQn, 5);
		uart_enable_interrupt(UART1, UART_IER_RXRDY | UART_IER_OVRE | UART_IER_FRAME);
	}
	
	// Send a character to the printer.
	// A typical command string is only about 12 characters long, which at 115200 baud takes just over 1ms to send.
	// So there is no particular reason to use interrupts, and by so doing so we avoid having to handle buffer full situations.
	void putChar(char c)
	{
		while(uart_write(UART1, c) != 0) { }
	}
	
	// Receive data processing
	const size_t rxBufsize = 1024;
	static char rxBuffer[rxBufsize];
	static volatile size_t nextIn = 0, nextOut = 0;
	static bool inError = false;
	
	// Enumeration to represent the json parsing state.
	// We don't allow nested objects or nested arrays, so we don't need a state stack.
	// An additional variable elementCount is 0 if we are not in an array, else the number of elements we have found (including the current one)
	enum JsonState 
	{
		jsBegin,			// initial state, expecting '{'
		jsBra,				// just had '{' so expecting a quoted ID
		jsId,				// expecting an identifier, or in the middle of one
		jsHadId,			// had a quoted identifier, expecting ':'
		jsVal,				// had ':', expecting value
		jsStringVal,		// had '"' and expecting or in a string value
		jsStringEscape,		// just had backslash in a string
		jsIntVal,			// receiving an integer value
		jsNegIntVal,		// had '-' so expecting a integer value
		jsFracVal,			// receiving a fractional value
		jsEndVal,			// had the end of a string or array value, expecting comma or ] or }
		jsError				// something went wrong
	};
	
	JsonState state = jsBegin;
	
	FieldId id;
	StringValue stringVal;
	unsigned int intVal;
	unsigned int fracVal, fracDivisor;
	bool negative;
	int arrayElems = -1;
	
	void processStringValue()
	{
		processStringData(id.c_str(), arrayElems, stringVal.c_str());
	}
	
	void processIntValue()
	{
		int val = (negative) ? -(int)intVal : (int)intVal;
		processIntData(id.c_str(), arrayElems, val);
	}
	
	void processFloatValue()
	{
		float val = (float)fracVal/(float)fracDivisor + (float)intVal;
		if (negative)
		{
			val = -val;
		}
		processFloatData(id.c_str(), arrayElems, val);
	}
	
	void checkInput()
	{
		if (nextIn != nextOut)
		{
			char c = rxBuffer[nextOut];
			nextOut = (nextOut + 1) % rxBufsize;
			if (c == '\n')
			{
				state = jsBegin;		// abandon current parse (if any) and start again
			}
			else
			{
				switch(state)
				{
				case jsBegin:			// initial state, expecting '{'
					if (c == '{')
					{
						state = jsBra;
					}
					break;

				case jsBra:				// just had '{' so expecting a quoted ID
					switch (c)
					{
					case ' ':
						break;
					case '"':
						id.clear();
						state = jsId;
						break;
					case '}':
						state = jsBegin;
						break;
					default:
						state = jsError;
						break;
					}
					break;
						
				case jsId:				// expecting an identifier, or in the middle of one
					switch (c)
					{
					case '"':
						state = jsHadId;
						break;
					case '\n':
						state = jsBegin;
						break;
					default:
						if (c >= ' ' && !id.full())
						{
							id.add(c);
						}
						else
						{
							state = jsError;
						}
						break;
					}
					break;
	
				case jsHadId:			// had a quoted identifier, expecting ':'
					switch(c)
					{
					case ':':
						arrayElems = -1;
						state = jsVal;
						break;
					case ' ':
						break;
					default:
						state = jsError;
						break;
					}
					break;

				case jsVal:				// had ':', expecting value
					switch(c)
					{
					case ' ':
						break;
					case '"':
						stringVal.clear();
						state = jsStringVal;
						break;
					case '[':
						if (arrayElems == -1)
						{
							arrayElems = 0;
						}
						else
						{
							state = jsError;
						}
						break;
					case '-':
						state = jsNegIntVal;
						break;
					default:
						if (c >= '0' && c <= '9')
						{
							negative = false;
							intVal = c - '0';
							state = jsIntVal;
							break;
						}
						else
						{
							state = jsError;
						}
					}
					break;
					
				case jsStringVal:		// had '"' and expecting or in a string value
					switch (c)
					{
					case '"':
						processStringValue();
						state = jsEndVal;
						break;
					case '\\':
						if (!stringVal.full())
						{
							state = jsStringEscape;
						}
						else
						{
							state = jsError;
						}
						break;
					default:
						if (c >= ' ' && !stringVal.full())
						{
							stringVal.add(c);
						}
						else
						{
							state = jsError;
						}
						break;
					}
					break;

				case jsStringEscape:	// just had backslash in a string
					switch (c)
					{
					case '"':
					case '\\':
						stringVal.add(c);
						break;
					case 'b':
						stringVal.add('\b');
						break;
					case 'f':
						stringVal.add('\f');
						break;
					case 'n':
						stringVal.add('\n');
						break;
					case 'r':
						stringVal.add('\r');
						break;
					case 't':
						stringVal.add('\t');
						break;
					// we don't handle '\uxxxx'
					default:
						state = jsEndVal;
						break;
					}
					break;

				case jsNegIntVal:		// had '-' so expecting a integer value
					if (c >= '0' && c <= '9')
					{
						negative = true;
						intVal = c - '0';
						state = jsIntVal;
					}
					else
					{
						state = jsError;
					}
					break;
					
				case jsIntVal:			// receiving an integer value
					switch(c)
					{
					case '.':
						fracVal = 0;
						fracDivisor = 1;
						state = jsFracVal;
						break;
					case ',':
						processIntValue();
						if (arrayElems >= 0)
						{
							++arrayElems;
							state = jsVal;
						}
						else
						{
							state = jsId;
						}
						break;					
					case ']':
						if (arrayElems >= 0)
						{
							processIntValue();
							arrayElems = -1;
							state = jsEndVal;
						}
						else
						{
							state = jsError;
						}
						break;
					case '}':
						if (arrayElems == -1)
						{
							processIntValue();
							state = jsBegin;
						}
						else
						{
							state = jsError;
						}
						break;
					default:
						if (c >= '0' && c <= '9')
						{
							intVal = (10 * intVal) + (c - '0');
						}
						else
						{
							state = jsError;
						}
						break;
					}
					break;

				case jsFracVal:			// receiving a fractional value
					switch(c)
					{
					case ',':
						processFloatValue();
						if (arrayElems >= 0)
						{
							++arrayElems;
							state = jsVal;
						}
						else
						{
							state = jsId;
						}
						break;
					case ']':
						if (arrayElems >= 0)
						{
							processFloatValue();
							arrayElems = -1;
							state = jsEndVal;
						}
						else
						{
							state = jsError;
						}
						break;
					case '}':
						if (arrayElems == -1)
						{
							processFloatValue();
							state = jsBegin;
						}
						else
						{
							state = jsError;
						}
						break;
					default:
						if (c >= '0' && c <= '9')
						{
							fracVal = (10 * fracVal) + (c - '0');
							fracDivisor *= 10;
						}
						else
						{
							state = jsError;
						}
						break;
					}
					break;

				case jsEndVal:			// had the end of a string or array value, expecting comma or ] or }
					switch (c)
					{
					case ',':
						if (arrayElems >= 0)
						{
							++arrayElems;
							state = jsVal;
						}
						else
						{
							state = jsId;
						}
						break;
					case ']':
						if (arrayElems >= 0)
						{
							arrayElems = -1;
							state = jsEndVal;
						}
						else
						{
							state = jsError;
						}
						break;
					case '}':
						if (arrayElems == -1)
						{
							state = jsBegin;
						}
						else
						{
							state = jsError;
						}
						break;
					default:
						break;
					}
					break;

				case jsError:
					break;
				}
			}
		}
	}
	
	// Called by the ISR to store a received character.
	// If the buffer is full, we wait for the next end-of-line.
	void receiveChar(char c)
	{
		if (c == '\n')
		{
			inError = false;
		}
		if (!inError)
		{
			size_t temp = (nextIn + 1) % rxBufsize;
			if (temp == nextOut)
			{
				inError = true;
			}
			else
			{
				rxBuffer[nextIn] = c;
				nextIn = temp;
			}
		}
	}
	
	// Called by the ISR to signify an error. We wait for the next end of line.
	void receiveError()
	{
		inError = true;
	}
}

extern "C" {

	void UART1_Handler()
	{
		uint32_t status = UART1->UART_SR;

		// Did we receive data ?
		if ((status & UART_SR_RXRDY) == UART_SR_RXRDY)
		{
			SerialIo::receiveChar(UART1->UART_RHR);
		}

		// Acknowledge errors
		if (status & (UART_SR_OVRE | UART_SR_FRAME))
		{
			UART1->UART_CR |= UART_CR_RSTSTA;
			SerialIo::receiveError();
		}	
	}
	
};

// End
