#pragma once
#include <windows.h>
#include <string>
#include <cstdio>

namespace Cla {
// Startup/UI only: never call from process(), DSP tick(), or the audio thread.
inline std::wstring startupLogPath(){
    wchar_t folder[MAX_PATH]{};
    const DWORD count=GetTempPathW(MAX_PATH,folder);
    if(!count||count>=MAX_PATH)return {};
    return std::wstring(folder)+L"CLA3AStudio-startup-"+std::to_wstring(GetCurrentProcessId())+L".log";
}
inline void startupTrace(const char* stage,HRESULT result=S_OK){
    const DWORD previous=GetLastError();
    std::string clean(stage?stage:"unknown");
    for(auto& c:clean)if(c=='\r'||c=='\n')c=' ';
    char line[1024]{};
    std::snprintf(line,sizeof(line),"CLA3A 0.1.4 tick=%llu pid=%lu tid=%lu %s hr=0x%08lX\r\n",
        static_cast<unsigned long long>(GetTickCount64()),GetCurrentProcessId(),GetCurrentThreadId(),clean.c_str(),static_cast<unsigned long>(result));
    OutputDebugStringA(line);
    const auto path=startupLogPath();
    if(!path.empty()){
        const auto file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(file,line,static_cast<DWORD>(std::char_traits<char>::length(line)),&written,nullptr);CloseHandle(file);}
    }
    SetLastError(previous);
}
struct StartupScope {
    const char* stage;ULONGLONG started=GetTickCount64();
    explicit StartupScope(const char* name):stage(name){startupTrace((std::string(stage)+" begin").c_str());}
    ~StartupScope(){startupTrace((std::string(stage)+" end elapsed_ms="+std::to_string(GetTickCount64()-started)).c_str());}
};
}
