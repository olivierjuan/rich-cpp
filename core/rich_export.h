#pragma once
#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef rich_core_EXPORTS
#    define RICH_EXPORT __declspec(dllexport)
#  else
#    define RICH_EXPORT __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define RICH_EXPORT __attribute__((visibility("default")))
#else
#  define RICH_EXPORT
#endif