import test from 'node:test';
import assert from 'node:assert/strict';
import {
  PARAMETERS, INITIAL_SETTINGS, STARTUP_SETTINGS, RESET_SETTINGS, KNOB_RANGES,
  FACTORY_PRESETS, GENERATED_PRESETS, BUILT_IN_PRESETS, RESET_PRESET_INDEX, PRESET_SCHEMA_VERSION,
  normalizeControlValue, normalizeSettings, hfControlOffset, applyPreset, loadUserPresets,
} from '../src/cla3a-config.js';

const factory = name => FACTORY_PRESETS.find(preset => preset.name === name);

test('XML control metadata keeps independent initial/reset values and resolutions', () => {
  assert.deepEqual([PARAMETERS.gain.index, PARAMETERS.gain.initial, PARAMETERS.gain.reset, PARAMETERS.gain.step], [38, 5.5, 4.08, .01]);
  assert.equal(PARAMETERS.gain.role, 'makeup');
  assert.deepEqual(KNOB_RANGES, {gain:[0,10,4.08], peak:[0,10,0], hifreq:[0,100,100], mix:[0,100,100], trim:[-18,18,0]});
  assert.equal(PARAMETERS.peak.step, .01);
  for (const key of ['hifreq','mix','trim']) assert.equal(PARAMETERS[key].step, .1);
  assert.equal(INITIAL_SETTINGS.analog, 'OFF');
  assert.deepEqual(STARTUP_SETTINGS, {gain:5.5, peak:4, hifreq:50, mix:100, trim:0, vu:'GR', mode:'COMP', analog:'60Hz'});
  assert.deepEqual(RESET_SETTINGS, {gain:4.08, peak:0, hifreq:100, mix:100, trim:0, vu:'GR', mode:'COMP', analog:'OFF'});
});

test('all 14 original factory presets retain their XML order and name', () => {
  assert.deepEqual(FACTORY_PRESETS.map(preset => preset.name), [
    'Start Me Up','Full Reset','PA Default','Vox','Sibilant Vox','Rock Piano',
    'Acoustic Guitar','Electric Guitar','Drum Group','Hard Drum Group','Bass',
    'Strings','Greg Price Kick','Unity No Comp',
  ]);
  assert.equal(RESET_PRESET_INDEX, 1);
  assert.equal(factory('Full Reset').type, 'Reset');
});

// Independently transcribed from positions 38, 74, 104, and 87 in the uploaded preset XML.
const expected = [
  ['Start Me Up',5.5,4,50,'COMP'], ['PA Default',4,3,50,'COMP'],
  ['Vox',5,5,100,'COMP'], ['Sibilant Vox',5,5,50,'COMP'],
  ['Rock Piano',4.5,4.25,40,'LIMITER'], ['Acoustic Guitar',5,5,90,'COMP'],
  ['Electric Guitar',5,4.5,37,'COMP'], ['Drum Group',5,5.25,100,'COMP'],
  ['Hard Drum Group',6,7,50,'LIMITER'], ['Bass',5,4.85,100,'COMP'],
  ['Strings',4.25,4.25,100,'LIMITER'], ['Greg Price Kick',5,5,50,'COMP'],
  ['Unity No Comp',4,0,90,'COMP'],
];
for (const [name, gain, peak, hifreq, mode] of expected) {
  test(`${name}: original control values load without positional offset`, () => {
    const loaded = applyPreset(INITIAL_SETTINGS, factory(name));
    assert.deepEqual([loaded.gain, loaded.peak, loaded.hifreq, loaded.mode], [gain, peak, hifreq, mode]);
  });
}

test('sparse asterisks and absent newer fields preserve the current settings', () => {
  const previous = {...STARTUP_SETTINGS, vu:'OUT', trim:-3.1, mix:23.4};
  const loaded = applyPreset(previous, factory('Vox'));
  assert.equal(loaded.vu, 'OUT');
  assert.equal(loaded.trim, -3.1);
  assert.equal(loaded.mix, 23.4);
  assert.equal(loaded.hifreq, 100);
  assert.deepEqual(previous, {...STARTUP_SETTINGS, vu:'OUT', trim:-3.1, mix:23.4});
  assert.notEqual(loaded, previous);
});

