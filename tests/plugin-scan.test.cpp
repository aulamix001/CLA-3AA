#include "../native/Vst2Abi.h"
#include "../native/PluginProcessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
static int automated=0;
static std::intptr_t host(Cla::Vst2::Effect*,std::int32_t op,std::int32_t,std::intptr_t,void*,float){if(op==1)return 2400;if(op==0)++automated;return 0;}
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"expected VST3 and VST2 binaries");
 const auto vst3=LoadLibraryW(argv[1]);check(vst3!=nullptr,"VST3 LoadLibrary failed");
 const auto init=reinterpret_cast<bool(*)()>(GetProcAddress(vst3,"InitDll"));check(init&&init(),"VST3 InitDll failed");
 auto factoryFunction=reinterpret_cast<Steinberg::IPluginFactory*(*)()>(GetProcAddress(vst3,"GetPluginFactory"));check(factoryFunction!=nullptr,"VST3 factory export missing");
 auto factory=factoryFunction();check(factory&&factory->countClasses()==2,"VST3 class registration invalid");
 Steinberg::PClassInfo info{};check(factory->getClassInfo(0,&info)==Steinberg::kResultOk,"VST3 class metadata missing");
 Steinberg::Vst::IComponent* component=nullptr;check(factory->createInstance(info.cid,Steinberg::Vst::IComponent::iid,reinterpret_cast<void**>(&component))==Steinberg::kResultOk&&component,"VST3 processor cannot be created");
 check(component->initialize(nullptr)==Steinberg::kResultOk,"VST3 processor init failed");component->terminate();component->release();factory->release();
 auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(vst3,"ExitDll"));check(exit&&exit(),"VST3 ExitDll failed");FreeLibrary(vst3);
 const auto vst2=LoadLibraryW(argv[2]);check(vst2!=nullptr,"VST2 LoadLibrary failed");
 auto entry=reinterpret_cast<Cla::Vst2::Effect*(*)(Cla::Vst2::Host)>(GetProcAddress(vst2,"VSTPluginMain"));check(entry!=nullptr,"VSTPluginMain export missing");
 auto e=entry(host);check(e&&e->magic==Cla::Vst2::magic&&e->numParams==9&&e->numPrograms==44,"VST2 ABI or programs invalid");
 e->dispatcher(e,0,0,0,nullptr,0);e->dispatcher(e,10,0,0,nullptr,48000);
 auto clean=[&]{e->setParameter(e,0,.408f);e->setParameter(e,1,0);e->setParameter(e,2,1);e->setParameter(e,3,1);e->setParameter(e,4,.5f);e->setParameter(e,7,1);e->setParameter(e,8,0);e->dispatcher(e,12,0,1,nullptr,0);};clean();
 std::vector<float> input(48000),left(48000),right(48000);for(int i=0;i<48000;++i)input[i]=static_cast<float>(std::sin(i*.13)*.3);
 float* ins[]={input.data(),input.data()};float* outs[]={left.data(),right.data()};e->processReplacing(e,ins,outs,48000);
 for(int i=1000;i<48000;++i)check(std::abs(left[i]-input[i])<1.e-6,"VST2 unity mapping failed");
 e->setParameter(e,1,1);e->processReplacing(e,ins,outs,48000);double dry=0,wet=0;for(int i=24000;i<48000;++i){dry+=input[i]*input[i];wet+=left[i]*left[i];}check(wet<dry*.1,"VST2 Peak Reduction not processing");
 void* chunk=nullptr;auto bytes=e->dispatcher(e,23,0,0,&chunk,0);check(bytes==72&&chunk,"VST2 chunk missing");std::vector<char> state(static_cast<char*>(chunk),static_cast<char*>(chunk)+bytes);e->setParameter(e,0,0);check(e->dispatcher(e,24,0,bytes,state.data(),0)==1,"VST2 state restore failed");
 e->dispatcher(e,2,0,27,nullptr,0);check(e->dispatcher(e,3,0,0,nullptr,0)==27,"VST2 program selection failed");
 const auto parent=CreateWindowExW(0,L"STATIC",L"VST2 host test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,680,450,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 check(e->dispatcher(e,14,0,0,parent,0)==1,"VST2 editor failed");auto editor=FindWindowExW(parent,nullptr,L"CLA3AStudioWebView",nullptr);auto cover=FindWindowExW(editor,nullptr,L"CLA3AStudioLoading",nullptr);
 SendMessageW(cover,WM_COMMAND,8001,0);auto knob=GetDlgItem(cover,8100);check(knob!=nullptr,"VST2 native fallback missing");SendMessageW(knob,TBM_SETPOS,TRUE,850);SendMessageW(cover,WM_HSCROLL,TB_THUMBTRACK,reinterpret_cast<LPARAM>(knob));check(std::abs(e->getParameter(e,0)-.85f)<1.e-6&&automated>0,"VST2 editor changes not connected to audio/host");
 e->setParameter(e,0,.4f);SendMessageW(editor,WM_TIMER,3,0);check(SendMessageW(knob,TBM_GETPOS,0,0)==400,"VST2 host automation not reflected by editor");
 e->dispatcher(e,15,0,0,nullptr,0);DestroyWindow(parent);e->dispatcher(e,1,0,0,nullptr,0);FreeLibrary(vst2);
 std::cout<<"PASS: VST3 DLL scan/instantiate; VST2 ABI, 44 programs, audio, state, editor and bidirectional automation.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<" Win32="<<GetLastError()<<'\n';return 1;}}
