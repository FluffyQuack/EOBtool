#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "Types.h"

char *ReturnStringPos(char *find, char *in) //Find position of "find" string within "in" string. Does a lower case comparison.
{
	int findPos = 0, inPos = 0, returnPos = 0;
	while (in[inPos] != 0)
	{
		if (tolower(in[inPos]) == tolower(find[findPos]))
		{
			if (findPos == 0)
				returnPos = inPos;
			findPos++;
			if (find[findPos] == 0)
				return &in[returnPos];
		}
		else
		{
			findPos = 0;
			if (returnPos > 0)
			{
				inPos = returnPos + 1;
				returnPos = 0;
			}
		}
		inPos++;
	}
	return 0;
}

bool FileExists(char *path) //Checks if file exists
{
	DWORD dwAttrib = GetFileAttributes(path);
	if((dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) == 1)
		return 1;
	return 0;
}