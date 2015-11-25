/*
 * FileManager.cpp
 *
 * Created: 06/11/2015 10:52:13
 *  Author: David
 */ 

#include "FileManager.hpp"
#include "PanelDue.hpp"
#include <cctype>

namespace FileManager
{
	typedef Vector<char, 2048> FileList;						// we use a Vector instead of a String because we store multiple null-terminated strings in it
	typedef Vector<const char* array, 100> FileListIndex;

	const char * array filesRoot = "/gcodes";
	const char * array macrosRoot = "/macros";
	const uint32_t FileListRequestTimeout = 8000;				// file info request timeout in milliseconds

	static FileList fileLists[3];								// one for gcode file list, one for macro list, one for receiving new lists into
	static FileListIndex fileIndices[3];						// pointers into the individual filenames in the list

	static int newFileList = -1;								// which file list we received a new listing into
	static Path fileDirectoryName;
	static FileSet gcodeFilesList(evFile, evFilesUp, filesRoot, "Files on SD card");
	static FileSet macroFilesList(evMacro, evMacrosUp, macrosRoot, "Macros");
	static FileSet * null displayedFileSet = nullptr;
	
	// Return true if the second string is alphabetically greater then the first, case insensitive
	static bool StringGreaterThan(const char* a, const char* b)
	{
		return strcasecmp(a, b) > 0;
	}

	FileSet::FileSet(Event fe, Event fu, const char * array rootDir, const char * array title)
		: requestedPath(rootDir), currentPath(), timer(FileListRequestTimeout, "M20 S2 P", requestedPath.c_str()), which(-1), fileEvent(fe), upEvent(fu), popupTitle(title), scrollOffset(0)
	{
	}

	void FileSet::Display()
	{
		RefreshPopup();
		filePopupTitleField->SetValue(popupTitle);
		filesUpButton->SetEvent(upEvent, nullptr);
		mgr.SetPopup(fileListPopup, fileListPopupX, fileListPopupY);
		timer.SetPending();										// refresh the list of files
	}

	void FileSet::Reload(int whichList, const Path& dir)
	{
		SetIndex(whichList);
		SetPath(dir.c_str());
		RefreshPopup();
		StopTimer();
	}

	// Refresh the list of files or macros in the Files popup window
	void FileSet::RefreshPopup()
	{
		if (which >= 0)
		{
			FileListIndex& fileIndex = fileIndices[which];
		
			// 1. Sort the file list
			fileIndex.sort(StringGreaterThan);
		
			// 2. Make sure the scroll position is still sensible
			if (scrollOffset < 0)
			{
				scrollOffset = 0;
			}
			else if ((unsigned int)scrollOffset >= fileIndex.size())
			{
				scrollOffset = ((fileIndex.size() - 1)/numFileRows) * numFileRows;
			}
		
			// 3. Display the scroll buttons if needed
			mgr.Show(scrollFilesLeftButton, scrollOffset != 0);
			mgr.Show(scrollFilesRightButton, scrollOffset + (numFileRows * numFileColumns) < fileIndex.size());
			mgr.Show(filesUpButton, IsInSubdir());
		
			// 4. Display the file list
			for (size_t i = 0; i < numDisplayedFiles; ++i)
			{
				TextButton *f = filenameButtons[i];
				if (i + scrollOffset < fileIndex.size())
				{
					const char *text = fileIndex[i + scrollOffset];
					f->SetText(text);
					f->SetEvent(fileEvent, text);
					mgr.Show(f, true);
				}
				else
				{
					f->SetText("");
					mgr.Show(f, false);
				}
			}
			displayedFileSet = this;
		}
		else
		{
			mgr.Show(scrollFilesLeftButton, false);
			mgr.Show(scrollFilesRightButton, false);
			for (size_t i = 0; i < numDisplayedFiles; ++i)
			{
				mgr.Show(filenameButtons[i], false);
			}
		}
	}

	void FileSet::Scroll(int amount)
	{
		scrollOffset += amount;
		RefreshPopup();
	}
	
	void FileSet::SetPath(const char * array pPath)
	{
		currentPath.copyFrom(pPath);
	}
	
	// Return true if the path has more than one directory component
	bool FileSet::IsInSubdir() const
	{
		// Find the start of the first component of the path
		size_t start = (currentPath.size() >= 3 && isdigit(currentPath[0]) && currentPath[1] == ':' && currentPath[2] == '/') ? 3
					: (currentPath.size() >= 1 && currentPath[0] == '/') ? 1
					: 0;
		size_t end = currentPath.size();
		if (end != 0 && currentPath[end - 1] == '/')
		{
			--end;			// remove trailing '/' if there is one
		}
		while (end != 0 && currentPath[end] != '/')
		{
			--end;
		}
		return (end > start);
	}

