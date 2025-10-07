#include <stdio.h>
#include <windows.h>
#include "Types.h"
#include "Misc.h"
#include "Dir.h"

bool Dir_CompareFilenames(char *file1, char *file2) //Compare filenames ignoring case. Treating \ and / as the same (TODO: Move this to string.cpp?)
{
	int i = 0;
	char a, b;
	while(1)
	{
		if(file1[i] == 0 || file2[i] == 0)
		{
			if(file1[i] == 0 && file2[i] == 0)
				break;
			else
				return 0;
		}
		
		a = tolower(file1[i]);
		b = tolower(file2[i]);
		if( a == b
		|| ( (a == '\\' || a == '/')  && (b == '\\' || b == '/') )
			)
		{
			i++;
			continue;
		}
		return 0;
	}
	if(i == 0)
		return 0;
	return 1;
}

static void Dir_IncreaseQueSize(dirContext_s *context)
{
	context->queueCurrentSize *= 2;
	context->que = (dir_queue_s *) realloc(context->que, sizeof(dir_queue_s) * context->queueCurrentSize); 
	context->alphaque = (int *) realloc(context->alphaque, sizeof(int) * context->queueCurrentSize); 
	for(unsigned int i = context->queueCurrentSize - DIRQUE_INCREASESIZEBY; i < context->queueCurrentSize; i++)
		context->que[i].active = 0;
}

bool Dir_DirExists(dirContext_s *context, char *str)
{
	for(unsigned int i = 0; i < context->highestDirNum; i++)
	{
		if(context->que[i].active && context->que[i].dir && Dir_CompareFilenames(str, context->que[i].fileName))
			return 1;
	}
	return 0;
}

bool Dir_FileExists(dirContext_s *context, char *str)
{
	for(unsigned int i = 0; i < context->highestDirNum; i++)
	{
		if(context->que[i].active && !context->que[i].dir && Dir_CompareFilenames(str, context->que[i].fileName))
			return 1;
	}
	return 0;
}

int Dir_FindActiveDirQueue(dirContext_s *context, s32 searchIdx = 0)
{
	s32 searchedNum = 0;
	if(searchIdx < 0) searchIdx = 0;
	while(searchedNum < context->highestDirNum)
	{
		if(searchIdx >= context->highestDirNum) searchIdx = 0;
		if(context->que[searchIdx].active && context->que[searchIdx].dirSearch)
			return searchIdx;
		searchIdx++;
		searchedNum++;
	}
	return -1;
}

int Dir_FindInactiveQueue(dirContext_s *context)
{
	context->queCur++;
	if(context->queCur >= context->queueCurrentSize)
		Dir_IncreaseQueSize(context);
	if(context->highestDirNum < context->queCur + 1)
		context->highestDirNum = context->queCur + 1;
	return context->queCur;
}

