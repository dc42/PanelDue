/*
 * MessageLog.cpp
 *
 * Created: 15/11/2015 10:54:42
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "Configuration.hpp"
#include "Library/Vector.hpp"
#include "MessageLog.hpp"
#include "Fields.hpp"
#include "Hardware/SysTick.hpp"
#include "Library/Misc.hpp"

namespace MessageLog
{
	const unsigned int maxMessageChars = 100;

	struct Message
	{
		static const size_t rttLen = 5;					// number of chars we print for the message age
		uint32_t receivedTime;
		char receivedTimeText[rttLen];					// 5 characters plus null terminator
		char msg[maxMessageChars];
	};

	static Message messages[numMessageRows + 1];		// one extra slot for receiving new messages into
	static unsigned int messageStartRow = 0;			// the row number at the top
	static unsigned int newMessageStartRow = 0;			// the row number that we put a new message in

	void Init()
	{
		// Clear the message log
		for (size_t i = 0; i <= numMessageRows; ++i)	// note we have numMessageRows+1 message slots
		{
			messages[i].receivedTime = 0;
			messages[i].msg[0] = 0;
		}
		
		UpdateMessages(true);
	}
	
	// Update the messages on the message tab. If 'all' is true we do the times and the text, else we just do the times.
	void UpdateMessages(bool all)
	{
		size_t index = messageStartRow;
		for (size_t i = 0; i < numMessageRows; ++i)
		{
			Message *m = &messages[index];
			uint32_t tim = m->receivedTime;
			char* p = m->receivedTimeText;
			if (tim == 0)
			{
				p[0] = 0;
			}
			else
			{
				uint32_t age = (SystemTick::GetTickCount() - tim)/1000;	// age of message in seconds
				if (age < 10 * 60)
				{
					snprintf(p, Message::rttLen, "%lum%02lu", age/60, age%60);
				}
				else
				{
					age /= 60;		// convert to minutes
					if (age < 60)
					{
						snprintf(p, Message::rttLen, "%lum", age);
					}
					else if (age < 10 * 60)
					{
						snprintf(p, Message::rttLen, "%luh%02lu", age/60, age%60);
					}
					else
					{
						age /= 60;	// convert to hours
						if (age < 10)
						{
							snprintf(p, Message::rttLen, "%luh", age);
						}
						else if (age < 24 + 10)
						{
							snprintf(p, Message::rttLen, "%lud%02lu", age/24, age%24);
						}
						else
						{
							snprintf(p, Message::rttLen, "%lud", age/24);
						}
					}
				}
				messageTimeFields[i]->SetValue(p);
			}
			if (all)
			{
				messageTextFields[i]->SetValue(m->msg);
			}
			index = (index + 1) % (numMessageRows + 1);
		}
	}

	// Add a message to the end of the list. It will be just off the visible part until we scroll it in.
	void AppendMessage(const char* data)
	{
		newMessageStartRow = (messageStartRow + 1) % (numMessageRows + 1);
		size_t msgRow = (newMessageStartRow + numMessageRows - 1) % (numMessageRows + 1);
		safeStrncpy(messages[msgRow].msg, data, maxMessageChars);
		messages[msgRow].receivedTime = SystemTick::GetTickCount();
	}

	// If there is a new message, scroll it in
	void DisplayNewMessage()
	{
		if (newMessageStartRow != messageStartRow)
		{
			messageStartRow = newMessageStartRow;
			UpdateMessages(true);
		}
	}
	
	// This is called when we receive a new response from the host, which may or may not include a new message for the log
	void BeginNewMessage()
	{
		newMessageStartRow = messageStartRow;
	}

}			// end namespace

// End

