#pragma once

// Export/import macros for Windows DLL
#ifdef _WIN32
    #ifdef yona_lib_EXPORTS
        #define YONA_API __declspec(dllexport)
    #else
        #define YONA_API __declspec(dllimport)
    #endif
#else
    #define YONA_API
#endif
