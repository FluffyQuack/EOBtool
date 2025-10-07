#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <direct.h>
#include <windows.h>
#include "Types.h"
#include "Dir.h"
#include "Misc.h"

//PAK format: https://moddingwiki.shikadi.net/wiki/PAK_Format_(Westwood)

enum
{
	MODE_NOTHING,
	MODE_EXTRACT,
	MODE_CREATE,
};

bool extractDuplicates = 0;

//Make directory. This version handles a path with a file, so it'll not add a directory with file name. Note: If you give this a path ending in a directory with no slash that dir won't get added (as it assumes it ends with a file)
void MakeDirectory_PathEndsWithFile(char *fullpath, int pos = 0)
{
	char path[_MAX_PATH];
	memset(path, 0, _MAX_PATH);

	if(pos != 0)
		memcpy(path, fullpath, pos);

	while(1)
	{
		if(fullpath[pos] == 0)
			break;
		path[pos] = fullpath[pos];
		pos++;
		if(fullpath[pos] == '\\' || fullpath[pos] == '/')
		{
			_mkdir(path);
		}
	}
}

void CreatePAK(char *path)
{
	//Get a directory listing of all files in path
	dirContext_s *dirContext = CreateFileQueue(path, 0);

	//Count files
	u32 fileCount = 0;
	u32 totalfileNameSize = 0;
	for(int i = 0; i < dirContext->alphaqueNum; i++)
	{
		dir_queue_s *dir = &dirContext->que[dirContext->alphaque[i]];
		if(!dir->active)
			continue;
		fileCount++;
		totalfileNameSize += (u32) strlen(dir->fileNameWithoutDir) + 1;

		//Reduce size if this has an extension longer than 3 characters
		u32 extSize = strlen(dir->extension);
		if(extSize > 3)
			totalfileNameSize -= extSize - 3;
	}
	if(fileCount == 0)
	{
		printf("Error: No files to add\n");
		DeleteFileQueue(dirContext);
		return;
	}

	//Create new PAK
	char pakName[_MAX_PATH];
	sprintf_s(pakName, _MAX_PATH, "%s.PAK", path);
	FILE *pakFile;
	fopen_s(&pakFile, pakName, "wb");
	if(!pakFile)
	{
		printf("Error: Failed to open %s for writing\n", pakName);
		DeleteFileQueue(dirContext);
		return;
	}

	//Calculate position of file data inside PAK
	u32 curFileOffset = totalfileNameSize + fileCount * sizeof(u32) + sizeof(u32); //Start offset for each file + end of file offset + total size of filename strings

	//Write header
	for(int i = 0; i < dirContext->alphaqueNum; i++)
	{
		dir_queue_s *dir = &dirContext->que[dirContext->alphaque[i]];
		if(!dir->active)
			continue;
		fwrite(&curFileOffset, sizeof(u32), 1, pakFile); //Offset to file data

		//Get size of filename (we skip characters that go beyond the 3 character limit for the extension)
		u32 fileNameSize = (u32) strlen(dir->fileNameWithoutDir);
		{
			//Reduce size if this has an extension longer than 3 characters
			u32 extSize = strlen(dir->extension);
			if(extSize > 3)
				fileNameSize -= extSize - 3;
		}

		//Make filename upper case
		for(int l = 0; l < fileNameSize; l++)
			dir->fileNameWithoutDir[l] = toupper(dir->fileNameWithoutDir[l]);

		fwrite(dir->fileNameWithoutDir, fileNameSize, 1, pakFile);
		fputc(0, pakFile);

		curFileOffset += (u32) dir->fileSize;
	}
	fwrite(&curFileOffset, sizeof(u32), 1, pakFile); //Final offset, which should match up with total PAK size

	//Write all file data
	for(int i = 0; i < dirContext->alphaqueNum; i++)
	{
		dir_queue_s *dir = &dirContext->que[dirContext->alphaque[i]];
		if(!dir->active)
			continue;
		FILE *file;
		char filePath[_MAX_PATH];
		sprintf_s(filePath, _MAX_PATH, "%s\\%s", path, dir->fileName);
		fopen_s(&file, filePath, "rb");
		if(!file)
		{
			printf("Error: Failed to open %s for reading\n", filePath);
			fclose(pakFile);
			DeleteFileQueue(dirContext);
			return;
		}
		u8 *fileData = (u8 *) malloc(dir->fileSize);
		fread(fileData, dir->fileSize, 1, file);
		fclose(file);
		fwrite(fileData, dir->fileSize, 1, pakFile);
		free(fileData);
		printf("Added %s\n", dir->fileNameWithoutDir);
	}

	//Finish
	printf("Wrote %s with %u files\n", pakName, fileCount);
	fclose(pakFile);
	DeleteFileQueue(dirContext);
}

