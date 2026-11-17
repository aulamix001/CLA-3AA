#include "../native/PluginEditor.h"
#include "../native/StartupDiagnostics.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>

void* moduleHandle=nullptr;
static std::string readLog(){
    std::ifstream log(std::filesystem::path(Cla::startupLogPath()),std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(log)),std::istreambuf_iterator<char>());
}
static bool waitForReady(int expected){
    const auto start=GetTickCount64();
    while(GetTickCount64()-start<30000){
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        const auto log=readLog();std::size_t pos=0;int count=0;
        while((pos=log.find("JavaScript ready received",pos))!=std::string::npos){++count;++pos;}
        if(count>=expected)return true;
        MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }return false;
}
int main(){
    moduleHandle=GetModuleHandleW(nullptr);DeleteFileW(Cla::startupLogPath().c_str());
    Cla::startupTrace("Windows two-instance editor smoke test begin");
    Cla::Controller* controllers[2]{};Steinberg::IPlugView* views[2]{};HWND parents[2]{};bool ok=true;
    for(int i=0;i<2&&ok;++i){
        controllers[i]=new Cla::Controller;ok=controllers[i]->initialize(nullptr)==Steinberg::kResultOk;
        parents[i]=CreateWindowExW(0,L"STATIC",L"CLA-3A editor smoke test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,680,450,nullptr,nullptr,static_cast<HINSTANCE>(moduleHandle),nullptr);
        if(!parents[i]){ok=false;break;}
        views[i]=controllers[i]->createView(Steinberg::Vst::ViewType::kEditor);
        ok=ok&&views[i]&&views[i]->attached(parents[i],Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue;
    }
    ok=ok&&waitForReady(2);
    if(ok){
        Cla::startupTrace("Both instances ready; testing close/reopen");
        views[0]->removed();views[0]->release();views[0]=controllers[0]->createView(Steinberg::Vst::ViewType::kEditor);
        ok=views[0]&&views[0]->attached(parents[0],Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue&&waitForReady(3);
    }
    for(int i=0;i<2;++i){if(views[i]){views[i]->removed();views[i]->release();}if(parents[i])DestroyWindow(parents[i]);if(controllers[i]){controllers[i]->terminate();controllers[i]->release();}}
    std::cout<<readLog();
    if(!ok){std::cerr<<"FAIL: two-instance startup or editor reopen did not reach JavaScript ready.\n";return 1;}
    std::cout<<"PASS: two simultaneous Windows WebView2 editors and close/reopen reached ready with offline UI.\n";return 0;
}
