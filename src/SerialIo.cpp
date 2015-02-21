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
#include "PanelDue.hpp"

namespace SerialIo
{
	// Initialize the serial I/O subsystem, or re-initialize it with a new baud rate
	void Init(uint32_t baudRate)
	{
		uart_disable_interrupt(UART1, 0xFFFFFFFF);
		pio_configure(PIOB, PIO_PERIPH_A, PIO_PB2 | PIO_PB3, 0);	// enable UART 1 pins
	
		sam_uart_opt uartOptions;
		uartOptions.ul_mck = sysclk_get_main_hz()/2;	// master clock is PLL clock divided by 2
		uartOptions.ul_baudrate = baudRate;
		uartOptions.ul_mode = US_MR_PAR_NO;				// mode = normal, no parity
		uart_init(UART1, &uartOptions);
		irq_register_handler(UART1_IRQn, 5);
		uart_enable_interrupt(UART1, UART_IER_RXRDY | UART_IER_OVRE | UART_IER_FRAME);
	}
	
	uint16_t numChars = 0;
	uint8_t checksum = 0;
	
	// Send a character to the 3D printer.
	// A typical command string is only about 12 characters long, which at 115200 baud takes just over 1ms to send.
	// So there is no particular reason to use interrupts, and by so doing so we avoid having to handle buffer full situations.
	void RawSendChar(char c)
	{
		while(uart_write(UART1, c) != 0) { }
	}
	
	void SendCharAndChecksum(char c)
	{
		checksum ^= c;
		RawSendChar(c);
		++numChars;	
	}

	void SendChar(char c)
	{
		if (c == '\n')
		{
			if (numChars != 0)
			{
				// Send the checksum
				RawSendChar('*');
				char digit0 = checksum % 10 + '0';
				checksum /= 10;
				char digit1 = checksum % 10 + '0';
				checksum /= 10;
				if (checksum != 0)
				{
					RawSendChar(checksum + '0');
				}
				RawSendChar(digit1);
				RawSendChar(digit0);
			}
			RawSendChar(c);
			numChars = 0;
		}
		else
		{
			if (numChars == 0)
			{
				checksum = 0;
				// Send a dummy line number
				SendCharAndChecksum('N');
				SendCharAndChecksum('0');
				SendCharAndChecksum(' ');
			}
			SendCharAndChecksum(c);
		}
	}
	
	void SendString(const char* array s)
	{
		while (*s != 0)
		{
			SendChar(*s++);
		}
	}

	void SendInt(int i)
	decrease(i < 0; i)
	{
		if (i < 0)
		{
			SendChar('-');
			i = -i;
		}
		if (i >= 10)
		{
			SendInt(i/10);
			i %= 10;
		}
		SendChar((char)((char)i + '0'));
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
		jsExpectId,			// just had '{' so expecting a quoted ID
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
	
	String<20> fieldId;
	String<100> fieldVal;
	int arrayElems = -1;
	
	static void ProcessField()
	{
		ProcessReceivedValue(fieldId.c_str(), fieldVal.c_str(), arrayElems);
		fieldVal.clear();
	}
	
	void CheckInput()
	{
		while (nextIn != nextOut)
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
						StartReceivedMessage();
						state = jsExpectId;
						fieldVal.clear();
					}
					break;

				case jsExpectId:		// expecting a quoted ID
					switch (c)
					{
					case ' ':
						break;
					case '"':
						fieldId.clear();
						state = jsId;
						break;
					case '}':
						EndReceivedMessage();
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
					default:
						if (c >= ' ' && !fieldId.full())
						{
							fieldId.add(c);
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
						fieldVal.add(c);
						state = jsNegIntVal;
						break;
					default:
						if (c >= '0' && c <= '9')
						{
							fieldVal.add(c);
							state = jsIntVal;
							break;
						}
						else
						{
							state = jsError;
						}
					}
					break;
					
				case jsStringVal:		// just had '"' and expecting a string value
					switch (c)
					{
					case '"':
						ProcessField();
						state = jsEndVal;
						break;
					case '\\':
						state = jsStringEscape;
						break;
					default:
						if (c < ' ')
						{
							state = jsError;
						}
						else if (!fieldVal.full())
						{
							fieldVal.add(c);
						}
						break;
					}
					break;

				case jsStringEscape:	// just had backslash in a string
					if (!fieldVal.full())
					{
						switch (c)
						{
						case '"':
						case '\\':
							fieldVal.add(c);
							break;
						case 'n':
						case 't':
							fieldVal.add(' ');		// replace newline and tab by space
							break;
						case 'b':
						case 'f':
						case 'r':
						default:
							break;
						}
					}
					state = jsStringVal;
					break;

				case jsNegIntVal:		// had '-' so expecting a integer value
					if (c >= '0' && c <= '9')
					{
						fieldVal.add(c);
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
						fieldVal.add(c);
						state = jsFracVal;
						break;
					case ',':
						ProcessField();
						if (arrayElems >= 0)
						{
							++arrayElems;
							state = jsVal;
						}
						else
						{
							state = jsExpectId;
						}
						break;					
					case ']':
						if (arrayElems >= 0)
						{
							ProcessField();
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
							ProcessField();
							EndReceivedMessage();
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
							fieldVal.add(c);
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
						ProcessField();
						if (arrayElems >= 0)
						{
							++arrayElems;
							state = jsVal;
						}
						else
						{
							state = jsExpectId;
						}
						break;
					case ']':
						if (arrayElems >= 0)
						{
							ProcessField();
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
							ProcessField();
							EndReceivedMessage();
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
							fieldVal.add(c);
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
							state = jsExpectId;
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
							EndReceivedMessage();
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