void ExtractPAK(char *path)
{
	//Open file
	FILE *file;
	fopen_s(&file, path, "rb");
	if(!file)
	{
		printf("Error: Can't open %s for reading\n", path);
		return;
	}

	//Get PAK size
	u32 pakSize = 0;
	{
		fpos_t fpos;
		_fseeki64(file, 0, SEEK_END);
		fgetpos(file, &fpos);
		_fseeki64(file, 0, SEEK_SET);
		pakSize = (u32) fpos;
	}

	//Remove dot from path
	{
		s32 dotPos = -1, pos = 0;
		while(1)
		{
			if(path[pos] == '.') dotPos = pos;
			else if(path[pos] == 0) break;
			pos++;
		}
		if(dotPos != -1) path[dotPos] = 0;
	}

	//Read files, one by one
	u32 curPAKPos = 0;
	u32 firstFileOffset = 0;
	u32 fileCount = 0;
	while(1)
	{
		char filename[100];
		
		//Reset read position
		_fseeki64(file, curPAKPos, SEEK_SET);

		//File offset
		u32 fileOffset = 0;
		fread(&fileOffset, sizeof(u32), 1, file); curPAKPos += sizeof(u32);
		if(firstFileOffset == 0) firstFileOffset = fileOffset;

		//Check if we're done with filelist
		if(curPAKPos >= firstFileOffset || curPAKPos >= pakSize)
			break;

		//Check if fileoffset is valid
		if(fileOffset >= pakSize)
		{	
			printf("Error: Invalid file offset");
			fclose(file);
			return;
		}

		//Read filename (one byte at a time, whoo!!)
		{
			u32 filenamePos = 0;
			while(1)
			{
				fread(&filename[filenamePos], 1, 1, file);
				filenamePos++;
				if(filename[filenamePos - 1] == 0) break;
			}
			curPAKPos += filenamePos;
		}

		//Determine size of file
		u32 fileSize = 0;
		fread(&fileSize, sizeof(u32), 1, file);
		if(fileSize >= pakSize || fileSize <= fileOffset) fileSize = pakSize; //Apparently some EOB1 PAKs have a gibberish pointer at the end, so attempt to fix it
		fileSize -= fileOffset;

		//Create full file path
		char outputPath[_MAX_PATH];
		sprintf_s(outputPath, _MAX_PATH, "%s\\%s", path, filename);

		//Check if there's already a file with this filename, and if so, give this a different name (this is necessary because EOBDATA4.PAK contains multiple files that share filename!)
		if(extractDuplicates)
		{
			s32 variant = 0;
			while(FileExists(outputPath))
			{
				sprintf_s(outputPath, _MAX_PATH, "%s\\%s_%03i", path, filename, variant);
				variant++;
			}
		}

		//Read file data
		char *fileData = (char *) malloc(fileSize);
		_fseeki64(file, fileOffset, SEEK_SET);
		fread(fileData, 1, fileSize, file);

		//Open output file
		FILE *outputFile;
		MakeDirectory_PathEndsWithFile(outputPath);
		fopen_s(&outputFile, outputPath, "wb");
		if(!outputFile)
		{
			printf("Error: Failed too open file %s for writing\n", outputPath);
		}
		else
		{
			fwrite(fileData, 1, fileSize, outputFile);
			fclose(outputFile);
			printf("Extracted %s\n", filename);
		}
		fileCount++;
	}

	//Finish
	fclose(file);
	printf("Extracted %i files from %s.PAK\n", fileCount, path);
}

int _tmain(int argc, _TCHAR *argv[])
{
	//Defaults
	int mode = MODE_NOTHING;

	//Process command line arguments
	int i = 1, strcount = 0;
	char *str1 = 0;
	while(argc > i)
	{
		if(argv[i][0] == '-')
		{
			if(_stricmp(argv[i], "-x") == 0)
				mode = MODE_EXTRACT;
			else if(_stricmp(argv[i], "-c") == 0)
				mode = MODE_CREATE;
			else if(_stricmp(argv[i], "-d") == 0)
				extractDuplicates = 1;
		}
		else
		{
			if(strcount == 0)
				str1 = argv[i];
			strcount++;
		}
		i++;
	}

	if(mode == MODE_EXTRACT)
	{
		ExtractPAK(str1);
	}
	else if(mode == MODE_CREATE)
	{
		CreatePAK(str1);
	}
	return 1;
}