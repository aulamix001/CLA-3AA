#include "PluginEditor.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <filesystem>
#include <sstream>
#include <locale>
#include <cstring>
#include <cstdlib>

extern void* moduleHandle;
using namespace Steinberg;
using namespace Steinberg::Vst;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;

namespace Cla {
tresult PLUGIN_API Controller::initialize(FUnknown* context){
    auto result=EditController::initialize(context);if(result!=kResultOk)return result;
    const TChar* titles[]={STR16("GAIN"),STR16("PEAK REDUCTION"),STR16("HI FREQ"),STR16("MIX"),STR16("TRIM")};
    for(std::size_t i=0;i<5;++i){auto p=new RangeParameter(titles[i],specs[i].id,i==Trim?STR16("dB"):i==Mix?STR16("%"):nullptr,specs[i].min,specs[i].max,specs[i].initial);p->setPrecision(i<2?2:1);parameters.addParameter(p);}
    const TChar* meter[]={STR16("IN"),STR16("GR"),STR16("OUT")};
    const TChar* mode[]={STR16("COMP"),STR16("LIMITER")};
    const TChar* analog[]={STR16("50Hz"),STR16("60Hz"),STR16("OFF")};
    for(std::size_t i=Meter;i<=Analog;++i){auto p=new StringListParameter(i==Meter?STR16("VU DISPLAY"):i==Mode?STR16("COMP/LIMITER"):STR16("ANALOG"),specs[i].id);
        const auto count=static_cast<int>(specs[i].max+1);for(int n=0;n<count;++n)p->appendString(i==Meter?meter[n]:i==Mode?mode[n]:analog[n]);
        p->getInfo().defaultNormalizedValue=normalize(i,specs[i].initial);p->setNormalized(p->getInfo().defaultNormalizedValue);parameters.addParameter(p);
    }
    parameters.addParameter(STR16("Bypass"),nullptr,1,0.,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass,specs[Bypass].id);
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){Values values;if(!readState(stream,values))return kResultFalse;for(std::size_t i=0;i<parameterCount;++i)setParamNormalized(specs[i].id,normalize(i,values[i]));return kResultOk;}
IPlugView* PLUGIN_API Controller::createView(FIDString name){return name&&std::strcmp(name,ViewType::kEditor)==0?new Editor(this):nullptr;}
Values Controller::currentValues(){Values values;for(std::size_t i=0;i<parameterCount;++i)values[i]=denormalize(i,getParamNormalized(specs[i].id));return values;}
void Controller::acceptValues(const Values& values){for(std::size_t i=0;i<parameterCount;++i){const auto value=normalize(i,values[i]);if(std::abs(getParamNormalized(specs[i].id)-value)<1.e-9)continue;beginEdit(specs[i].id);setParamNormalized(specs[i].id,value);performEdit(specs[i].id,value);endEdit(specs[i].id);}}
void Controller::requestMeters(){if(auto message=owned(allocateMessage())){message->setMessageID("meter");sendMessage(message);}}
tresult PLUGIN_API Controller::notify(IMessage* message){if(message&&std::strcmp(message->getMessageID(),"meter")==0){const char* names[]={"input","output","reduction"};for(int i=0;i<3;++i){double value=0.;if(message->getAttributes()->getFloat(names[i],value)==kResultOk&&std::isfinite(value))meters[i].store(value,std::memory_order_relaxed);}return kResultOk;}return EditController::notify(message);}

struct EditorState {
    bool active=true,ready=false,comInitialized=false;
    HWND window=nullptr;
    Controller* owner=nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> browserController;
    ComPtr<ICoreWebView2> browser;
    ULONGLONG started=GetTickCount64();
    std::wstring status=L"CLA-3A Studio: loading interface...";
    void fail(const wchar_t* stage,HRESULT error){
        std::wostringstream text;text<<L"CLA-3A Studio: "<<stage<<L" (0x"<<std::hex<<static_cast<unsigned long>(error)<<L").\n"
            <<L"Check the WebView2 Runtime and the complete VST3 Resources folder.";
        ready=false;status=text.str();if(browserController)browserController->put_IsVisible(FALSE);
        if(window){KillTimer(window,2);InvalidateRect(window,nullptr,TRUE);}
        OutputDebugStringW(status.c_str());
    }
    void frame(){
        if(!active||!ready||!browser||!owner)return;
        owner->requestMeters();
        const auto values=owner->currentValues();
        std::ostringstream message;message.imbue(std::locale::classic());message.precision(12);
        message<<"{\"type\":\"native-state\",\"values\":[";
        for(std::size_t i=0;i<parameterCount;++i){if(i)message<<',';message<<values[i];}message<<"],\"meters\":[";
        for(std::size_t i=0;i<3;++i){if(i)message<<',';message<<owner->meters[i].load(std::memory_order_relaxed);}message<<"]}";
        const std::string ascii=message.str();std::wstring wide(ascii.begin(),ascii.end());browser->PostWebMessageAsJson(wide.c_str());
    }
    void message(const std::wstring& input){
        if(!active||!owner)return;
        if(input==L"ready"){ready=true;KillTimer(window,2);if(browserController)browserController->put_IsVisible(TRUE);frame();return;}
        if(input.rfind(L"params:",0)!=0)return;
        std::wistringstream reader(input.substr(7));reader.imbue(std::locale::classic());Values values{};
        for(std::size_t i=0;i<parameterCount;++i){if(!(reader>>values[i])||!std::isfinite(values[i]))return;if(i+1<parameterCount){wchar_t comma=0;if(!(reader>>comma)||comma!=L',')return;}}
        owner->acceptValues(values);
    }
};
static LRESULT CALLBACK windowProc(HWND window,UINT msg,WPARAM w,LPARAM l){
    auto state=reinterpret_cast<EditorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(l);state=static_cast<EditorState*>(create->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
    if(msg==WM_PAINT&&state){
        PAINTSTRUCT paint;auto dc=BeginPaint(window,&paint);RECT area;GetClientRect(window,&area);
        FillRect(dc,&area,reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(220,220,210));InflateRect(&area,-24,-24);
        DrawTextW(dc,state->status.c_str(),-1,&area,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        EndPaint(window,&paint);return 0;
    }
    if(msg==WM_TIMER&&state){
        if(w==1)state->frame();
        if(w==2&&!state->ready&&GetTickCount64()-state->started>15000){
            state->status=L"CLA-3A Studio: WebView2 is taking longer than expected.\nCheck Microsoft Edge WebView2 Runtime. Close and reopen the editor if loading does not finish.";
            KillTimer(window,2);InvalidateRect(window,nullptr,TRUE);
        }
        return 0;
    }
    if(msg==WM_SIZE&&state&&state->browserController){RECT bounds;GetClientRect(window,&bounds);state->browserController->put_Bounds(bounds);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
static std::filesystem::path webDirectory(){wchar_t file[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),file,32768);return std::filesystem::path(file).parent_path().parent_path()/L"Resources"/L"web";}
static void startBrowser(const std::shared_ptr<EditorState>& state){
    std::weak_ptr<EditorState> weak=state;
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    const auto folder=(std::filesystem::path(temp)/L"CLA3AStudio-WebView2").wstring();
    const auto startup=CreateCoreWebView2EnvironmentWithOptions(nullptr,folder.c_str(),nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([weak](HRESULT error,ICoreWebView2Environment* environment)->HRESULT{
        auto s=weak.lock();if(!s||!s->active)return S_OK;
        if(FAILED(error)||!environment){s->fail(L"WebView2 environment failed",FAILED(error)?error:E_FAIL);return S_OK;}
        s->environment=environment;
        const auto creation=environment->CreateCoreWebView2Controller(s->window,Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([weak](HRESULT result,ICoreWebView2Controller* controller)->HRESULT{
          auto s=weak.lock();if(!s||!s->active)return S_OK;if(FAILED(result)||!controller){s->fail(L"WebView2 controller failed",FAILED(result)?result:E_FAIL);return S_OK;}
          s->browserController=controller;const auto view=controller->get_CoreWebView2(&s->browser);if(FAILED(view)||!s->browser){s->fail(L"WebView2 interface unavailable",FAILED(view)?view:E_FAIL);return S_OK;}
          ComPtr<ICoreWebView2Controller3> pixels;
          if(SUCCEEDED(s->browserController.As(&pixels))){pixels->put_ShouldDetectMonitorScaleChanges(FALSE);pixels->put_RasterizationScale(1.0);pixels->put_BoundsMode(COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);}
          RECT bounds;GetClientRect(s->window,&bounds);controller->put_Bounds(bounds);controller->put_IsVisible(FALSE);
          ComPtr<ICoreWebView2Settings> settings;s->browser->get_Settings(&settings);settings->put_AreDevToolsEnabled(FALSE);settings->put_AreDefaultContextMenusEnabled(FALSE);settings->put_IsStatusBarEnabled(FALSE);settings->put_IsZoomControlEnabled(FALSE);
          ComPtr<ICoreWebView2_8> audio;if(SUCCEEDED(s->browser.As(&audio)))audio->put_IsMuted(TRUE);
          ComPtr<ICoreWebView2_3> resources;if(FAILED(s->browser.As(&resources))){s->fail(L"Update WebView2 Runtime",E_NOINTERFACE);return S_OK;}
          auto directory=webDirectory();std::error_code fileError;
          if(!std::filesystem::is_regular_file(directory/L"index.html",fileError)){s->fail(L"Missing Resources/web/index.html",HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));return S_OK;}
          auto folder=directory.wstring();const auto mapping=resources->SetVirtualHostNameToFolderMapping(L"cla3a.local",folder.c_str(),COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
          if(FAILED(mapping)){s->fail(L"Local interface mapping failed",mapping);return S_OK;}
          EventRegistrationToken token{};
          s->browser->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([weak](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT{auto s=weak.lock();if(!s||!s->active)return S_OK;LPWSTR text=nullptr;if(SUCCEEDED(args->TryGetWebMessageAsString(&text))&&text){s->message(text);CoTaskMemFree(text);}return S_OK;}).Get(),&token);
          s->browser->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT{
            auto s=weak.lock();if(!s||!s->active)return S_OK;BOOL success=FALSE;args->get_IsSuccess(&success);
            if(!success){COREWEBVIEW2_WEB_ERROR_STATUS status;args->get_WebErrorStatus(&status);s->fail(L"Interface navigation failed",static_cast<HRESULT>(status));}return S_OK;
          }).Get(),&token);
          s->browser->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([weak](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*)->HRESULT{
            auto s=weak.lock();if(s&&s->active)s->fail(L"WebView2 process failed; reopen the editor",E_FAIL);return S_OK;
          }).Get(),&token);
          const auto navigation=s->browser->Navigate(L"https://cla3a.local/index.html");
          if(FAILED(navigation)){s->fail(L"Cannot load interface",navigation);return S_OK;}
          SetTimer(s->window,1,40,nullptr);return S_OK;
        }).Get());if(FAILED(creation))s->fail(L"Cannot create WebView2 controller",creation);return S_OK;
      }).Get());
    if(FAILED(startup))state->fail(L"Cannot start WebView2",startup);
}
Editor::Editor(Controller* controller):owner(controller),state(std::make_shared<EditorState>()){owner->addRef();state->owner=owner;rect=ViewRect(0,0,640,390);}
Editor::~Editor(){removed();owner->release();}
tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type){return type&&std::strcmp(type,kPlatformTypeHWND)==0?kResultTrue:kResultFalse;}
tresult PLUGIN_API Editor::attached(void* parent,FIDString type){
    if(!parent||isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
    if(state->window)removed();state=std::make_shared<EditorState>();state->owner=owner;
    auto result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);state->comInitialized=SUCCEEDED(result);
    WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=static_cast<HINSTANCE>(moduleHandle);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.lpszClassName=L"CLA3AStudioWebView";RegisterClassW(&cls);
    state->window=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,640,390,static_cast<HWND>(parent),nullptr,cls.hInstance,state.get());
    if(!state->window)return kResultFalse;systemWindow=parent;
    if(FAILED(result)){state->fail(L"Host UI thread is incompatible with WebView2 COM initialization",result);return kResultTrue;}
    SetTimer(state->window,2,1000,nullptr);startBrowser(state);return kResultTrue;
}
tresult PLUGIN_API Editor::removed(){
    if(state){state->active=false;state->owner=nullptr;if(state->window){KillTimer(state->window,1);KillTimer(state->window,2);DestroyWindow(state->window);state->window=nullptr;}
        if(state->browserController)state->browserController->Close();state->browser.Reset();state->browserController.Reset();state->environment.Reset();if(state->comInitialized){CoUninitialize();state->comInitialized=false;}}
    systemWindow=nullptr;return kResultTrue;
}
tresult PLUGIN_API Editor::onSize(ViewRect* size){if(!size)return kInvalidArgument;rect=*size;if(state->window)MoveWindow(state->window,0,0,size->getWidth(),size->getHeight(),TRUE);return kResultTrue;}
tresult PLUGIN_API Editor::checkSizeConstraint(ViewRect* size){if(!size)return kInvalidArgument;size->right=size->left+640;size->bottom=size->top+390;return kResultTrue;}
}
