#include "PluginEditor.h"
#include "StartupDiagnostics.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <shlwapi.h>
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
    StartupScope trace("Controller initialize");
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
    for(const auto& spec:specs)if(!getParameterObject(spec.id)){startupTrace("Missing registered parameter",E_UNEXPECTED);return kInternalError;}
    startupTrace("All 9 parameter IDs registered");
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){Values values;if(!readState(stream,values))return kResultFalse;for(std::size_t i=0;i<parameterCount;++i)setParamNormalized(specs[i].id,normalize(i,values[i]));return kResultOk;}
IPlugView* PLUGIN_API Controller::createView(FIDString name){StartupScope trace("Controller createView");return name&&std::strcmp(name,ViewType::kEditor)==0?new Editor(this):nullptr;}
Values Controller::currentValues(){Values values;for(std::size_t i=0;i<parameterCount;++i)values[i]=denormalize(i,getParamNormalized(specs[i].id));return values;}
void Controller::acceptValues(const Values& values){for(std::size_t i=0;i<parameterCount;++i){const auto value=normalize(i,values[i]);if(std::abs(getParamNormalized(specs[i].id)-value)<1.e-9)continue;beginEdit(specs[i].id);setParamNormalized(specs[i].id,value);performEdit(specs[i].id,value);endEdit(specs[i].id);}}
void Controller::requestMeters(){if(auto message=owned(allocateMessage())){message->setMessageID("meter");sendMessage(message);}}
tresult PLUGIN_API Controller::notify(IMessage* message){if(message&&std::strcmp(message->getMessageID(),"meter")==0){const char* names[]={"input","output","reduction"};for(int i=0;i<3;++i){double value=0.;if(message->getAttributes()->getFloat(names[i],value)==kResultOk&&std::isfinite(value))meters[i].store(value,std::memory_order_relaxed);}return kResultOk;}return EditController::notify(message);}

