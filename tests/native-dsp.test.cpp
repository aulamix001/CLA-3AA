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
 // Exercise the complete audio-to-meter relationship, not only knob metadata.
 double lastReduction=-1.;
 for(double peak:{0.,2.,4.,6.,8.,10.}){
  Cla::Dsp d;auto settings=clean();settings[Cla::Peak]=peak;d.setValues(settings);d.prepare(48000);
  run(d,1000,.3,192000,48000);
  const auto reduction=d.reductionDb();
  require(reduction>=lastReduction,"Peak Reduction must not reduce compression when turned up");
  require(std::abs((d.inputDb()-d.outputDb())-reduction)<.15,"GR meter disagrees with measured attenuation at unity gain");
  std::cout<<"Peak "<<peak<<": GR "<<reduction<<" dB; IN "<<d.inputDb()<<" dBFS; OUT "<<d.outputDb()<<" dBFS\n";
  lastReduction=reduction;
 }
 require(lastReduction>10.,"Peak sweep did not exercise meaningful compression");++checks;
 {
  Cla::Dsp base,gain,trim,mix;auto settings=clean();settings[Cla::Peak]=6;
  base.setValues(settings);base.prepare(48000);
  settings[Cla::Gain]+=1.;gain.setValues(settings);gain.prepare(48000);
  settings[Cla::Gain]=4.08;settings[Cla::Trim]=6.;trim.setValues(settings);trim.prepare(48000);
  settings[Cla::Trim]=0.;settings[Cla::Mix]=0.;mix.setValues(settings);mix.prepare(48000);
  for(auto d:{&base,&gain,&trim,&mix})run(*d,1000,.3,192000,48000);
  require(std::abs(gain.outputDb()-base.outputDb()-6.)<.01,"Gain must add 6 dB per unit after compression");
  require(std::abs(trim.outputDb()-base.outputDb()-6.)<.01,"Trim output calibration incorrect");
  require(std::abs(mix.outputDb()-mix.inputDb())<.01,"Mix 0 must meter the dry output");
  for(auto d:{&gain,&trim,&mix}){
   require(std::abs(d->inputDb()-base.inputDb())<.01,"output controls changed input meter");
   require(std::abs(d->reductionDb()-base.reductionDb())<.01,"output controls changed gain reduction meter");
  }
 }++checks;
 {
  Cla::Dsp d;auto settings=clean();d.setValues(settings);d.prepare(48000);
  run(d,1000,std::sqrt(2.)*std::pow(10.,-18./20.),192000,48000);
  require(std::abs(d.inputDb()+18.)<.01,"IN RMS calibration must read -18 dBFS at 0 VU");
  require(std::abs(d.outputDb()+18.)<.01,"OUT RMS calibration must read -18 dBFS at unity");
  settings[Cla::Peak]=8;settings[Cla::Bypass]=1;d.setValues(settings);run(d,1000,.5,192000,48000);
  require(d.reductionDb()==0.&&std::abs(d.inputDb()-d.outputDb())<.01,"bypass metering must indicate dry signal and zero GR");
 }++checks;
 {
  Cla::Dsp flatHigh,contourHigh;auto settings=clean();settings[Cla::Peak]=6;
  flatHigh.setValues(settings);flatHigh.prepare(48000);settings[Cla::HiFreq]=0;
  contourHigh.setValues(settings);contourHigh.prepare(48000);
  run(flatHigh,10000,.2,192000,48000);run(contourHigh,10000,.2,192000,48000);
  require(contourHigh.reductionDb()>flatHigh.reductionDb(),"HF contour must increase high-frequency sensitivity");
 }++checks;
 for(std::size_t i=Cla::Meter;i<Cla::parameterCount;++i){
  require(Cla::denormalize(i,.3)==std::round(.3*Cla::specs[i].max),"switch automation must select a discrete state");
 }++checks;
 std::cout<<"PASS: "<<checks<<" native DSP checks (44.1/48/96/192 kHz, stereo, unity, bypass, ratio, makeup, contour, Mix and hum).\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
