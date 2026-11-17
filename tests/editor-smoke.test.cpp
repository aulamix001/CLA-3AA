#include "../native/PluginEditor.h"
#include "../native/StartupDiagnostics.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>

void* moduleHandle=nullptr;
int main(){
    moduleHandle=GetModuleHandleW(nullptr);
    DeleteFileW(Cla::startupLogPath().c_str());
    Cla::startupTrace("Windows editor smoke test begin");
    auto controller=new Cla::Controller;
    if(controller->initialize(nullptr)!=Steinberg::kResultOk){controller->release();return 1;}
    const auto parent=CreateWindowExW(0,L"STATIC",L"CLA-3A editor smoke test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,680,450,nullptr,nullptr,static_cast<HINSTANCE>(moduleHandle),nullptr);
    if(!parent){controller->terminate();controller->release();return 1;}
    auto view=controller->createView(Steinberg::Vst::ViewType::kEditor);
    const bool attached=view&&view->attached(parent,Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue;
    bool ready=false;const auto started=GetTickCount64();
    while(attached&&!ready&&GetTickCount64()-started<30000){
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        std::ifstream log(std::filesystem::path(Cla::startupLogPath()),std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(log)),std::istreambuf_iterator<char>());
        ready=text.find("JavaScript ready received")!=std::string::npos;
        if(!ready)MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
    if(view){view->removed();view->release();}
    DestroyWindow(parent);controller->terminate();controller->release();
    std::ifstream log(std::filesystem::path(Cla::startupLogPath()),std::ios::binary);
    std::cout<<log.rdbuf();
    if(!ready){std::cerr<<"FAIL: editor did not receive JavaScript ready within 30 seconds.\n";return 1;}
    std::cout<<"PASS: real Windows HWND + WebView2 + packaged UI reached JavaScript ready.\n";return 0;
}
