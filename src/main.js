import {meterAngle} from './vu-meter.js';
import { PARAMETERS, STARTUP_SETTINGS, RESET_SETTINGS, KNOB_RANGES, BUILT_IN_PRESETS, RESET_PRESET_INDEX,
  PRESET_SCHEMA_VERSION, applyPreset, normalizeControlValue, loadUserPresets, hfControlOffset } from './cla3a-config.js';
const $ = (s) => document.querySelector(s);
const $$ = (s) => [...document.querySelectorAll(s)];
const nativeBridge = globalThis.chrome?.webview;
let nativeReady = false, applyingNative = false;
const defaults = STARTUP_SETTINGS;
const ranges = KNOB_RANGES;
let slot='A', slots={A:{...defaults},B:{...defaults}}, state=slots.A, presetIndex=0, presetTitle='Start Me Up', undo=[], redo=[], bypass=false;
let custom=[];
try {custom=loadUserPresets(JSON.parse(localStorage.getItem('cla3a-presets')||'[]'));}catch{}
const presets=[...BUILT_IN_PRESETS,...custom];
function snapshot(){return JSON.stringify({slots,slot,presetTitle,presetIndex});}
function record(before=snapshot()){undo.push(before);if(undo.length>80)undo.shift();redo=[];historyUI();}
function restore(raw){const x=JSON.parse(raw);slots=x.slots;slot=x.slot;presetTitle=x.presetTitle;presetIndex=x.presetIndex;state=slots[slot];render();}
function historyUI(){$('#undo').disabled=!undo.length;$('#redo').disabled=!redo.length;}
function change(key,value){value=normalizeControlValue(key,value);if(state[key]===value)return;record();state[key]=value;render();}
function toast(message){$('#toast').textContent=message;$('#toast').classList.add('visible');clearTimeout(toast.timer);toast.timer=setTimeout(()=>$('#toast').classList.remove('visible'),2600);}
const ns='http://www.w3.org/2000/svg';
function svgElement(tag,attrs){const e=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))e.setAttribute(k,v);return e;}
$$('[data-control]').forEach(el=>{
 const key=el.dataset.control,small=el.classList.contains('small-knob');
 el.setAttribute('role','slider');el.setAttribute('tabindex','0');el.setAttribute('aria-label',({gain:'Input gain',peak:'Peak reduction',hifreq:'High frequency sidechain',mix:'Dry wet mix',trim:'Output trim'})[key]);el.setAttribute('aria-valuemin',ranges[key][0]);el.setAttribute('aria-valuemax',ranges[key][1]);
 const svg=svgElement('svg',{viewBox:small?(key==='hifreq'?'0 0 35 40':'0 0 40 40'):'0 0 150 130',class:'knob-scale','aria-hidden':'true'});
 const cx=small?20:75,cy=small?20:77,r=small?18:62;
 if(key==='hifreq'){
  svg.append(svgElement('path',{d:'M3 8 Q17 -6 31 8',fill:'none',stroke:'#e7e8dc','stroke-width':1.6}));
  svg.append(svgElement('path',{d:'m28 6 4 3 -1-5',fill:'none',stroke:'#e7e8dc','stroke-width':1}));
 }else{
  for(let i=0;i<=10;i++){
   const angle=(-150+i*30)*Math.PI/180;
   if(!small){const t=svgElement('text',{x:cx+Math.sin(angle)*r,y:cy-Math.cos(angle)*r+4,'text-anchor':'middle','data-number':i});t.textContent=i;svg.append(t);}
   const rad=small?r:r-10;
   svg.append(svgElement('circle',{cx:cx+Math.sin(angle)*rad,cy:cy-Math.cos(angle)*rad,r:small?.55:1.35}));
  }
  if(small){
   const left=svgElement('text',{x:3,y:40,'text-anchor':'middle',class:'small-scale-label'});left.textContent=key==='trim'?'-18':'0';svg.append(left);
   const right=svgElement('text',{x:37,y:40,'text-anchor':'middle',class:'small-scale-label'});right.textContent=key==='trim'?'18':'100';svg.append(right);
  }
 }
 el.append(svg);const knob=document.createElement('div');knob.className='knob';knob.innerHTML='<div class="knob-pointer"></div>';el.append(knob);
 let startY,startX,startValue,before,drag=false;
 el.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();el.focus({preventScroll:true});el.setPointerCapture(e.pointerId);startY=e.clientY;startX=e.clientX;startValue=state[key];before=snapshot();drag=true;});
 el.addEventListener('pointermove',e=>{if(!drag)return;const [min,max]=ranges[key],delta=(startY-e.clientY+(e.clientX-startX)*.35)*(max-min)/((e.shiftKey?1600:180)*(el.getBoundingClientRect().width/el.offsetWidth));state[key]=normalizeControlValue(key,startValue+delta);render();});
 el.addEventListener('pointerup',()=>{if(!drag)return;drag=false;if(state[key]!==startValue)record(before);});
 el.addEventListener('pointercancel',()=>{drag=false;});
 el.addEventListener('dblclick',()=>change(key,ranges[key][2]));
 el.addEventListener('keydown',e=>{const [min,max]=ranges[key];let val=state[key],step=Math.max(PARAMETERS[key].step,(max-min)/(e.shiftKey?1000:100));if(e.key==='ArrowUp'||e.key==='ArrowRight')val+=step;else if(e.key==='ArrowDown'||e.key==='ArrowLeft')val-=step;else if(e.key==='Home')val=min;else if(e.key==='End')val=max;else return;e.preventDefault();change(key,val);});
 el.addEventListener('wheel',e=>{if(document.activeElement!==el)return;e.preventDefault();const [min,max]=ranges[key];change(key,state[key]-Math.sign(e.deltaY)*Math.max(PARAMETERS[key].step,(max-min)/(e.shiftKey?1000:100)));},{passive:false});
});
// The compact, printed meter scale follows the reference window.
const meterMarks=[['-20',30,59,38,69,49,81],['-10',60,46,67,57,76,71],['-7',87,35,94,48,101,63],['-5',115,29,120,42,125,57],['-3',145,25,148,39,151,54],['-2',174,24,175,37,175,52],['-1',202,25,201,39,199,54],['0',230,29,225,42,221,57],['1',258,36,251,48,245,63],['2',286,47,277,59,269,72],['3',316,63,304,75,293,86]];
meterMarks.forEach(([value,tx,ty,x1,y1,x2,y2],i)=>{
 const color=i>=7?'#ae4930':'#333524';
 $('#meter-scale').append(svgElement('line',{x1,y1,x2,y2,stroke:color,'stroke-width':1.7}));
 const t=svgElement('text',{x:tx,y:ty,'text-anchor':'middle',fill:color,'font-size':19,'font-family':'Arial'});t.textContent=value;$('#meter-scale').append(t);
});
const stage=$('.plugin-stage');
function scaleInterface(){ $('.plugin').style.transform=`scale(${stage.clientWidth/640})`; }
new ResizeObserver(scaleInterface).observe(stage);scaleInterface();
function render(){
 $$('[data-control]').forEach(el=>{const key=el.dataset.control;const [min,max]=ranges[key];el.style.setProperty('--rotation',`${key==='hifreq'?hfControlOffset(state[key])*1.35:-150+(state[key]-min)/(max-min)*300+(key==='gain'?14:0)}deg`);el.setAttribute('aria-valuenow',state[key]);el.setAttribute('aria-valuetext',`${state[key]}${key==='mix'?' percent':key==='trim'?' dB':''}`);el.title=`${el.getAttribute('aria-label')}: ${state[key]} · Drag to adjust`;el.querySelectorAll('[data-number]').forEach(t=>t.classList.toggle('active',Number(t.dataset.number)===Math.round(state[key])));});
 $('#gain-readout').textContent=state.gain.toFixed(1);$('#peak-readout').textContent=state.peak.toFixed(1);
 $('#mix-value').innerHTML=`${Math.round(state.mix)}<span>%</span>`;$('#trim-value').innerHTML=`${state.trim>0?'+':''}${state.trim.toFixed(1)}<span>dB</span>`;$('#hifreq-value').textContent=state.hifreq===PARAMETERS.hifreq.reset?'[ FLAT ]':`[ ${Math.round(state.hifreq)} ]`;
 $$('[data-analog]').forEach(b=>{b.classList.toggle('active',b.dataset.analog===state.analog);b.setAttribute('aria-pressed',b.dataset.analog===state.analog);});
 $$('[data-vu]').forEach(b=>{b.classList.toggle('selected',b.dataset.vu===state.vu);b.setAttribute('aria-pressed',b.dataset.vu===state.vu);});
 $('.vu-svg').setAttribute('aria-label',`VU meter: ${state.vu}, ${state.vu==='GR'?'gain reduction in dB':'0 VU = -18 dBFS RMS'}`);
 $('#vu-switch').dataset.mode=state.vu;$('#vu-switch').setAttribute('aria-label',`VU display: ${state.vu}. Click to cycle.`);
 $('#mode-switch').classList.toggle('on',state.mode==='LIMITER');$('#mode-switch').setAttribute('aria-checked',state.mode==='LIMITER');$('#comp-label').classList.toggle('selected',state.mode==='COMP');$('#limiter-label').classList.toggle('selected',state.mode==='LIMITER');
 $('#setup').textContent=`Setup ${slot}`;$('#copy').textContent=slot==='A'?'A→B':'B→A';$('#preset-name').textContent=`${slot}: ${presetTitle}`;historyUI();updateAudio();postNativeState();
}
function setPreset(index){record();presetIndex=(index+presets.length)%presets.length;state=slots[slot]=applyPreset(state,presets[presetIndex]);presetTitle=presets[presetIndex].name;render();closeMenus();}
function renderPresets(){const menu=$('#preset-menu');menu.innerHTML='';presets.forEach((p,i)=>{const btn=document.createElement('button');btn.textContent=p.name;btn.setAttribute('role','option');btn.setAttribute('aria-selected',presetIndex===i);btn.classList.toggle('active',presetIndex===i);if(i===presetIndex){const mark=document.createElement('span');mark.textContent='✓';btn.append(mark);}if(p.description){const hint=document.createElement('small');hint.textContent=p.description;btn.append(hint);btn.title=p.description;}btn.onclick=()=>setPreset(i);menu.append(btn);});}
function closeMenus(){$('#preset-menu').hidden=true;$('#settings-menu').hidden=true;$('#preset-btn').setAttribute('aria-expanded','false');$('#menu-btn').setAttribute('aria-expanded','false');}
$('#preset-btn').onclick=e=>{e.stopPropagation();const open=$('#preset-menu').hidden;closeMenus();renderPresets();$('#preset-menu').hidden=!open;$('#preset-btn').setAttribute('aria-expanded',open);};
$('#menu-btn').onclick=e=>{e.stopPropagation();const open=$('#settings-menu').hidden;closeMenus();$('#settings-menu').hidden=!open;$('#menu-btn').setAttribute('aria-expanded',open);};
document.addEventListener('click',e=>{if(!e.target.closest('.popup'))closeMenus();});
document.addEventListener('keydown',e=>{if(e.key==='Escape')closeMenus();if((e.ctrlKey||e.metaKey)&&e.key==='z'&&!e.target.matches('input')){e.preventDefault();$(e.shiftKey?'#redo':'#undo').click();}});
$('#previous').onclick=()=>setPreset(presetIndex-1);$('#next').onclick=()=>setPreset(presetIndex+1);
$('#undo').onclick=()=>{if(!undo.length)return;redo.push(snapshot());restore(undo.pop());};$('#redo').onclick=()=>{if(!redo.length)return;undo.push(snapshot());restore(redo.pop());};
$('#copy').onclick=()=>{record();const other=slot==='A'?'B':'A';slots[other]={...state};toast(`Setup ${slot} copied to Setup ${other}`);render();};
$('#setup').onclick=()=>{record();slot=slot==='A'?'B':'A';state=slots[slot];render();toast(`Comparing Setup ${slot}`);};
function reset(){record();slots[slot]={...RESET_SETTINGS};state=slots[slot];presetIndex=RESET_PRESET_INDEX;presetTitle=presets[presetIndex].name;render();closeMenus();toast('Default settings restored');}
$('#reset').onclick=reset;
$$('[data-analog]').forEach(b=>b.onclick=()=>change('analog',b.dataset.analog));$$('[data-vu]').forEach(b=>b.onclick=()=>change('vu',b.dataset.vu));$('#vu-switch').onclick=()=>change('vu',['IN','GR','OUT'][(['IN','GR','OUT'].indexOf(state.vu)+1)%3]);
function mode(value){change('mode',value);}$('#mode-switch').onclick=()=>mode(state.mode==='COMP'?'LIMITER':'COMP');$('#comp-label').onclick=()=>mode('COMP');$('#limiter-label').onclick=()=>mode('LIMITER');
$('#about').onclick=()=>{closeMenus();$('#guide').showModal();};$('#guide .close-dialog').onclick=()=>$('#guide').close();$('#guide').addEventListener('click',e=>{if(e.target===$('#guide')){const r=e.target.getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)e.target.close();}});
$('#demo-open').onclick=()=>{closeMenus();$('#audition-dialog').showModal();};
$('#audition-dialog .close-dialog').onclick=()=>$('#audition-dialog').close();
const saveDialog=document.createElement('dialog');saveDialog.className='save-dialog';saveDialog.innerHTML='<button class="close-dialog" aria-label="Cancel">×</button><div class="eyebrow">YOUR SOUND, SAVED</div><h2>Save your preset.</h2><form><label for="save-name">Preset name</label><input id="save-name" required maxlength="40" autocomplete="off" placeholder="My classic sound"><p>Stored locally in this browser. Find it in the preset menu.</p><button type="submit" class="play-btn">Save preset ↗</button></form>';document.body.append(saveDialog);saveDialog.querySelector('.close-dialog').onclick=()=>saveDialog.close();$('#save').onclick=()=>{$('#save-name').value=presetTitle==='Start Me Up'?'':presetTitle;saveDialog.showModal();};saveDialog.querySelector('form').onsubmit=e=>{e.preventDefault();const name=$('#save-name').value.trim();if(!name){$('#save-name').focus();return;}const preset={name,schemaVersion:PRESET_SCHEMA_VERSION,settings:{...state}};try{const at=custom.findIndex(p=>p.name===name);if(at>=0)custom[at]=preset;else custom.push(preset);localStorage.setItem('cla3a-presets',JSON.stringify(custom));const existing=presets.findIndex((p,i)=>i>=BUILT_IN_PRESETS.length&&p.name===name);if(existing>=0){presets[existing]=preset;presetIndex=existing;}else{presets.push(preset);presetIndex=presets.length-1;}presetTitle=name;render();saveDialog.close();toast(`“${name}” saved to your presets`);}catch{toast('Browser storage is unavailable. Preset could not be saved.');}};
// Web Audio demonstration: a synthesized source through native compression.
let audio=null,playing=false,timer=null,nextBeat=0,beat=0,animation;
function setupAudio(){const ctx=new AudioContext();const input=ctx.createGain(),comp=ctx.createDynamicsCompressor(),dry=ctx.createGain(),wet=ctx.createGain(),output=ctx.createGain(),inputMeter=ctx.createAnalyser(),outputMeter=ctx.createAnalyser();inputMeter.fftSize=256;outputMeter.fftSize=256;input.connect(comp);input.connect(dry);input.connect(inputMeter);comp.connect(wet);dry.connect(output);wet.connect(output);output.connect(outputMeter);outputMeter.connect(ctx.destination);comp.attack.value=.012;comp.release.value=.25;const hum=ctx.createOscillator(),humGain=ctx.createGain();hum.type='sine';hum.connect(humGain);humGain.connect(input);humGain.gain.value=0;hum.start();audio={ctx,input,comp,dry,wet,output,inputMeter,outputMeter,hum,humGain,voices:[],noise:null};const noise=ctx.createBuffer(1,ctx.sampleRate,ctx.sampleRate);const data=noise.getChannelData(0);for(let i=0;i<data.length;i++)data[i]=Math.random()*2-1;audio.noise=noise;}
function updateAudio(){if(!audio)return;const {ctx,input,comp,dry,wet,output,hum,humGain}=audio,t=ctx.currentTime;input.gain.setTargetAtTime(.45+state.gain*.15,t,.02);comp.threshold.setTargetAtTime(-4-state.peak*3+hfControlOffset(state.hifreq)*.025,t,.02);comp.knee.value=state.mode==='COMP'?20:4;comp.ratio.value=state.mode==='COMP'?4:20;dry.gain.setTargetAtTime(bypass?1:1-state.mix/100,t,.02);wet.gain.setTargetAtTime(bypass?0:state.mix/100,t,.02);output.gain.setTargetAtTime(Math.pow(10,state.trim/20)*.65,t,.02);hum.frequency.setTargetAtTime(state.analog==='50Hz'?50:60,t,.02);humGain.gain.setTargetAtTime(playing&&state.analog!=='OFF'?.001:0,t,.02);}
function voice(freq,time,duration,type='triangle',volume=.2){const {ctx,input}=audio,o=ctx.createOscillator(),g=ctx.createGain();o.type=type;o.frequency.value=freq;g.gain.setValueAtTime(0,time);g.gain.linearRampToValueAtTime(volume,time+.008);g.gain.exponentialRampToValueAtTime(.0001,time+duration);o.connect(g);g.connect(input);o.start(time);o.stop(time+duration+.05);audio.voices.push({o,g});o.onended=()=>{o.disconnect();g.disconnect();audio.voices=audio.voices.filter(v=>v.o!==o);};return o;}
function noiseHit(time,duration,volume,highpass){const {ctx,input,noise}=audio,n=ctx.createBufferSource(),g=ctx.createGain(),filter=ctx.createBiquadFilter();n.buffer=noise;filter.type='highpass';filter.frequency.value=highpass;g.gain.setValueAtTime(volume,time);g.gain.exponentialRampToValueAtTime(.0001,time+duration);n.connect(filter);filter.connect(g);g.connect(input);n.start(time);n.stop(time+duration);audio.voices.push({o:n,g});n.onended=()=>{n.disconnect();g.disconnect();filter.disconnect();audio.voices=audio.voices.filter(v=>v.o!==n);};}
function schedule(){if(!playing)return;while(nextBeat<audio.ctx.currentTime+.16){const source=$('#source').value;const t=nextBeat;if(source==='keys'){if(beat%4===0){const chords=[[130.81,164.81,196],[110,130.81,164.81],[87.31,130.81,174.61],[98,123.47,146.83]];chords[Math.floor(beat/4)%4].forEach((f,i)=>voice(f,t+i*.025,1.7,'triangle',.13));}voice([523.25,392,440,329.63,392,293.66,329.63,392][beat%8],t,.3,'sine',beat%2?.055:.09);}else if(source==='bass'){const f=[65.41,65.41,98,73.42,55,55,82.41,61.74][beat%8];voice(f,t,.42,'triangle',.45);voice(f*2,t,.15,'sine',.1);}else{noiseHit(t,.045,.06,7000);if(beat%4===0||beat%8===3){const o=voice(135,t,.35,'sine',.65);o.frequency.exponentialRampToValueAtTime(42,t+.25);}if(beat%4===2){noiseHit(t,.17,.25,1000);voice(170,t,.1,'triangle',.15);}}beat++;nextBeat+=.25;}}
function meterLoop(){
 if(!playing)return;
 const levels=[-120,-120,bypass?0:Math.abs(audio.comp.reduction)];
 for(const [index,analyser] of [audio.inputMeter,audio.outputMeter].entries()){
  const samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);
  const power=samples.reduce((sum,v)=>sum+v*v,0)/samples.length;
  levels[index]=10*Math.log10(Math.max(power,1.e-12));
 }
 $('#needle').style.transform=`rotate(${meterAngle(state.vu,levels,bypass)}deg)`;
 animation=requestAnimationFrame(meterLoop);
}
$('#play').onclick=async()=>{if(nativeBridge)return;try{if(!audio)setupAudio();await audio.ctx.resume();playing=!playing;if(playing){nextBeat=audio.ctx.currentTime+.04;beat=0;schedule();timer=setInterval(schedule,70);meterLoop();}else{clearInterval(timer);cancelAnimationFrame(animation);audio.voices.forEach(v=>{try{v.o.stop();}catch{}});$('#needle').style.transform=`rotate(${meterAngle(state.vu,[-120,-120,0],bypass)}deg)`;}updateAudio();$('#play').innerHTML=playing?'<svg viewBox="0 0 20 20"><path d="M6 4h3v12H6zM12 4h3v12h-3z" fill="currentColor"/></svg><span>Stop demo</span>':'<svg viewBox="0 0 20 20"><path d="m7 4 9 6-9 6z" fill="currentColor"/></svg><span>Play demo</span>';$('#play').setAttribute('aria-label',playing?'Stop demo':'Play demo');$('.audition').classList.toggle('is-playing',playing);$('#signal-label').textContent=playing?'SIGNAL ACTIVE':'SIGNAL IDLE';}catch{toast('Audio could not start. Check browser audio permissions.');}};
$('#bypass').onclick=()=>{bypass=!bypass;$('#bypass').setAttribute('aria-checked',bypass);updateAudio();postNativeState();toast(bypass?'Bypass on · dry signal':'Compressor active');};
$('#source').onchange=()=>{beat=0;};
$('#audition-dialog').addEventListener('close',()=>{if(playing)$('#play').click();});
$('#needle').style.transform=`rotate(${meterAngle(state.vu,[-120,-120,0],bypass)}deg)`;render();

