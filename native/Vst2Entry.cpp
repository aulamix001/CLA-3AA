#include "Vst2Abi.h"
#include "PluginEditor.h"
#include "NativePresets.h"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
void* moduleHandle=nullptr;
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){moduleHandle=module;DisableThreadLibraryCalls(module);}return TRUE;}
namespace Cla::Vst2 {
struct Instance {
 Effect effect{};Host host=nullptr;Dsp dsp;double rate=48000.;std::atomic<int> program{0};
 std::array<std::atomic<double>,parameterCount> values{};
 std::array<std::atomic<double>,3> meters{{-120.,-120.,0.}};
 Controller* controller=nullptr;Steinberg::IPlugView* view=nullptr;Values chunk{};
 struct Rect{std::int16_t top,left,bottom,right;} rect{0,0,390,640};
 Values snapshot()const{Values result;for(std::size_t i=0;i<parameterCount;++i)result[i]=values[i].load();return result;}
 void select(int index){if(index<0||index>=static_cast<int>(std::size(nativePresets)))return;program=index;for(std::size_t i=0;i<parameterCount;++i)values[i].store(nativePresets[index].values[i]);}
 void closeEditor(){if(view){view->removed();view->release();view=nullptr;}if(controller){controller->terminate();controller->release();controller=nullptr;}}
 ~Instance(){closeEditor();}
 bool openEditor(void* parent){
  closeEditor();controller=new Controller;if(controller->initialize(nullptr)!=Steinberg::kResultOk){closeEditor();return false;}
  controller->externalContext=this;
  controller->externalValues=[](void* context,Values& output){output=static_cast<Instance*>(context)->snapshot();};
  controller->externalMeters=[](void* context,std::array<double,3>& output){auto self=static_cast<Instance*>(context);for(int i=0;i<3;++i)output[i]=self->meters[i].load();};
  controller->externalEdit=[](void* context,std::size_t i,double normalized){auto self=static_cast<Instance*>(context);self->values[i].store(denormalize(i,normalized));if(self->host){self->host(&self->effect,43,static_cast<int>(i),0,nullptr,0);self->host(&self->effect,0,static_cast<int>(i),0,nullptr,static_cast<float>(normalized));self->host(&self->effect,44,static_cast<int>(i),0,nullptr,0);}};
  controller->syncExternalValues();view=controller->createView(Steinberg::Vst::ViewType::kEditor);
  return view&&view->attached(parent,Steinberg::kPlatformTypeHWND)==Steinberg::kResultTrue;
 }
 template<class T> void audio(T** in,T** out,int count,bool add=false){
  dsp.setValues(snapshot());for(int i=0;i<count;++i){const double l=in&&in[0]?in[0][i]:0,r=in&&in[1]?in[1][i]:l;const auto y=dsp.tick(l,r);if(out&&out[0])out[0][i]=static_cast<T>(y.first)+(add?out[0][i]:0);if(out&&out[1])out[1][i]=static_cast<T>(y.second)+(add?out[1][i]:0);}
  meters[0].store(dsp.inputDb());meters[1].store(dsp.outputDb());meters[2].store(dsp.reductionDb());
 }
};
static Instance* self(Effect* e){return static_cast<Instance*>(e->object);}
static void text(void* destination,const char* source,std::size_t size){if(destination&&size){std::strncpy(static_cast<char*>(destination),source,size-1);static_cast<char*>(destination)[size-1]=0;}}
static std::intptr_t dispatch(Effect* effect,std::int32_t opcode,std::int32_t index,std::intptr_t value,void* ptr,float opt){
 auto p=self(effect);
 switch(opcode){
 case 0:return 1;
 case 1:delete p;return 1;
 case 2:p->select(static_cast<int>(value));return 1;
 case 3:return p->program.load();
 case 4:return 0;
 case 5:text(ptr,nativePresets[p->program].name,24);return 1;
 case 6:text(ptr,index==Trim?"dB":index==Mix?"%":"",8);return 1;
 case 7:if(index>=0&&index<static_cast<int>(parameterCount)){char b[24];std::snprintf(b,sizeof(b),"%.2f",p->values[index].load());text(ptr,b,8);return 1;}return 0;
 case 8:{const char* names[]={"Gain","Peak","HiFreq","Mix","Trim","VU","Mode","Analog","Bypass"};if(index>=0&&index<9){text(ptr,names[index],8);return 1;}return 0;}
 case 10:if(std::isfinite(opt)&&opt>=8000&&opt<=384000){p->rate=opt;p->dsp.setValues(p->snapshot());p->dsp.prepare(p->rate);}return 1;
 case 11:return 1;
 case 12:if(value){p->dsp.setValues(p->snapshot());p->dsp.prepare(p->rate);}return 1;
 case 13:if(ptr){*static_cast<void**>(ptr)=&p->rect;return 1;}return 0;
 case 14:return p->openEditor(ptr)?1:0;
 case 15:p->closeEditor();return 1;
 case 19:return 0;
 case 23:p->chunk=p->snapshot();if(ptr)*static_cast<void**>(ptr)=p->chunk.data();return sizeof(Values);
 case 24:if(ptr&&value==sizeof(Values)){Values incoming;std::memcpy(incoming.data(),ptr,sizeof(incoming));for(std::size_t i=0;i<parameterCount;++i)p->values[i].store(clampValue(i,incoming[i]));return 1;}return 0;
 case 26:return index>=0&&index<9?1:0;
 case 29:if(value>=0&&value<static_cast<std::intptr_t>(std::size(nativePresets))){text(ptr,nativePresets[value].name,24);return 1;}return 0;
 case 35:return 1; // effect category
 case 44:p->values[Bypass].store(value?1.:0.);return 1;
 case 45:text(ptr,"CLA-3A Studio",32);return 1;
 case 47:text(ptr,"Aulamix",64);return 1;
 case 48:text(ptr,"CLA-3A Studio",64);return 1;
 case 49:return 106;
 case 51:return -1; // no optional MIDI/event capabilities
 case 52:return static_cast<std::intptr_t>(p->rate*3.);
 case 58:return 2400;
 case 71:case 72:return 1;
 case 77:return value==0||value==1?1:0;
 default:return 0;
 }
}
}
extern "C" __declspec(dllexport) Cla::Vst2::Effect* VSTPluginMain(Cla::Vst2::Host host){
 using namespace Cla;using namespace Cla::Vst2;
 if(!host||host(nullptr,1,0,0,nullptr,0)<2400)return nullptr;
 auto p=new Instance;p->host=host;p->select(0);p->dsp.setValues(p->snapshot());p->dsp.prepare(48000);
 auto& e=p->effect;e.magic=magic;e.dispatcher=dispatch;e.numPrograms=static_cast<int>(std::size(nativePresets));e.numParams=9;e.numInputs=e.numOutputs=2;e.flags=1|16|32|4096;e.ioRatio=1.;e.object=p;e.uniqueID=0x434c3341;e.version=106;
 e.setParameter=[](Effect* e,std::int32_t i,float v){if(i>=0&&i<9)self(e)->values[i].store(denormalize(i,v));};
 e.getParameter=[](Effect* e,std::int32_t i)->float{return i>=0&&i<9?static_cast<float>(normalize(i,self(e)->values[i].load())):0.f;};
 e.processReplacing=[](Effect* e,float** in,float** out,std::int32_t n){self(e)->audio(in,out,n);};e.process=[](Effect* e,float** in,float** out,std::int32_t n){self(e)->audio(in,out,n,true);};
 e.processDoubleReplacing=[](Effect* e,double** in,double** out,std::int32_t n){self(e)->audio(in,out,n);};return &e;
}
