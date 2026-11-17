#include "PluginEditor.h"
#include "StartupDiagnostics.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <shlwapi.h>
#include <commctrl.h>
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
    bool fallback=false;
    std::array<HWND,9> fallbackControls{};
    HWND fallbackButton=nullptr,fallbackMeters=nullptr;
    void enableFallback();
    void syncFallback();
    void changeFallback(int id);
    bool active=true,ready=false,painted=false,comInitialized=false,failed=false,timedOut=false;
    HWND window=nullptr,cover=nullptr;
    Controller* owner=nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> browserController;
    ComPtr<ICoreWebView2> browser;
    std::wstring navigationUrl;
    ULONGLONG started=GetTickCount64();
    std::string lastStage="editor window";
    std::wstring status=L"CLA-3A Studio 0.1.5: loading interface...";
    void stage(const char* name){
        lastStage=name;startupTrace(name);
        if(!ready&&!failed){status=L"CLA-3A Studio 0.1.5: loading interface...\nStage: "+std::wstring(lastStage.begin(),lastStage.end());if(window)InvalidateRect(window,nullptr,TRUE);}
    }
    void fail(const wchar_t* stage,HRESULT error){
        startupTrace(std::string(stage,stage+wcslen(stage)).c_str(),error);
        std::wostringstream text;text<<L"CLA-3A Studio: "<<stage<<L" (0x"<<std::hex<<static_cast<unsigned long>(error)<<L").\n"
            <<L"Check the WebView2 Runtime and the complete VST3 Resources folder.\nStartup log: "<<startupLogPath();
        ready=false;painted=false;failed=true;status=text.str();if(cover){ShowWindow(cover,SW_SHOW);SetWindowPos(cover,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);InvalidateRect(cover,nullptr,FALSE);}if(browserController)browserController->put_IsVisible(FALSE);
        if(window){KillTimer(window,2);InvalidateRect(window,nullptr,TRUE);}
        OutputDebugStringW(status.c_str());enableFallback();
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
        if(input==L"ready"){stage("JavaScript ready received");failed=false;ready=true;if(browserController)browserController->put_IsVisible(TRUE);frame();return;}
        if(input==L"painted"&&ready&&!failed){
            stage("First painted frame revealed");painted=true;timedOut=false;
            KillTimer(window,2);if(!fallback)KillTimer(window,3);
            if(cover&&!fallback)ShowWindow(cover,SW_HIDE);
            if(fallbackButton&&fallback)SetWindowTextW(fallbackButton,L"Voltar à interface gráfica");
            return;
        }
        if(input.rfind(L"console-error:",0)==0||input.rfind(L"console-warn:",0)==0){
            const auto bounded=input.substr(0,260);startupTrace(std::string(bounded.begin(),bounded.end()).c_str());return;
        }
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
void EditorState::enableFallback(){
    if(!cover||!owner)return;
    fallback=true;startupTrace("Native parameter fallback enabled");ShowWindow(cover,SW_SHOW);
    SetWindowPos(cover,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetWindowTextW(fallbackButton,painted?L"Voltar à interface gráfica":L"Controles nativos ativos");
    MoveWindow(fallbackButton,355,298,255,27,TRUE);
    if(!fallbackControls[0]){
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);
        const wchar_t* names[]={L"GAIN",L"PEAK REDUCTION",L"HI FREQ",L"MIX",L"TRIM"};
        for(int i=0;i<5;++i){
            auto label=CreateWindowExW(0,L"STATIC",names[i],WS_CHILD|WS_VISIBLE,24,80+i*36,150,24,cover,nullptr,nullptr,nullptr);
            SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            fallbackControls[i]=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,180,76+i*36,425,30,cover,reinterpret_cast<HMENU>(8100+i),nullptr,nullptr);
            SendMessageW(fallbackControls[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,i==Trim?360:1000));
        }
        const wchar_t* choices[3][3]={{L"VU: IN",L"VU: GR",L"VU: OUT"},{L"COMP",L"LIMITER",nullptr},{L"ANALOG: 50 Hz",L"ANALOG: 60 Hz",L"ANALOG: OFF"}};
        for(int i=5;i<8;++i){
            fallbackControls[i]=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,24+(i-5)*194,264,180,150,cover,reinterpret_cast<HMENU>(8100+i),nullptr,nullptr);
            for(int n=0;n<3&&choices[i-5][n];++n)SendMessageW(fallbackControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(choices[i-5][n]));
        }
        fallbackControls[Bypass]=CreateWindowExW(0,L"BUTTON",L"Bypass",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,24,302,110,24,cover,reinterpret_cast<HMENU>(8100+Bypass),nullptr,nullptr);
        fallbackMeters=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,145,300,205,35,cover,nullptr,nullptr,nullptr);
        for(auto control:fallbackControls)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
        SendMessageW(fallbackMeters,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    }
    SetTimer(window,3,250,nullptr);syncFallback();InvalidateRect(cover,nullptr,FALSE);
}
void EditorState::syncFallback(){
    if(!fallback||!owner||!fallbackControls[0])return;
    const auto values=owner->currentValues();
    for(int i=0;i<5;++i)SendMessageW(fallbackControls[i],TBM_SETPOS,TRUE,static_cast<LPARAM>(std::lround(normalize(i,values[i])*(i==Trim?360:1000))));
    for(int i=5;i<8;++i)SendMessageW(fallbackControls[i],CB_SETCURSEL,static_cast<WPARAM>(values[i]),0);
    SendMessageW(fallbackControls[Bypass],BM_SETCHECK,values[Bypass]>=.5?BST_CHECKED:BST_UNCHECKED,0);
    owner->requestMeters();std::wostringstream meter;meter.precision(1);meter<<std::fixed;
    const auto mode=static_cast<int>(values[Meter]);meter<<(mode==0?L"IN: ":mode==1?L"GR: ":L"OUT: ")<<owner->meters[mode==0?0:mode==1?2:1].load()<<L" dB";
    SetWindowTextW(fallbackMeters,meter.str().c_str());
}
void EditorState::changeFallback(int id){
    if(!owner||id<8100||id>=8100+static_cast<int>(parameterCount))return;
    const int i=id-8100;auto values=owner->currentValues();
    if(i<5)values[i]=denormalize(i,static_cast<double>(SendMessageW(fallbackControls[i],TBM_GETPOS,0,0))/(i==Trim?360.:1000.));
    else if(i<8){const auto selection=SendMessageW(fallbackControls[i],CB_GETCURSEL,0,0);if(selection==CB_ERR)return;values[i]=static_cast<double>(selection);}
    else values[i]=SendMessageW(fallbackControls[i],BM_GETCHECK,0,0)==BST_CHECKED?1.:0.;
    owner->acceptValues(values);syncFallback();
}
static void drawLoadingScreen(HWND window,HDC target,EditorState* state){
    RECT bounds;GetClientRect(window,&bounds);const int width=bounds.right,height=bounds.bottom;
    if(width<=0||height<=0)return;
    auto dc=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,width,height);
    if(!dc||!bitmap){if(dc)DeleteDC(dc);if(bitmap)DeleteObject(bitmap);return;}
    auto oldBitmap=SelectObject(dc,bitmap);
    for(int y=0;y<height;++y){
        const int shade=29+(height-y)*12/height+((y*17)%5==0?2:0);
        RECT row{0,y,width,y+1};SetDCBrushColor(dc,RGB(shade,shade,shade));FillRect(dc,&row,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    SetBkMode(dc,TRANSPARENT);
    auto title=CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto body=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto oldFont=SelectObject(dc,title);SetTextColor(dc,RGB(217,216,207));
    RECT text{20,state->fallback?8:height/2-65,width-20,state->fallback?48:height/2-20};DrawTextW(dc,L"CLA-3A STUDIO",-1,&text,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,body);SetTextColor(dc,RGB(175,173,157));
    text={24,state->fallback?45:height/2-10,width-24,state->fallback?72:height/2+24};
    DrawTextW(dc,state->fallback?L"Controles nativos — áudio e automação ativos":state->failed?L"Falha ao abrir a interface":state->timedOut?L"A interface está demorando mais que o esperado":L"Preparando a interface...",-1,&text,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    if(state->fallback){
        text={24,height-50,width-24,height-12};DrawTextW(dc,L"Logs: %TEMP%\\CLA3AStudio-startup-<PID>.log",-1,&text,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }else if(state->failed||state->timedOut){
        text={24,height/2+36,width-24,height-14};DrawTextW(dc,state->status.c_str(),-1,&text,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }else{
        const int left=width/2-110,top=height/2+42;
        RECT track{left,top,left+220,top+3};SetDCBrushColor(dc,RGB(66,65,59));FillRect(dc,&track,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        const int offset=static_cast<int>((GetTickCount64()-state->started)/12%160);
        RECT progress{left+offset,top,left+offset+60,top+3};SetDCBrushColor(dc,RGB(188,174,104));FillRect(dc,&progress,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    SelectObject(dc,oldFont);DeleteObject(title);DeleteObject(body);
    BitBlt(target,0,0,width,height,dc,0,0,SRCCOPY);SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);
}
static LRESULT CALLBACK loadingWindowProc(HWND window,UINT msg,WPARAM w,LPARAM l){
    auto state=reinterpret_cast<EditorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(l);state=static_cast<EditorState*>(create->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
    if(msg==WM_COMMAND&&state){
        const int id=LOWORD(w);
        if(id==8001){
            if(state->fallback&&state->painted){state->fallback=false;ShowWindow(state->cover,SW_HIDE);KillTimer(state->window,3);}
            else state->enableFallback();
        }else if(state->fallback)state->changeFallback(id);
        return 0;
    }
    if(msg==WM_HSCROLL&&state&&state->fallback){state->changeFallback(GetDlgCtrlID(reinterpret_cast<HWND>(l)));return 0;}
    if(msg==WM_CTLCOLORSTATIC){SetTextColor(reinterpret_cast<HDC>(w),RGB(225,225,215));SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PRINTCLIENT&&state){drawLoadingScreen(window,reinterpret_cast<HDC>(w),state);return 0;}
    if(msg==WM_PAINT&&state){PAINTSTRUCT paint;auto dc=BeginPaint(window,&paint);drawLoadingScreen(window,dc,state);EndPaint(window,&paint);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
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
        PAINTSTRUCT paint;auto dc=BeginPaint(window,&paint);drawLoadingScreen(window,dc,state);EndPaint(window,&paint);return 0;
    }
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_TIMER&&state){
        if(w==1)state->frame();
        if(w==3&&state->fallback)state->syncFallback();
        if(w==3&&(!state->painted||state->fallback)&&state->cover){SetWindowPos(state->cover,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);InvalidateRect(state->cover,nullptr,FALSE);}
        if(w==2&&!state->painted&&GetTickCount64()-state->started>15000){
            startupTrace(("Timeout waiting for first painted frame; last="+state->lastStage).c_str(),HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            state->status=L"CLA-3A Studio 0.1.5: loading timed out.\nLast stage: "+std::wstring(state->lastStage.begin(),state->lastStage.end())+L"\nStartup log: "+startupLogPath();
            state->timedOut=true;state->enableFallback();
            KillTimer(window,2);if(state->cover)InvalidateRect(state->cover,nullptr,FALSE);InvalidateRect(window,nullptr,TRUE);
        }
        return 0;
    }
    if(msg==WM_SIZE&&state){RECT bounds;GetClientRect(window,&bounds);if(state->browserController)state->browserController->put_Bounds(bounds);if(state->cover)MoveWindow(state->cover,0,0,bounds.right,bounds.bottom,TRUE);return 0;}
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
    LPWSTR installedVersion=nullptr;const auto runtime=GetAvailableCoreWebView2BrowserVersionString(nullptr,&installedVersion);
    startupTrace("WebView2 runtime discovery",runtime);if(installedVersion)CoTaskMemFree(installedVersion);
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
          RECT bounds;GetClientRect(s->window,&bounds);controller->put_IsVisible(FALSE);controller->put_Bounds(bounds);
          ComPtr<ICoreWebView2Controller2> background;
          if(SUCCEEDED(s->browserController.As(&background))){COREWEBVIEW2_COLOR transparent{0,0,0,0};background->put_DefaultBackgroundColor(transparent);}
          controller->put_IsVisible(TRUE);
          if(s->cover)SetWindowPos(s->cover,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
          ComPtr<ICoreWebView2Settings> settings;const auto settingsResult=s->browser->get_Settings(&settings);if(FAILED(settingsResult)||!settings){s->fail(L"WebView2 settings unavailable",FAILED(settingsResult)?settingsResult:E_FAIL);return S_OK;}
          const auto messaging=settings->put_IsWebMessageEnabled(TRUE);if(FAILED(messaging)){s->fail(L"WebView2 messaging unavailable",messaging);return S_OK;}settings->put_AreDevToolsEnabled(FALSE);settings->put_AreDefaultContextMenusEnabled(FALSE);settings->put_IsStatusBarEnabled(FALSE);settings->put_IsZoomControlEnabled(FALSE);
          ComPtr<ICoreWebView2_8> audio;if(SUCCEEDED(s->browser.As(&audio)))audio->put_IsMuted(TRUE);
          s->stage("checking self-contained Resources/web/native.html");
          const auto document=webDirectory()/L"native.html";std::error_code fileError;
          const auto resolved=document.wstring();startupTrace(("Resolved native HTML: "+std::string(resolved.begin(),resolved.end())).c_str());
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
    WNDCLASSW loading{};loading.lpfnWndProc=loadingWindowProc;loading.hInstance=cls.hInstance;loading.hCursor=cls.hCursor;loading.lpszClassName=L"CLA3AStudioLoading";RegisterClassW(&loading);
    state->cover=CreateWindowExW(0,loading.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,0,0,640,390,state->window,nullptr,cls.hInstance,state.get());
    if(!state->cover){state->fail(L"Cannot create native loading screen",HRESULT_FROM_WIN32(GetLastError()));return kResultTrue;}
    state->fallbackButton=CreateWindowExW(0,L"BUTTON",L"Usar controles nativos",WS_CHILD|WS_VISIBLE|WS_TABSTOP,200,335,240,28,state->cover,reinterpret_cast<HMENU>(8001),cls.hInstance,nullptr);
    SendMessageW(state->fallbackButton,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    UpdateWindow(state->window);UpdateWindow(state->cover);startupTrace("Native loading screen painted before browser startup");
    SetTimer(state->window,3,80,nullptr);
    if(FAILED(result)){state->fail(L"Host UI thread is incompatible with WebView2 COM initialization",result);return kResultTrue;}
    SetTimer(state->window,2,1000,nullptr);
    if(!PostMessageW(state->window,startBrowserMessage,0,0))state->fail(L"Cannot queue browser startup",HRESULT_FROM_WIN32(GetLastError()));
    return kResultTrue;
}
tresult PLUGIN_API Editor::removed(){
    StartupScope trace("Editor removed");
    if(state){state->active=false;state->owner=nullptr;if(state->browserController)state->browserController->Close();if(state->window){KillTimer(state->window,1);KillTimer(state->window,2);KillTimer(state->window,3);DestroyWindow(state->window);state->window=nullptr;state->cover=nullptr;}
        state->browser.Reset();state->browserController.Reset();state->environment.Reset();if(state->comInitialized){CoUninitialize();state->comInitialized=false;}}
    systemWindow=nullptr;return kResultTrue;
}
tresult PLUGIN_API Editor::onSize(ViewRect* size){if(!size)return kInvalidArgument;rect=*size;if(state->window)MoveWindow(state->window,0,0,size->getWidth(),size->getHeight(),TRUE);return kResultTrue;}
tresult PLUGIN_API Editor::checkSizeConstraint(ViewRect* size){if(!size)return kInvalidArgument;size->right=size->left+640;size->bottom=size->top+390;return kResultTrue;}
}
