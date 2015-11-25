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
#include "RequestTimer.hpp"

namespace FileManager
{
	const size_t maxPathLength = 100;
	typedef String<maxPathLength> Path;

	class FileSet
	{
	private:
		Path requestedPath;
		Path currentPath;
		RequestTimer timer;
		int which;
		const Event fileEvent;
		const Event upEvent;
		const char * array const popupTitle;
		int scrollOffset;
		bool IsInSubdir() const;
		
	public:
		FileSet(Event fe, Event fu, const char * array rootDir, const char * array title);
		void Display();
		void Reload(int whichList, const Path& dir);
		void RefreshPopup();
		void Scroll(int amount);
		void SetIndex(int index) { which = index; }
		int GetIndex() const { return which; }
		void SetPath(const char * array pPath);
		const char * array GetPath() { return currentPath.c_str(); }
		void RequestParentDir()
			pre(IsInSubdir());
		void RequestSubdir(const char * array dir);
		void RequestRootDir();
		void SetPending() { timer.SetPending(); }
		void StopTimer() { timer.Stop(); }
		bool ProcessTimer() { return timer.Process(); }
	};

	void BeginNewMessage();
	void EndReceivedMessage(bool displayingFileInfo);
	void BeginReceivingFiles();
	void ReceiveFile(const char * array data);
	void ReceiveDirectoryName(const char * array data);
	
	void DisplayFilesList();
	void DisplayMacrosList();
	void Scroll(int amount);
	
	void RequestFilesSubdir(const char * array dir);
	void RequestMacrosSubdir(const char * array dir);
	void RequestFilesParentDir();
	void RequestMacrosParentDir();
	void RequestFilesRootDir();
	void RequestMacrosRootDir();
	const char * array GetFilesDir();
	const char * array GetMacrosDir();

	void RefreshFilesList();
	bool ProcessTimers();
}

#endif /* FILEMANAGER_H_ */