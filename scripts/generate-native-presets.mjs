import {BUILT_IN_PRESETS,STARTUP_SETTINGS,applyPreset} from '../src/cla3a-config.js';
import {writeFile} from 'node:fs/promises';
const rows=BUILT_IN_PRESETS.map(p=>{
 const s=applyPreset(STARTUP_SETTINGS,p);
 const values=[s.gain,s.peak,s.hifreq,s.mix,s.trim,['IN','GR','OUT'].indexOf(s.vu),s.mode==='LIMITER'?1:0,['50Hz','60Hz','OFF'].indexOf(s.analog),0];
 return `{${JSON.stringify(p.name)},{{${values.join(',')}}}}`;
});
await writeFile(process.argv[2],`#pragma once\n#include "Parameters.h"\nnamespace Cla {struct NativePreset {const char* name;Values values;};inline const NativePreset nativePresets[]={${rows.join(',\n')}};}\n`);
