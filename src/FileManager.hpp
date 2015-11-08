/*
 * FileManager.h
 *
 * Created: 06/11/2015 10:52:38
 *  Author: David
 */ 

#ifndef FILEMANAGER_H_
#define FILEMANAGER_H_

#include "Configuration.hpp"
#include "Library/Vector.hpp"
#include "Fields.hpp"

namespace FileManager
{
	class FileSet
	{
		int which;
		const Event evt;
		String<100> path;
		int scrollOffset;
		
	public:
		FileSet(Event e);
		void RefreshPopup();
		void Scroll(int amount);
		void SetIndex(int index) { which = index; }
		int GetIndex() const { return which; }
	};

	void BeginNewMessage();
	void EndReceivedMessage(bool displayingFileInfo);
	void BeginReceivingFiles();
	void ReceiveFile(const char *data);
	void ReceiveDirectoryName(const char *data);
	
	void DisplayFilesList();
	void DisplayMacrosList();
	bool Scroll(int amount);
}

#endif /* FILEMANAGER_H_ */