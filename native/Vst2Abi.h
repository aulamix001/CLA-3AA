#pragma once
#include <cstdint>
// Independently declared public VST 2.x binary layout; no legacy SDK headers.
namespace Cla::Vst2 {
struct Effect;
using Host=std::intptr_t (*)(Effect*,std::int32_t,std::int32_t,std::intptr_t,void*,float);
using Dispatch=Host;
using Process=void (*)(Effect*,float**,float**,std::int32_t);
using ProcessDouble=void (*)(Effect*,double**,double**,std::int32_t);
struct Effect {
 std::int32_t magic;
 Dispatch dispatcher;Process process;
 void (*setParameter)(Effect*,std::int32_t,float);
 float (*getParameter)(Effect*,std::int32_t);
 std::int32_t numPrograms,numParams,numInputs,numOutputs,flags;
 void* reserved1;void* reserved2;
 std::int32_t initialDelay,realQualities,offQualities;float ioRatio;
 void* object;void* user;
 std::int32_t uniqueID,version;
 Process processReplacing;ProcessDouble processDoubleReplacing;char future[56];
};
static_assert(sizeof(void*)!=8||sizeof(Effect)==192);
constexpr std::int32_t magic=0x56737450;
}