constexpr UINT startBrowserMessage=WM_APP+71;
constexpr UINT navigateBrowserMessage=WM_APP+72;
struct EditorState : std::enable_shared_from_this<EditorState> {
    bool active=true,ready=false,comInitialized=false,failed=false;
    HWND window=nullptr;
    Controller* owner=nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> browserController;
    ComPtr<ICoreWebView2> browser;
    std::wstring navigationUrl;
    ULONGLONG started=GetTickCount64();
    std::string lastStage="editor window";
    std::wstring status=L"CLA-3A Studio 0.1.3: loading interface...";
    void stage(const char* name){
        lastStage=name;startupTrace(name);
        if(!ready&&!failed){status=L"CLA-3A Studio 0.1.3: loading interface...\nStage: "+std::wstring(lastStage.begin(),lastStage.end());if(window)InvalidateRect(window,nullptr,TRUE);}
    }
    void fail(const wchar_t* stage,HRESULT error){
        startupTrace(std::string(stage,stage+wcslen(stage)).c_str(),error);
        std::wostringstream text;text<<L"CLA-3A Studio: "<<stage<<L" (0x"<<std::hex<<static_cast<unsigned long>(error)<<L").\n"
            <<L"Check the WebView2 Runtime and the complete VST3 Resources folder.\nStartup log: "<<startupLogPath();
        ready=false;failed=true;status=text.str();if(browserController)browserController->put_IsVisible(FALSE);
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
        if(input==L"ready"){stage("JavaScript ready received");failed=false;ready=true;KillTimer(window,2);if(browserController)browserController->put_IsVisible(TRUE);frame();return;}
        if(input.rfind(L"diagnostic:",0)==0){
            const auto detail=input.substr(11,240);const std::string ascii(detail.begin(),detail.end());stage(ascii.c_str());return;
        }
        if(input.rfind(L"startup-error:",0)==0){
            const auto detail=input.substr(14,240);const std::string ascii(detail.begin(),detail.end());startupTrace(ascii.c_str(),E_FAIL);fail(L"JavaScript or resource loading failed; see startup log",E_FAIL);return;
        }
        if(input.rfind(L"params:",0)!=0)return;
        std::wistringstream reader(input.substr(7));reader.imbue(std::locale::classic());Values values{};
        for(std::size_t i=0;i<parameterCount;++i){if(!(reader>>values[i])||!std::isfinite(values[i]))return;if(i+1<parameterCount){wchar_t comma=0;if(!(reader>>comma)||comma!=L',')return;}}
        owner->acceptValues(values);
    }
};
static void startBrowser(const std::shared_ptr<EditorState>& state);
static LRESULT CALLBACK windowProc(HWND window,UINT msg,WPARAM w,LPARAM l){
    auto state=reinterpret_cast<EditorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(l);state=static_cast<EditorState*>(create->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
    if(msg==startBrowserMessage&&state&&state->active){startBrowser(state->shared_from_this());return 0;}
    if(msg==navigateBrowserMessage&&state&&state->active&&state->browser){
        state->stage("Navigate embedded file interface call");
        const auto navigation=state->browser->Navigate(state->navigationUrl.c_str());
        startupTrace("Navigate embedded file interface returned",navigation);
        if(FAILED(navigation))state->fail(L"Cannot load offline interface",navigation);
        return 0;
    }
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
            startupTrace(("Timeout waiting for JavaScript ready; last="+state->lastStage).c_str(),HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            state->status=L"CLA-3A Studio 0.1.3: loading timed out.\nLast stage: "+std::wstring(state->lastStage.begin(),state->lastStage.end())+L"\nStartup log: "+startupLogPath();
            if(state->browserController)state->browserController->put_IsVisible(FALSE);
            KillTimer(window,2);InvalidateRect(window,nullptr,TRUE);
        }
        return 0;
    }
    if(msg==WM_SIZE&&state&&state->browserController){RECT bounds;GetClientRect(window,&bounds);state->browserController->put_Bounds(bounds);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
static std::filesystem::path webDirectory(){wchar_t file[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),file,32768);return std::filesystem::path(file).parent_path().parent_path()/L"Resources"/L"web";}
static void startBrowser(const std::shared_ptr<EditorState>& state){
    StartupScope trace("startBrowser");
    std::weak_ptr<EditorState> weak=state;
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    // Use a fresh versioned profile, stable per host executable (not per instance).
    // Two instances share it; unrelated DAWs do not share a running browser profile.
    wchar_t host[32768]{};GetModuleFileNameW(nullptr,host,32768);
    const auto hostName=std::filesystem::path(host).stem().wstring();
    const auto folder=(std::filesystem::path(temp)/L"CLA3AStudio-WebView2-0.1.3"/hostName).wstring();
    std::error_code profileError;std::filesystem::create_directories(folder,profileError);
    if(profileError){state->fail(L"Cannot create WebView2 profile",HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));return;}
    auto options=Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    const auto optionsResult=options->put_AdditionalBrowserArguments(L"--disable-gpu");
    if(FAILED(optionsResult)){state->fail(L"Cannot configure software rendering",optionsResult);return;}
    startupTrace("Software rendering enabled; per-host profile 0.1.3");
    state->stage("CreateCoreWebView2Environment call");
    const auto startup=CreateCoreWebView2EnvironmentWithOptions(nullptr,folder.c_str(),options.Get(),
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([weak](HRESULT error,ICoreWebView2Environment* environment)->HRESULT{
        auto s=weak.lock();if(!s||!s->active)return S_OK;
        startupTrace("WebView2 environment callback",error);s->stage("environment callback");
        if(FAILED(error)||!environment){s->fail(L"WebView2 environment failed",FAILED(error)?error:E_FAIL);return S_OK;}
        s->environment=environment;
        LPWSTR version=nullptr;if(SUCCEEDED(environment->get_BrowserVersionString(&version))&&version){const std::wstring wideVersion(version);startupTrace(("WebView2 runtime version="+std::string(wideVersion.begin(),wideVersion.end())).c_str());CoTaskMemFree(version);}
        s->stage("CreateCoreWebView2Controller call (HWND attached)");
        const auto creation=environment->CreateCoreWebView2Controller(s->window,Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([weak](HRESULT result,ICoreWebView2Controller* controller)->HRESULT{
          auto s=weak.lock();if(!s||!s->active)return S_OK;startupTrace("WebView2 controller callback",result);s->stage("controller callback");if(FAILED(result)||!controller){s->fail(L"WebView2 controller failed",FAILED(result)?result:E_FAIL);return S_OK;}
          s->browserController=controller;const auto view=controller->get_CoreWebView2(&s->browser);if(FAILED(view)||!s->browser){s->fail(L"WebView2 interface unavailable",FAILED(view)?view:E_FAIL);return S_OK;}
          ComPtr<ICoreWebView2Controller3> pixels;
          if(SUCCEEDED(s->browserController.As(&pixels))){pixels->put_ShouldDetectMonitorScaleChanges(FALSE);pixels->put_RasterizationScale(1.0);pixels->put_BoundsMode(COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);}
          RECT bounds;GetClientRect(s->window,&bounds);controller->put_Bounds(bounds);controller->put_IsVisible(TRUE);
          ComPtr<ICoreWebView2Settings> settings;const auto settingsResult=s->browser->get_Settings(&settings);if(FAILED(settingsResult)||!settings){s->fail(L"WebView2 settings unavailable",FAILED(settingsResult)?settingsResult:E_FAIL);return S_OK;}
          const auto messaging=settings->put_IsWebMessageEnabled(TRUE);if(FAILED(messaging)){s->fail(L"WebView2 messaging unavailable",messaging);return S_OK;}settings->put_AreDevToolsEnabled(FALSE);settings->put_AreDefaultContextMenusEnabled(FALSE);settings->put_IsStatusBarEnabled(FALSE);settings->put_IsZoomControlEnabled(FALSE);
          ComPtr<ICoreWebView2_8> audio;if(SUCCEEDED(s->browser.As(&audio)))audio->put_IsMuted(TRUE);
          s->stage("checking self-contained Resources/web/native.html");
          const auto document=webDirectory()/L"native.html";std::error_code fileError;
          if(!std::filesystem::is_regular_file(document,fileError)){s->fail(L"Missing Resources/web/native.html",HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));return S_OK;}
          wchar_t url[65536]{};DWORD urlSize=65536;
          const auto conversion=UrlCreateFromPathW(document.c_str(),url,&urlSize,0);
          if(FAILED(conversion)){s->fail(L"Cannot resolve offline interface path",conversion);return S_OK;}
          s->navigationUrl=url;
          EventRegistrationToken token{};
          const auto handler=s->browser->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([weak](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT{auto s=weak.lock();if(!s||!s->active)return S_OK;LPWSTR text=nullptr;if(SUCCEEDED(args->TryGetWebMessageAsString(&text))&&text){s->message(text);CoTaskMemFree(text);}return S_OK;}).Get(),&token);
          if(FAILED(handler)){s->fail(L"Cannot register JavaScript bridge",handler);return S_OK;}
          s->browser->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT{
            auto s=weak.lock();if(!s||!s->active)return S_OK;BOOL success=FALSE;args->get_IsSuccess(&success);
            s->stage(success?"navigation complete; waiting for JS ready":"navigation failed");
            if(success&&!s->failed&&s->browserController)s->browserController->put_IsVisible(TRUE);
            if(!success){COREWEBVIEW2_WEB_ERROR_STATUS status;args->get_WebErrorStatus(&status);s->fail(L"Interface navigation failed",static_cast<HRESULT>(status));}return S_OK;
          }).Get(),&token);
          s->browser->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([weak](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs* args)->HRESULT{
            auto s=weak.lock();if(s&&s->active){COREWEBVIEW2_PROCESS_FAILED_KIND kind{};args->get_ProcessFailedKind(&kind);startupTrace(("WebView2 process failure kind="+std::to_string(static_cast<int>(kind))).c_str(),E_FAIL);s->fail(L"WebView2 process failed; see startup log",E_FAIL);}return S_OK;
          }).Get(),&token);
          const auto starting=s->browser->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs*)->HRESULT{
            auto s=weak.lock();if(s&&s->active)s->stage("NavigationStarting: offline file");return S_OK;
          }).Get(),&token);
          if(FAILED(starting)){s->fail(L"Cannot register navigation diagnostics",starting);return S_OK;}
          s->stage("offline navigation queued after controller callback");
          if(!PostMessageW(s->window,navigateBrowserMessage,0,0)){s->fail(L"Cannot queue navigation",HRESULT_FROM_WIN32(GetLastError()));return S_OK;}
          SetTimer(s->window,1,40,nullptr);return S_OK;
        }).Get());startupTrace("CreateCoreWebView2Controller returned",creation);if(FAILED(creation))s->fail(L"Cannot create WebView2 controller",creation);return S_OK;
      }).Get());
    startupTrace("CreateCoreWebView2Environment returned",startup);
    if(FAILED(startup))state->fail(L"Cannot start WebView2",startup);
}
Editor::Editor(Controller* controller):owner(controller),state(std::make_shared<EditorState>()){StartupScope trace("Editor constructor");owner->addRef();state->owner=owner;rect=ViewRect(0,0,640,390);}
Editor::~Editor(){removed();owner->release();}
tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type){return type&&std::strcmp(type,kPlatformTypeHWND)==0?kResultTrue:kResultFalse;}
tresult PLUGIN_API Editor::attached(void* parent,FIDString type){
    StartupScope trace("Editor attached");
    if(!parent||isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
    if(state->window)removed();state=std::make_shared<EditorState>();state->owner=owner;
    auto result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);startupTrace("CoInitializeEx STA",result);state->comInitialized=SUCCEEDED(result);
    WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=static_cast<HINSTANCE>(moduleHandle);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.lpszClassName=L"CLA3AStudioWebView";RegisterClassW(&cls);
    state->window=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,640,390,static_cast<HWND>(parent),nullptr,cls.hInstance,state.get());
    if(!state->window){startupTrace("CreateWindowEx failed",HRESULT_FROM_WIN32(GetLastError()));return kResultFalse;}systemWindow=parent;state->stage("HWND attached");
    if(FAILED(result)){state->fail(L"Host UI thread is incompatible with WebView2 COM initialization",result);return kResultTrue;}
    SetTimer(state->window,2,1000,nullptr);
    if(!PostMessageW(state->window,startBrowserMessage,0,0))state->fail(L"Cannot queue browser startup",HRESULT_FROM_WIN32(GetLastError()));
    return kResultTrue;
}
tresult PLUGIN_API Editor::removed(){
    StartupScope trace("Editor removed");
    if(state){state->active=false;state->owner=nullptr;if(state->browserController)state->browserController->Close();if(state->window){KillTimer(state->window,1);KillTimer(state->window,2);DestroyWindow(state->window);state->window=nullptr;}
        state->browser.Reset();state->browserController.Reset();state->environment.Reset();if(state->comInitialized){CoUninitialize();state->comInitialized=false;}}
    systemWindow=nullptr;return kResultTrue;
}
tresult PLUGIN_API Editor::onSize(ViewRect* size){if(!size)return kInvalidArgument;rect=*size;if(state->window)MoveWindow(state->window,0,0,size->getWidth(),size->getHeight(),TRUE);return kResultTrue;}
tresult PLUGIN_API Editor::checkSizeConstraint(ViewRect* size){if(!size)return kInvalidArgument;size->right=size->left+640;size->bottom=size->top+390;return kResultTrue;}
}
