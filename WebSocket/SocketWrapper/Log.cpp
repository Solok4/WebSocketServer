#include "Log.h"


Clog::Clog()
{
}


Clog::~Clog()
{
}

void Clog::Log(LogTag tag,const char* format, ...)
{

	char result[2048];
	char VAContent[2048];
	switch (tag)
	{
	case Info:
		strcpy_s(result, 8, "[Log]: ");
		//strncpy(result, "[Log]: ", 8);
		break;
	case Warning:
		strcpy_s(result, 12, "[Warning]: ");
		//strncpy(result, "[Warning]: ", 12);
		break;
	case Error:
		strcpy_s(result, 10, "[Error]: ");
		//strncpy(result, "[Error]: ", 10);
		break;
	default:

		break;
	}

	va_list list;
	va_start(list, format);
	vsprintf_s(VAContent, format, list);
	strcat_s(result, sizeof(result), VAContent);
	//strcat(result, VAContent);
	printf("%s\n",result);

	va_end(list);
}