void Dir_AddToQueue(dirContext_s *context, char *startDir, char *searchPath, char *dirName, bool dir, unsigned long long lastModified, unsigned long long fileSize, char *fileNameMustContain)
{
	if( (dirName[0] == '.') || (dirName[0] == '.' && dirName[1] == '.') )
		return; //Skip "." and ".."

	int slot = Dir_FindInactiveQueue(context), i = 0, j = 0;
	bool startCopying = 0;
	
	if(slot == -1)
		return;

	context->que[slot].archive = -1;
	context->que[slot].lastModified = lastModified;
	context->que[slot].fileSize = fileSize;
	context->que[slot].dir = dir;
	memset(context->que[slot].fileName, 0, FILENAME_SIZE_BIG);

	if(dir && context->searchSubDirs) //Set this to true if we're searching through subdirectories
		context->que[slot].dirSearch = 1;
	else 
		context->que[slot].dirSearch = 0;

	int posOfLastDot = 0;
	int posOfLastSlash = -1;
	
	//Copy search path to string
	int length = (int) strlen(startDir);
	char *copyStr = &searchPath[length + 1];
	while(copyStr[j] != 0)
	{
		if(copyStr[j] != '*')
		{
			context->que[slot].fileName[i] = copyStr[j];
			if(context->que[slot].fileName[i] == '\\' || context->que[slot].fileName[i] == '/')
				posOfLastSlash = i;
			i++;
		}
		j++;
	}

	j = 0;
	
	//Copy dirName to string
	while(dirName[j] != 0)
	{
		context->que[slot].fileName[i] = dirName[j];
		if(context->que[slot].fileName[i] == '.')
			posOfLastDot = i;
		if(context->que[slot].fileName[i] == '\\' || context->que[slot].fileName[i] == '/')
			posOfLastSlash = i;
		i++;
		j++;
	}
	strcpy_s(context->que[slot].fileNameWithoutExt, FILENAME_SIZE_BIG, context->que[slot].fileName);
	if(posOfLastDot != 0 && !dir)
	{
		context->que[slot].extension = &context->que[slot].fileName[posOfLastDot + 1];
		context->que[slot].fileNameWithoutExt[posOfLastDot] = 0;
	}
	else
		context->que[slot].extension = context->que[slot].fileName;

	if(posOfLastSlash == -1)
	{
		context->que[slot].fileNameWithoutDir = context->que[slot].fileName;
		context->que[slot].fileNameWithoutDirAndExt = context->que[slot].fileNameWithoutExt;
	}
	else
	{
		context->que[slot].fileNameWithoutDir = &context->que[slot].fileName[posOfLastSlash + 1];
		context->que[slot].fileNameWithoutDirAndExt = &context->que[slot].fileNameWithoutExt[posOfLastSlash + 1];
	}

	if(dir == 0 && fileNameMustContain != 0 && ReturnStringPos(fileNameMustContain, context->que[slot].fileNameWithoutDir) == 0)
		return; //Could not find part of filename we require for this to be added to dir queue

	if(context->que[slot].fileName[0] != 0)
	{
		context->que[slot].active = 1;
		if( (context->addDirs && dir) || (context->addFiles && !dir) )
		{
			if(dir)
				context->dirsTotal++;
			else 
				context->filesTotal++;
			context->alphaque[context->alphaqueNum] = slot;
			context->alphaqueNum++;
		}
	}
}

static void Scan(dirContext_s *context, char *startDir, char *fileNameMustContain)
{
	//Initial search
	context->que[0].active = 1; 
	context->que[0].dirSearch = 1;
	context->que[0].dir = 1;
	context->que[0].fileSize = 0;
	context->que[0].fileName[0] = 0;
	context->que[0].fileNameWithoutExt[0] = 0;
	context->que[0].extension = 0;
	context->que[0].fileNameWithoutDir = 0;
	context->que[0].fileNameWithoutDirAndExt = 0;
	context->que[0].archive = -1;

	//Remove final slash if it exists in startDir
	int i = 0;
	while(startDir[i] != 0)
		i++;
	if(i != 0 && (startDir[i - 1] == '\\' || startDir[i - 1] == '/') )
		startDir[i - 1] = 0;

	char searchPath[DIR_MAXSTRING];
	s32 prevSlot = -1;
	while(1) //Cycle through dir queue until it's empty
	{
		int q = Dir_FindActiveDirQueue(context, prevSlot + 1);
		if(q == -1)
			break;
		prevSlot = q;
		if(context->que[q].fileName[0] == 0)
			sprintf_s(searchPath, DIR_MAXSTRING, "%s\\*", startDir);
		else
			sprintf_s(searchPath, DIR_MAXSTRING, "%s\\%s\\*", startDir, context->que[q].fileName);
		HANDLE hFind = FindFirstFile(searchPath, &context->findFileData);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			break;
			//TODO: Replace StatusUpdate with another similar command
			//StatusUpdate("FindFirstFile failed (%d)\n", GetLastError());
			return;
		}
		while(1)
		{
			unsigned long long lastModified = (((ULONGLONG) context->findFileData.ftLastWriteTime.dwHighDateTime) << 32) + context->findFileData.ftLastWriteTime.dwLowDateTime;

			if(context->findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) //This is a directory
				Dir_AddToQueue(context, startDir, searchPath, context->findFileData.cFileName, 1, lastModified, 0, fileNameMustContain);
			else //Should be a normal file
				Dir_AddToQueue(context, startDir, searchPath, context->findFileData.cFileName, 0, lastModified, ((unsigned long long) context->findFileData.nFileSizeHigh << 32) + context->findFileData.nFileSizeLow, fileNameMustContain);

			int success = FindNextFile(hFind, &context->findFileData);
			if(!success)
				break;
		}
		context->que[q].dirSearch = 0;
		FindClose(hFind);
	}

	context->que[0].active = 0; //Disable initial search entry

	//Deactivate any added files or dirs if we're not searching for both dirs and files (this is only necessary for when external functions iterate through entire list, and not only alphaque)
	for(unsigned int i = 0; i < context->highestDirNum; i++)
	{
		if(context->que[i].active && ( (!context->addDirs && context->que[i].dir) || (!context->addFiles && !context->que[i].dir) ) )
			context->que[i].active = 0;
	}
}

