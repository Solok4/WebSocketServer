#pragma once
#include<stdio.h>
#include<stdarg.h>
#include<cstring>

enum LogTag
{
	Info=0,
	Warning,
	Error,
};

class Clog
{
public:
	Clog();
	~Clog();
	static void Log(LogTag tag,const char* format, ...);
};

