#include "../native/Dsp.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <limits>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Cla::Values clean(){auto v=Cla::initialValues();v[Cla::Gain]=4.08;v[Cla::Peak]=0;v[Cla::HiFreq]=100;v[Cla::Analog]=2;return v;}
double run(Cla::Dsp& dsp,double frequency,double amplitude,int count,double rate){double energy=0.;for(int i=0;i<count;++i){const double x=std::sin(6.283185307179586*frequency*i/rate)*amplitude;auto y=dsp.tick(x,x);require(std::isfinite(y.first)&&std::isfinite(y.second),"non-finite audio");if(i>count/2)energy+=y.first*y.first;}return energy/(count/2);}
int main(){try{
 int checks=0;
 for(double rate:{44100.,48000.,96000.,192000.}){
  Cla::Dsp d;auto v=clean();d.setValues(v);d.prepare(rate);
  for(int i=0;i<10000;++i){double x=std::sin(i*.03)*.3;auto y=d.tick(x,-x);require(std::abs(y.first-x)<1.e-10&&std::abs(y.second+x)<1.e-10,"unity/multichannel mismatch");}++checks;
  v[Cla::Bypass]=1;v[Cla::Gain]=10;v[Cla::Peak]=10;d.setValues(v);for(int i=0;i<1000;++i){auto y=d.tick(.2,-.1);require(y.first==.2&&y.second==-.1,"bypass must be dry");}++checks;
 }
 Cla::Dsp comp,lim;auto v=clean();v[Cla::Peak]=7;comp.setValues(v);comp.prepare(48000);v[Cla::Mode]=1;lim.setValues(v);lim.prepare(48000);
 auto compEnergy=run(comp,1000,.7,96000,48000),limEnergy=run(lim,1000,.7,96000,48000);require(compEnergy<.15&&limEnergy<compEnergy,"compression/limiting ineffective");++checks;
 Cla::Dsp a,b;v=clean();v[Cla::Peak]=6;a.setValues(v);a.prepare(48000);v[Cla::Gain]=6;b.setValues(v);b.prepare(48000);run(a,1000,.5,96000,48000);run(b,1000,.5,96000,48000);require(std::abs(a.reductionDb()-b.reductionDb())<1.e-9,"makeup gain changes the detector");++checks;
 Cla::Dsp flat,contour;v=clean();v[Cla::Peak]=6;flat.setValues(v);flat.prepare(48000);v[Cla::HiFreq]=0;contour.setValues(v);contour.prepare(48000);run(flat,100,.5,96000,48000);run(contour,100,.5,96000,48000);require(contour.reductionDb()<flat.reductionDb(),"sidechain contour must reduce LF sensitivity");++checks;
 Cla::Dsp dry;v=clean();v[Cla::Peak]=10;v[Cla::Gain]=10;v[Cla::Mix]=0;dry.setValues(v);dry.prepare(48000);require(std::abs(dry.tick(.25,.25).first-.25)<1.e-10,"mix zero must be dry");++checks;
 Cla::Dsp silence;v=clean();silence.setValues(v);silence.prepare(48000);require(silence.tick(0,0).first==0,"analog off adds noise");v[Cla::Analog]=0;silence.setValues(v);double noise=0;for(int i=0;i<4800;++i)noise+=std::abs(silence.tick(0,0).first);require(noise>0,"analog on adds no hum");++checks;
 require(std::isfinite(silence.tick(std::numeric_limits<double>::quiet_NaN(),0).first),"NaN input not sanitized");++checks;
 for(std::size_t i=0;i<Cla::parameterCount;++i){require(std::abs(Cla::denormalize(i,Cla::normalize(i,Cla::specs[i].initial))-Cla::specs[i].initial)<1.e-10,"parameter normalization mismatch");}
 ++checks;
 std::cout<<"PASS: "<<checks<<" native DSP checks (44.1/48/96/192 kHz, stereo, unity, bypass, ratio, makeup, contour, Mix and hum).\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