test('IgnoreOnPresetLoad preserves Mix even when a factory preset supplies it', () => {
  const loaded = applyPreset({...STARTUP_SETTINGS, mix:42.3, trim:-7}, factory('PA Default'));
  assert.equal(factory('PA Default').parameters.PARAM_WET_DRY_GAIN, 100);
  assert.equal(loaded.mix, 42.3);
  assert.equal(loaded.trim, 0);
  assert.equal(loaded.analog, 'OFF');
});

test('Full Reset preset resets controls but respects the XML Mix load exclusion', () => {
  assert.deepEqual(applyPreset({...STARTUP_SETTINGS, mix:17.2, trim:9}, factory('Full Reset')), {...RESET_SETTINGS, mix:17.2});
  assert.equal(RESET_SETTINGS.mix, 100, 'explicit reset-all command has its own reset value');
});

test('the XML toggle states map to the existing controls', () => {
  for (const [id, key, values] of [
    ['PARAM_METER','vu',['IN','GR','OUT']],
    ['PARAM_COMP_LIMITER','mode',['COMP','LIMITER']],
    ['PARAM_ANALOG','analog',['50Hz','60Hz','OFF']],
  ]) {
    values.forEach((value, index) => assert.equal(applyPreset(INITIAL_SETTINGS, {parameters:{[id]:index}})[key], value));
    assert.equal(applyPreset(INITIAL_SETTINGS, {parameters:{[id]:99}})[key], INITIAL_SETTINGS[key]);
  }
});

test('Auto Makeup and calibration data remain inert and are not discarded', () => {
  const preset = factory('PA Default');
  assert.equal(preset.parameters.PARAM_MAKEUP_GAIN_ONOFF, 1);
  assert.ok(Object.hasOwn(preset.parameters, 'PARAM_RMS_ATTACK'));
  assert.deepEqual(Object.keys(applyPreset(INITIAL_SETTINGS, preset)).sort(), Object.keys(INITIAL_SETTINGS).sort());
  assert.equal(Object.hasOwn(applyPreset(INITIAL_SETTINGS, preset), 'autoMakeup'), false);
});

test('control changes clamp and quantize to the actual XML resolutions', () => {
  for (const [key, value, expected] of [
    ['gain',5.554,5.55], ['peak',4.249999999,4.25], ['hifreq',50.06,50.1],
    ['mix',99.94,99.9], ['trim',-3.14,-3.1], ['trim',-19,-18],
    ['gain',12,10], ['peak',-3,0], ['hifreq',200,100], ['mix',-1,0],
  ]) assert.equal(normalizeControlValue(key,value),expected);
  for (const invalid of [NaN,Infinity,-Infinity,undefined,null,'5']) assert.equal(normalizeControlValue('gain',invalid),5.5);
  assert.equal(normalizeControlValue('vu','INVALID'),'GR');
  assert.throws(() => normalizeControlValue('unknown',1),RangeError);
});

test('100=FLAT keeps the approved knob position and previous demo offset', () => {
  assert.equal(hfControlOffset(100),0);
  assert.equal(hfControlOffset(50),50);
  assert.equal(hfControlOffset(0),100);
  for (const legacy of [0,12.5,50,78.3,100]) assert.equal(hfControlOffset(100-legacy),legacy);
});

test('legacy browser presets migrate HF once without mutating saved data', () => {
  const original = [{name:'My saved sound',settings:{gain:6.25,peak:4.75,hifreq:0,mix:46.7,trim:-2.1,vu:'OUT',mode:'LIMITER',analog:'OFF'}}];
  const before = JSON.stringify(original);
  const migrated = loadUserPresets(original);
  assert.equal(JSON.stringify(original),before);
  assert.equal(migrated[0].schemaVersion,PRESET_SCHEMA_VERSION);
  assert.equal(migrated[0].settings.hifreq,100);
  assert.equal(migrated[0].settings.gain,6.25);
  assert.equal(migrated[0].settings.mix,46.7);
  assert.deepEqual(loadUserPresets(migrated),migrated);
  assert.deepEqual(applyPreset(INITIAL_SETTINGS,migrated[0]),migrated[0].settings);
});

