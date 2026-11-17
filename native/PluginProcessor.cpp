#include "PluginProcessor.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace Cla {
bool readState(IBStream* stream,Values& values){
    if(!stream)return false;IBStreamer reader(stream,kLittleEndian);int32 version=0;
    if(!reader.readInt32(version)||version!=1)return false;
    for(std::size_t i=0;i<parameterCount;++i){double value=0.;if(!reader.readDouble(value))return false;values[i]=clampValue(i,value);}return true;
}
bool writeState(IBStream* stream,const Values& values){
    if(!stream)return false;IBStreamer writer(stream,kLittleEndian);if(!writer.writeInt32(1))return false;
    for(double value:values)if(!writer.writeDouble(value))return false;return true;
}
Processor::Processor(){setControllerClass(controllerId);for(std::size_t i=0;i<parameterCount;++i)saved[i].store(values[i]);}
tresult PLUGIN_API Processor::initialize(FUnknown* context){auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;addAudioInput(STR16("Input"),SpeakerArr::kStereo);addAudioOutput(STR16("Output"),SpeakerArr::kStereo);return kResultOk;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup){sampleRate=setup.sampleRate;dsp.setValues(values);dsp.prepare(sampleRate);return AudioEffect::setupProcessing(setup);}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size){return size==kSample32||size==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs,int32 numIns,SpeakerArrangement* outputs,int32 numOuts){
    if(numIns!=1||numOuts!=1||inputs[0]!=outputs[0]||(inputs[0]!=SpeakerArr::kMono&&inputs[0]!=SpeakerArr::kStereo))return kResultFalse;
    return AudioEffect::setBusArrangements(inputs,numIns,outputs,numOuts);
}
tresult PLUGIN_API Processor::setState(IBStream* stream){Values next;if(!readState(stream,next))return kResultFalse;for(std::size_t i=0;i<parameterCount;++i)saved[i].store(next[i]);return kResultOk;}
tresult PLUGIN_API Processor::getState(IBStream* stream){Values snapshot;for(std::size_t i=0;i<parameterCount;++i)snapshot[i]=saved[i].load();return writeState(stream,snapshot)?kResultOk:kResultFalse;}
tresult PLUGIN_API Processor::notify(IMessage* message){
    if(message&&std::strcmp(message->getMessageID(),"meter")==0){
        if(auto reply=owned(allocateMessage())){reply->setMessageID("meter");reply->getAttributes()->setFloat("input",inMeter.load());reply->getAttributes()->setFloat("output",outMeter.load());reply->getAttributes()->setFloat("reduction",grMeter.load());sendMessage(reply);}return kResultOk;
    }return AudioEffect::notify(message);
}
template<class Sample> void Processor::processAudio(ProcessData& data){
    const auto channels=data.outputs[0].numChannels;
    Sample** input;if constexpr(sizeof(Sample)==4)input=data.inputs[0].channelBuffers32;else input=data.inputs[0].channelBuffers64;
    Sample** output;if constexpr(sizeof(Sample)==4)output=data.outputs[0].channelBuffers32;else output=data.outputs[0].channelBuffers64;
    std::array<IParamValueQueue*,parameterCount> queues{};std::array<int32,parameterCount> points{};
    if(auto changes=data.inputParameterChanges)for(int32 q=0;q<changes->getParameterCount();++q){auto queue=changes->getParameterData(q);if(!queue)continue;auto index=indexForId(queue->getParameterId());if(index>=0)queues[index]=queue;}
    data.outputs[0].silenceFlags=0;
    for(int32 sample=0;sample<data.numSamples;++sample){
        bool changed=false;
        for(std::size_t i=0;i<parameterCount;++i){auto queue=queues[i];if(!queue)continue;
            while(points[i]<queue->getPointCount()){int32 offset=0;ParamValue value=0.;if(queue->getPoint(points[i],offset,value)!=kResultTrue)break;if(offset>sample)break;values[i]=denormalize(i,value);saved[i].store(values[i]);++points[i];changed=true;}
        }if(changed)dsp.setValues(values);
        const double left=input&&input[0]?input[0][sample]:0.;const double right=channels>1&&input&&input[1]?input[1][sample]:left;
        const auto result=dsp.tick(left,right);
        if(output&&output[0])output[0][sample]=static_cast<Sample>(result.first);
        if(channels>1&&output&&output[1])output[1][sample]=static_cast<Sample>(result.second);
    }
    for(int32 channel=0;channel<channels;++channel){bool silent=true;if(output&&output[channel])for(int32 sample=0;sample<data.numSamples;++sample)if(output[channel][sample]!=0){silent=false;break;}if(silent)data.outputs[0].silenceFlags|=uint64(1)<<channel;}
}
tresult PLUGIN_API Processor::process(ProcessData& data){
    for(std::size_t i=0;i<parameterCount;++i)values[i]=saved[i].load();dsp.setValues(values);
    if(data.numSamples==0||data.numInputs==0||data.numOutputs==0){
        if(auto changes=data.inputParameterChanges)for(int32 q=0;q<changes->getParameterCount();++q){auto queue=changes->getParameterData(q);if(!queue||queue->getPointCount()==0)continue;auto i=indexForId(queue->getParameterId());int32 offset=0;ParamValue value=0.;if(i>=0&&queue->getPoint(queue->getPointCount()-1,offset,value)==kResultTrue)saved[i].store(denormalize(i,value));}return kResultOk;
    }
    if(data.symbolicSampleSize==kSample64)processAudio<double>(data);else processAudio<float>(data);
    inMeter.store(dsp.inputDb());outMeter.store(dsp.outputDb());grMeter.store(dsp.reductionDb());return kResultOk;
}
}
