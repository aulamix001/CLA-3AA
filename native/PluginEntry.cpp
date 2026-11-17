#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "public.sdk/source/main/pluginfactory.h"

BEGIN_FACTORY_DEF("Aulamix", "https://github.com/aulamix001/CLA-3AA", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(Cla::processorId),Steinberg::PClassInfo::kManyInstances,
    kVstAudioEffectClass,"CLA-3A Studio",0,"Fx|Dynamics","0.1.0",kVstVersionString,Cla::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(Cla::controllerId),Steinberg::PClassInfo::kManyInstances,
    kVstComponentControllerClass,"CLA-3A Studio Controller",0,"","0.1.0",kVstVersionString,Cla::Controller::create)
END_FACTORY