test('custom presets restore their explicitly saved Mix and canonical HF', () => {
  const preset = {name:'Mine',schemaVersion:PRESET_SCHEMA_VERSION,settings:{...RESET_SETTINGS,mix:31.7,hifreq:90}};
  const loaded = loadUserPresets([preset])[0];
  assert.equal(loaded.settings.hifreq,90);
  assert.equal(applyPreset(STARTUP_SETTINGS,loaded).mix,31.7);
});

test('malformed browser preset data cannot inject state or invalid numeric values', () => {
  assert.deepEqual(loadUserPresets(null),[]);
  assert.deepEqual(loadUserPresets({name:'bad'}),[]);
  assert.deepEqual(loadUserPresets([null,{}, {name:'',settings:{}}, {name:'bad',settings:[]}, {name:'future',settings:{},schemaVersion:999}]),[]);
  const [loaded] = loadUserPresets([{name:'Partial',settings:{gain:NaN,trim:900,extra:true}}]);
  assert.equal(loaded.settings.gain,5.5);
  assert.equal(loaded.settings.trim,18);
  assert.equal(Object.hasOwn(loaded.settings,'extra'),false);
  assert.deepEqual(normalizeSettings(null),INITIAL_SETTINGS);
});

test('30 original presets extend the 14 XML presets without replacing their identities', () => {
  assert.equal(GENERATED_PRESETS.length,30);
  assert.equal(BUILT_IN_PRESETS.length,44);
  assert.deepEqual(BUILT_IN_PRESETS.slice(0,14),FACTORY_PRESETS);
  assert.equal(new Set(BUILT_IN_PRESETS.map(preset => preset.name)).size,44);
  assert.equal(BUILT_IN_PRESETS[RESET_PRESET_INDEX].name,'Full Reset');
});

for (const preset of GENERATED_PRESETS) {
  test(`${preset.name}: all knobs and switches have explicit, valid mappings`, () => {
    assert.equal(preset.type,'Generated');
    assert.deepEqual(Object.keys(preset.settings).sort(),Object.keys(PARAMETERS).sort());
    const previous = {...STARTUP_SETTINGS,mix:12.3,trim:9,mode:'LIMITER',analog:'50Hz',vu:'IN'};
    const applied = applyPreset(previous,preset);
    for (const [key,parameter] of Object.entries(PARAMETERS)) {
      assert.equal(applied[key],preset.settings[key],key);
      if (parameter.values) assert.ok(parameter.values.includes(applied[key]),key);
      else {
        assert.ok(applied[key]>=parameter.min && applied[key]<=parameter.max,key);
        assert.equal(normalizeControlValue(key,applied[key]),applied[key],key);
      }
    }
    assert.notEqual(applied,preset.settings);
    assert.equal(previous.mix,12.3);
    assert.equal(previous.trim,9);
  });
}

test('parallel and vintage presets map Mix, limiter and both hum frequencies', () => {
  const generated = name => GENERATED_PRESETS.find(preset => preset.name===name);
  assert.deepEqual(applyPreset(STARTUP_SETTINGS,generated('Parallel Drum Crush')),
    {gain:6.6,peak:8.2,hifreq:100,mix:30,trim:-1.5,mode:'LIMITER',analog:'OFF',vu:'GR'});
  assert.equal(applyPreset(STARTUP_SETTINGS,generated('Vintage 50Hz Glue')).analog,'50Hz');
  assert.equal(applyPreset(STARTUP_SETTINGS,generated('Vintage 60Hz Punch')).analog,'60Hz');
  assert.equal(applyPreset(STARTUP_SETTINGS,generated('Mix Bus Kiss')).vu,'OUT');
});
