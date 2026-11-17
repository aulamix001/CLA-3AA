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
        while((pos=log.find("First painted frame revealed",pos))!=std::string::npos){++count;++pos;}
        if(count>=expected)return true;
        MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }return false;
}
static HWND findCover(HWND parent){
    const auto editor=FindWindowExW(parent,nullptr,L"CLA3AStudioWebView",nullptr);
    return editor?FindWindowExW(editor,nullptr,L"CLA3AStudioLoading",nullptr):nullptr;
}
static bool loadingCoverPaints(HWND parent){
    const auto cover=findCover(parent);if(!cover||!IsWindowVisible(cover))return false;
    auto target=GetDC(cover);auto dc=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,640,390);auto old=SelectObject(dc,bitmap);
    SendMessageW(cover,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
    const auto color=GetPixel(dc,10,10);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(cover,target);
    return color!=CLR_INVALID&&color!=RGB(0,0,0);
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
        ok=ok&&views[i]&&views[i]->attached(parents[i],Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue&&loadingCoverPaints(parents[i]);
    }
    ok=ok&&waitForReady(2);
    if(ok)for(auto parent:parents)ok=ok&&!IsWindowVisible(findCover(parent));
    if(ok){
        Cla::startupTrace("Both instances ready; testing close/reopen");
        views[0]->removed();views[0]->release();views[0]=controllers[0]->createView(Steinberg::Vst::ViewType::kEditor);
        ok=views[0]&&views[0]->attached(parents[0],Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue&&loadingCoverPaints(parents[0])&&waitForReady(3)&&!IsWindowVisible(findCover(parents[0]));
    }
    for(int i=0;i<2;++i){if(views[i]){views[i]->removed();views[i]->release();}if(parents[i])DestroyWindow(parents[i]);if(controllers[i]){controllers[i]->terminate();controllers[i]->release();}}
    std::cout<<readLog();
    if(!ok){std::cerr<<"FAIL: loading screen, first painted frame or reopen presentation failed.\n";return 1;}
    std::cout<<"PASS: native non-black loading screens, two painted editors, and close/reopen cover handoff.\n";return 0;
}
