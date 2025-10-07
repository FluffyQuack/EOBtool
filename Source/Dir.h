#pragma once

#define DIRQUE_STARTINGSIZE 100
#define DIRQUE_INCREASESIZEBY 100
#define FILENAME_SIZE_BIG 260
#define DIR_MAXSTRING 300

struct dir_queue_s
{
	unsigned long long lastModified; //Taken from a file's "FILETIME ftLastWriteTime"
	char fileName[FILENAME_SIZE_BIG]; //Filename, includes directory structure
	char fileNameWithoutExt[FILENAME_SIZE_BIG]; //Filename, includes directory structure but no extension
	char *extension; //Filename extension
	char *fileNameWithoutDir; //Filename without dir
	char *fileNameWithoutDirAndExt; //Filename without dir and ext
	bool active;
	unsigned long long fileSize;
	bool dir; //Whether or not this is a dir
	bool dirSearch; //Whether or not this is a subdir we're gonna search through
	int archive; //If not -1, this signifies what specific package file is in
};

struct dirContext_s
{
	WIN32_FIND_DATA findFileData;
	dir_queue_s *que;
	unsigned int filesTotal; //This and dirsTotal are for reference only.
	unsigned int dirsTotal;
	int *alphaque;
	int alphaqueNum; //This is what external functions should use for iterating through final list
	bool addDirs;
	bool addFiles;
	bool searchSubDirs;
	unsigned int highestDirNum;
	unsigned int queueCurrentSize;
	unsigned int queCur;
};

bool Dir_CompareFilenames(char *file1, char *file2);
bool Dir_DirExists(dirContext_s *context, char *str);
bool Dir_FileExists(dirContext_s *context, char *str);
int Dir_FindActiveDirQueue(dirContext_s *context);
int Dir_FindInactiveQueue(dirContext_s *context);
void Dir_AddToQueue(dirContext_s *context, char *startDir, char *searchPath, char *dirName, bool dir, unsigned long long lastModified = 0, unsigned long long fileSize = 0, char *fileNameMustContain = 0);
bool Dir_GetSizeAndModifiedDate(char *filePath, unsigned long long *date, unsigned long long *size);
dirContext_s *CreateFileQueue(char *startDir, bool subdir = 1, bool addDir = 0, bool addFile = 1, bool alsoSearchThroughPackages = 1, char *fileNameMustContain = 0);
void DeleteFileQueue(dirContext_s *context);