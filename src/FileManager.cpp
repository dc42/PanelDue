/*
 * FileManager.cpp
 *
 * Created: 06/11/2015 10:52:13
 *  Author: David
 */ 

#include "FileManager.hpp"
#include "PanelDue.hpp"

namespace FileManager
{
	typedef Vector<char, 2048> FileList;			// we use a Vector instead of a String because we store multiple null-terminated strings in it
	typedef Vector<const char* array, 100> FileListIndex;

	static FileList fileLists[3];								// one for gcode file list, one for macro list, one for receiving new lists into
	static FileListIndex fileIndices[3];						// pointers into the individual filenames in the list

	static int newFileList = -1;								// which file list we received a new listing into
	static String<100> fileDirectoryName;
	static FileSet gcodeFilesList(evFile), macroFilesList(evMacro);
	static FileSet * null displayedFileSet = nullptr;

	// Return true if the second string is alphabetically greater then the first, case insensitive
	static bool StringGreaterThan(const char* a, const char* b)
	{
		return strcasecmp(a, b) > 0;
	}

	FileSet::FileSet(Event e) : which(-1), evt(e), scrollOffset(0)
	{
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
		
			// 4. Display the file list
			for (size_t i = 0; i < numDisplayedFiles; ++i)
			{
				TextButton *f = filenameButtons[i];
				if (i + scrollOffset < fileIndex.size())
				{
					const char *text = fileIndex[i + scrollOffset];
					f->SetText(text);
					f->SetEvent(evt, text);
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
			if (fileDirectoryName.isEmpty() || fileDirectoryName.equalsIgnoreCase("/gcodes") || fileDirectoryName.equalsIgnoreCase("0:/gcodes/"))
			{
				if (!displayingFileInfo)
				{
					gcodeFilesList.SetIndex(newFileList);
					gcodeFilesList.RefreshPopup();
					filesListTimer.Stop();
				}
			}
			else if (fileDirectoryName.equalsIgnoreCase("/macros"))
			{
				macroFilesList.SetIndex(newFileList);
				macroFilesList.RefreshPopup();
				macroListTimer.Stop();
			}
			newFileList = -1;
		}
	}

	void FileSet::Scroll(int amount)
	{
		scrollOffset += amount;
		RefreshPopup();
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

	void ReceiveFile(const char *data)
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

	void ReceiveDirectoryName(const char *data)
	{
		fileDirectoryName.copyFrom(data);
	}

	void DisplayFilesList()
	{
		gcodeFilesList.RefreshPopup();
		filePopupTitleField->SetValue("Files on SD card");
		mgr.SetPopup(fileListPopup, fileListPopupX, fileListPopupY);
		filesListTimer.SetPending();								// refresh the list of files
	}

	void DisplayMacrosList()
	{
		macroFilesList.RefreshPopup();
		filePopupTitleField->SetValue("Macros");
		mgr.SetPopup(fileListPopup, fileListPopupX, fileListPopupY);
		macroListTimer.SetPending();								// refresh the list of macros
	}		

	bool Scroll(int amount)
	{
		if (displayedFileSet != nullptr)
		{
			displayedFileSet->Scroll(amount);
			return true;
		}
		return false;
	}

}		// end namespace

// End
