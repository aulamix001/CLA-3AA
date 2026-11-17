#pragma once
#include "Parameters.h"
#include <cmath>
#include <utility>

namespace Cla {
// Independent native compressor; the supplied XML does not contain Waves LA3AProc.
// No allocation, locks, browser audio or file I/O takes place on the audio thread.
class Dsp {
public:
    void prepare(double rate) {
        sampleRate=std::clamp(rate,8000.,384000.);
        detectorAttack=std::exp(-1./(.004*sampleRate));
        slowRelease=std::exp(-1./(1.2*sampleRate));
        fastRelease=std::exp(-1./(.075*sampleRate));
        gainAttack=std::exp(-1./(.01*sampleRate));
        meterCoeff=std::exp(-1./(.3*sampleRate));
        smoothing=std::exp(-1./(.005*sampleRate));
        filterCoeff=std::exp(-2.*pi*1000./sampleRate);
        envelope=memory=gainReduction=lowL=lowR=inputPower=outputPower=phase=0.;
        randomState=0x43a031u;
        smoothMakeup=targetMakeup; smoothTrim=targetTrim; smoothMix=targetMix;
    }
    void setValues(const Values& values) {
        for(std::size_t i=0;i<parameterCount;++i) params[i]=clampValue(i,values[i]);
        // A transparent gain scale with the documented 4.08 unity point.
        targetMakeup=std::pow(10.,(params[Gain]-4.08)*6./20.);
        targetTrim=std::pow(10.,params[Trim]/20.);
        targetMix=params[Mix]/100.;
        contour=params[HiFreq]/100.;
        threshold=-6.-38.*std::pow(params[Peak]/10.,1.5);
        ratio=params[Mode]>=.5?100.:3.;
    }
    std::pair<double,double> tick(double left,double right) {
        left=std::isfinite(left)?left:0.;right=std::isfinite(right)?right:0.;
        lowL=filterCoeff*lowL+(1.-filterCoeff)*left;
        lowR=filterCoeff*lowR+(1.-filterCoeff)*right;
        const double detL=(left-lowL)*(1.+2.*(1.-contour))+lowL*contour;
        const double detR=(right-lowR)*(1.+2.*(1.-contour))+lowR*contour;
        const double detector=std::max(detL*detL,detR*detR);
        const double detectorCoeff=detector>envelope?detectorAttack:fastRelease;
        envelope=detectorCoeff*envelope+(1.-detectorCoeff)*detector;
        memory=std::max(envelope,slowRelease*memory);
        const double db=10.*std::log10(std::max(1.e-20,envelope));
        const double above=db-threshold, knee=params[Mode]>=.5?2.:8.;
        double target=0.;
        if(params[Peak]>.0001) {
            if(above>=knee*.5) target=above*(1.-1./ratio);
            else if(above>-knee*.5) target=std::pow(above+knee*.5,2.)/(2.*knee)*(1.-1./ratio);
        }
        const double release=memory>envelope*2.?slowRelease:fastRelease;
        const double coeff=target>gainReduction?gainAttack:release;
        gainReduction=std::max(0.,coeff*gainReduction+(1.-coeff)*target);
        smoothMakeup=smoothing*smoothMakeup+(1.-smoothing)*targetMakeup;
        smoothTrim=smoothing*smoothTrim+(1.-smoothing)*targetTrim;
        smoothMix=smoothing*smoothMix+(1.-smoothing)*targetMix;
        const double attenuation=std::pow(10.,-gainReduction/20.);
        double hum=0.;
        if(params[Analog]<1.5) {
            const double frequency=params[Analog]<.5?50.:60.;
            phase+=2.*pi*frequency/sampleRate;if(phase>2.*pi)phase-=2.*pi;
            randomState=randomState*1664525u+1013904223u;
            const double noise=(static_cast<double>(randomState)/4294967295.-.5)*.000004;
            hum=(std::sin(phase)+.12*std::sin(3.*phase))*(frequency==50.?.00006:.00008)+noise;
        }
        double outL=((left*attenuation*smoothMakeup+hum)*smoothMix+left*(1.-smoothMix))*smoothTrim;
        double outR=((right*attenuation*smoothMakeup+hum)*smoothMix+right*(1.-smoothMix))*smoothTrim;
        if(params[Bypass]>=.5){outL=left;outR=right;}
        inputPower=meterCoeff*inputPower+(1.-meterCoeff)*.5*(left*left+right*right);
        outputPower=meterCoeff*outputPower+(1.-meterCoeff)*.5*(outL*outL+outR*outR);
        return {outL,outR};
    }
    double inputDb() const {return 10.*std::log10(std::max(1.e-12,inputPower));}
    double outputDb() const {return 10.*std::log10(std::max(1.e-12,outputPower));}
    double reductionDb() const {return params[Bypass]>=.5?0.:gainReduction;}
private:
    static constexpr double pi=3.14159265358979323846;
    Values params=initialValues();
    double sampleRate=48000.,detectorAttack=0.,fastRelease=0.,slowRelease=0.,gainAttack=0.;
    double meterCoeff=0.,smoothing=0.,filterCoeff=0.;
    double envelope=0.,memory=0.,gainReduction=0.,lowL=0.,lowR=0.,inputPower=0.,outputPower=0.,phase=0.;
    double targetMakeup=1.,targetTrim=1.,targetMix=1.,smoothMakeup=1.,smoothTrim=1.,smoothMix=1.;
    double contour=1.,threshold=-18.,ratio=3.;
    std::uint32_t randomState=0x43a031u;
};
}
