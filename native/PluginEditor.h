#pragma once
#include "PluginProcessor.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <memory>

namespace Cla {
class Editor;
class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* create(void*) {return static_cast<Steinberg::Vst::IEditController*>(new Controller);}
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    void acceptValues(const Values&);
    Values currentValues();
    void requestMeters();
    std::array<std::atomic<double>,3> meters {{-120.,-120.,0.}};
};
struct EditorState;
class Editor final : public Steinberg::CPluginView {
public:
    explicit Editor(Controller*);
    ~Editor() override;
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API attached(void*,Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) override;
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) override;
private:
    Controller* owner;
    std::shared_ptr<EditorState> state;
};
}