	// Request the parent path
	void FileSet::RequestParentDir()
	{
		size_t end = currentPath.size();
		// Skip any trailing '/'
		if (end != 0 && currentPath[end - 1] == '/')
		{
			--end;
		}
		// Find the last '/'
		while (end != 0)
		{
			--end;
			if (currentPath[end] == '/')
			{
				break;
			}
		}
		requestedPath.clear();
		for (size_t i = 0; i < end; ++i)
		{
			requestedPath.add(currentPath[i]);
		}
		timer.SetPending();
	}

	// Build a subdirectory of the current path
	void FileSet::RequestSubdir(const char * array dir)
	{
		requestedPath.copyFrom(currentPath);
		if (requestedPath.size() == 0 || requestedPath[requestedPath.size() - 1] != '/')
		{
			requestedPath.add('/');
		}
		requestedPath.catFrom(dir);
		timer.SetPending();
	}
	
	void BeginNewMessage()
	{
		fileDirectoryName.clear();
		newFileList = -1;
	}

	void EndReceivedMessage(bool displayingFileInfo)
	{
		if (newFileList >= 0)
		{
			// We received a new file list
			// Find the first component of the path
			size_t i = (fileDirectoryName.size() >= 3 && isdigit(fileDirectoryName[0]) && fileDirectoryName[1] == ':' && fileDirectoryName[2] == '/') ? 3
						: (fileDirectoryName.size() >= 1 && fileDirectoryName[0] == '/') ? 1
						: 0;
			String<10> temp;
			while (i < fileDirectoryName.size() && fileDirectoryName[i] != '/' && !temp.full())
			{
				temp.add(fileDirectoryName[i++]);
			}
			
			// Depending on the first component of the path, refresh the Files or Macros list
			if (temp.size() == 0 || temp.equalsIgnoreCase("gcodes"))
			{
				if (!displayingFileInfo)
				{
					gcodeFilesList.Reload(newFileList, fileDirectoryName);
				}
			}
			else if (temp.equalsIgnoreCase("macros"))
			{
				macroFilesList.Reload(newFileList, fileDirectoryName);
			}
			newFileList = -1;
		}
	}

	void BeginReceivingFiles()
	{
		// Find a free file list and index to receive the filenames into
		newFileList = 0;
		while (newFileList == gcodeFilesList.GetIndex() || newFileList == macroFilesList.GetIndex())
		{
			++newFileList;
		}
					
		_ecv_assert(0 <= newFileList && newFileList < 3);
		fileLists[newFileList].clear();
		fileIndices[newFileList].clear();
	}

	void ReceiveFile(const char * array data)
	{
		if (newFileList >= 0)
		{
			FileList& fileList = fileLists[newFileList];
			FileListIndex& fileIndex = fileIndices[newFileList];
			size_t len = strlen(data) + 1;		// we are going to copy the null terminator as well
			if (len + fileList.size() < fileList.capacity() && fileIndex.size() < fileIndex.capacity())
			{
				fileIndex.add(fileList.c_ptr() + fileList.size());
				fileList.add(data, len);
			}
		}
	}

	void ReceiveDirectoryName(const char * array data)
	{
		fileDirectoryName.copyFrom(data);
	}

	void DisplayFilesList()
	{
		gcodeFilesList.Display();
	}

	void DisplayMacrosList()
	{
		macroFilesList.Display();
	}		

	void Scroll(int amount)
	{
		if (displayedFileSet != nullptr)
		{
			displayedFileSet->Scroll(amount);
		}
	}

	void RequestFilesSubdir(const char * array dir)
	{
		gcodeFilesList.RequestSubdir(dir);
	}
	
	void RequestMacrosSubdir(const char * array dir)
	{
		macroFilesList.RequestSubdir(dir);
	}

	void RequestFilesParentDir()
	{
		gcodeFilesList.RequestParentDir();
	}

	void RequestMacrosParentDir()
	{
		macroFilesList.RequestParentDir();
	}

	void RequestFilesRootDir()
	{
		gcodeFilesList.RequestRootDir();
	}

	void RequestMacrosRootDir()
	{
		macroFilesList.RequestRootDir();
	}

	const char * array GetFilesDir()
	{
		return gcodeFilesList.GetPath();
	}

	const char * array GetMacrosDir()
	{
		return macroFilesList.GetPath();
	}

	void RefreshFilesList()
	{
		gcodeFilesList.SetPending();
	}

	bool ProcessTimers()
	{
		bool done = macroFilesList.ProcessTimer();
		if (!done)
		{
			done = gcodeFilesList.ProcessTimer();
		}
		return done;
	}
}		// end namespace

// End
