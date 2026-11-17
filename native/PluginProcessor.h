#pragma once
#include "Dsp.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include <atomic>

namespace Cla {
inline const Steinberg::FUID processorId {0xA29C4E19,0x4CC95E01,0x9B3BF257,0xC0AB03A1};
inline const Steinberg::FUID controllerId {0x31A203CB,0x017E52E6,0xB07A6194,0xE143339D};
bool readState(Steinberg::IBStream*,Values&);
bool writeState(Steinberg::IBStream*,const Values&);
class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();
    static Steinberg::FUnknown* create(void*) {return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor);}
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*,Steinberg::int32,Steinberg::Vst::SpeakerArrangement*,Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    Steinberg::uint32 PLUGIN_API getTailSamples() override {return static_cast<Steinberg::uint32>(sampleRate*3.);}
private:
    template<class Sample> void processAudio(Steinberg::Vst::ProcessData&);
    Values values=initialValues();
    std::array<std::atomic<double>,parameterCount> saved{};
    std::atomic<double> inMeter{-120.},outMeter{-120.},grMeter{0.};
    Dsp dsp;
    double sampleRate=48000.;
};
}
