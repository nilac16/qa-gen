#pragma once

#ifndef QAGEN_DEFS_H
#define QAGEN_DEFS_H


/** C4996: I tell them to stop but they just keep complaining */
#pragma warning(disable: 4996)
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE


/** C4100 and C4101: Why are these not enabled by default? */
#pragma warning(1: 4100 4101)


#define UNICODE 1
#define _UNICODE 1
#include <Windows.h>


/** C should have had a keyword for this long ago */
#define BUFLEN(buf) (sizeof (buf) / sizeof *(buf))


/** "Extensions" implementing standard features? */
#define restrict __restrict


/** stdbool *MUST* be supported by the target implementation */
#include <stdbool.h>


#if __has_include(<threads.h>)
#   include <threads.h>
#else
#   define thread_local __declspec(thread)
#endif


/** Pluralizing format helper */
#define PLFW(val) ((val == 1) ? L"" : L"s")


#endif /* QAGEN_DEFS_H */