bool Dir_GetSizeAndModifiedDate(char *filePath, unsigned long long *date, unsigned long long *size) //(TODO: Move this to main-FIO.cpp?)
{
	if(!filePath)
		return 0;
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(filePath, &findFileData);
	if(hFind == INVALID_HANDLE_VALUE)
		return 0;
	*date = (((ULONGLONG) findFileData.ftLastWriteTime.dwHighDateTime) << 32) + findFileData.ftLastWriteTime.dwLowDateTime;
	if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) //This is a directory
		*size = 0;
	else //Should be a normal file
		*size = (((ULONGLONG) findFileData.nFileSizeHigh) << 32) + findFileData.nFileSizeLow;
	FindClose(hFind);
	return 1;
}

static void ProcessPointersInQueue(dirContext_s *context, char *fileNameMustContain)
{
	for(unsigned int k = 0; k < context->highestDirNum; k++)
	{
		if(!context->que[k].active)
			continue;

		int posOfLastDot = 0;
		int posOfLastSlash = -1;
		int i = 0;
		while(context->que[k].fileName[i] != 0)
		{
			if(context->que[k].fileName[i] == '.')
				posOfLastDot = i;
			if(context->que[k].fileName[i] == '\\' || context->que[k].fileName[i] == '/')
				posOfLastSlash = i;
			i++;
		}

		if(posOfLastDot != 0 && !context->que[k].dir)
		{
			context->que[k].extension = &context->que[k].fileName[posOfLastDot + 1];
			context->que[k].fileNameWithoutExt[posOfLastDot] = 0;
		}
		else
			context->que[k].extension = context->que[k].fileName;
		
		if(posOfLastSlash == -1)
		{
			context->que[k].fileNameWithoutDir = context->que[k].fileName;
			context->que[k].fileNameWithoutDirAndExt = context->que[k].fileNameWithoutExt;
		}
		else
		{
			context->que[k].fileNameWithoutDir = &context->que[k].fileName[posOfLastSlash + 1];
			context->que[k].fileNameWithoutDirAndExt = &context->que[k].fileNameWithoutExt[posOfLastSlash + 1];
		}

		if(context->que[k].dir == 0 && fileNameMustContain != 0 && ReturnStringPos(fileNameMustContain, context->que[k].fileNameWithoutDir) == 0) //Could not find part of filename we require for this to be added to dir queue, so let's skip it
			context->que[k].active = 0;
	}
}

void Package_SearchForFiles(dirContext_s *dirContext, char *path, bool subDir = 1, bool addDirs = 0, bool addFiles = 1); //From package.cpp. We declare it here rather than package.h as it's only used here and I don't want everything to need to include dir.h for package.h

dirContext_s *CreateFileQueue(char *startDir, bool subdir, bool addDir, bool addFile, bool alsoSearchThroughPackages, char *fileNameMustContain) //Start dir is where it searches. subdir is whether or not we search for subdirectories. addDirs is whether we added directories to the final list of found items. addFiles is the same but for files
{
	dirContext_s *context = (dirContext_s *) malloc(sizeof(dirContext_s));

	//Initialize context
	memset(context, 0, sizeof(dirContext_s));
	context->queueCurrentSize = DIRQUE_STARTINGSIZE;
	context->que = (dir_queue_s *) malloc(sizeof(dir_queue_s) * context->queueCurrentSize); 
	context->alphaque = (int *) malloc(sizeof(int) * context->queueCurrentSize); 

	//Define some variables
	context->searchSubDirs = subdir;
	context->addDirs = addDir;
	context->addFiles = addFile;

	//Actual scan
	context->highestDirNum = 1;
	context->queCur = 0;
	Scan(context, startDir, fileNameMustContain);

	//Manage char pointers within the queue, This has to be done after the scan as these pointers become invalid during any realloc()
	ProcessPointersInQueue(context, fileNameMustContain);

	return context;
}

void DeleteFileQueue(dirContext_s *context)
{
	if(context == 0)
		return;
	free(context->que);
	free(context->alphaque);
	free(context);
}