// Native VST3 parameter transport. Existing drawing and browser behavior stay intact.
function postNativeState(){
 if(!nativeBridge||!nativeReady||applyingNative)return;
 const values=[state.gain,state.peak,state.hifreq,state.mix,state.trim,
  ['IN','GR','OUT'].indexOf(state.vu),state.mode==='LIMITER'?1:0,
  ['50Hz','60Hz','OFF'].indexOf(state.analog),bypass?1:0];
 nativeBridge.postMessage('params:'+values.join(','));
}
nativeBridge?.addEventListener('message',({data})=>{
 if(data?.type!=='native-state'||!Array.isArray(data.values)||data.values.length!==9||!data.values.every(Number.isFinite))return;
 applyingNative=true;nativeReady=true;
 try{
  const [gain,peak,hifreq,mix,trim,vu,mode,analog,nativeBypass]=data.values;
  const next={gain,peak,hifreq,mix,trim,vu:['IN','GR','OUT'][Math.round(vu)],mode:mode>=.5?'LIMITER':'COMP',analog:['50Hz','60Hz','OFF'][Math.round(analog)]};
  let changed=false;
  for(const key of Object.keys(next)){const value=normalizeControlValue(key,next[key]);if(state[key]!==value){state[key]=value;changed=true;}}
  const nextBypass=nativeBypass>=.5;
  if(bypass!==nextBypass){bypass=nextBypass;$('#bypass').setAttribute('aria-checked',bypass);changed=true;}
  if(changed)render();
  if(Array.isArray(data.meters)&&data.meters.length===3&&data.meters.every(Number.isFinite)){
   const angle=meterAngle(state.vu,data.meters,bypass);
   $('#needle').style.transform=`rotate(${angle}deg)`;
  }
 }finally{applyingNative=false;}
});
nativeBridge?.postMessage('ready');
