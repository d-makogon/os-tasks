#pragma once

#include <stdio.h>
#include <stdbool.h>

int SetSimpleSignalHandler(int sig, void (*handler)());

bool str2int(const char* str, int* num);

void PrintError(FILE* stream, const char* format, ...);
