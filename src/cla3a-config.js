// Parameter metadata and presets transcribed from the user-supplied CLA-3A XMLs.
// Parameters SHA-256: 335e456e28c9a2d3; presets SHA-256: 8b4bc0e8ea8abe85 (prefixes).
// This module contains no drawing, DOM code, or proprietary LA3AProc implementation.
export const PRESET_SCHEMA_VERSION = 2;

export const PARAMETERS = Object.freeze({
  gain: Object.freeze({id:'PARAM_OUT_GAIN', index:38, min:0, max:10, step:0.01, initial:5.5, reset:4.08, role:'makeup'}),
  peak: Object.freeze({id:'PARAM_GAIN_REDUCTION', index:74, min:0, max:10, step:0.01, initial:4, reset:0}),
  hifreq: Object.freeze({id:'PARAM_HF_CONTOUR', index:104, min:0, max:100, step:0.1, initial:50, reset:100}),
  // Mix and Trim have no ResetValue in the XML; reset values follow the user guide.
  mix: Object.freeze({id:'PARAM_WET_DRY_GAIN', index:159, min:0, max:100, step:0.1, initial:100, reset:100, ignoreOnPresetLoad:true}),
  trim: Object.freeze({id:'PARAM_TRIM', index:161, min:-18, max:18, step:0.1, initial:0, reset:0}),
  vu: Object.freeze({id:'PARAM_METER', index:17, values:Object.freeze(['IN','GR','OUT']), initial:'GR', reset:'GR'}),
  mode: Object.freeze({id:'PARAM_COMP_LIMITER', index:87, values:Object.freeze(['COMP','LIMITER']), initial:'COMP', reset:'COMP'}),
  // Numeric states follow the original radio-button order: 50 Hz, 60 Hz, Off.
  analog: Object.freeze({id:'PARAM_ANALOG', index:136, values:Object.freeze(['50Hz','60Hz','OFF']), initial:'OFF', reset:'OFF'}),
});

export const INITIAL_SETTINGS = Object.freeze(Object.fromEntries(
  Object.entries(PARAMETERS).map(([key, parameter]) => [key, parameter.initial]),
));
export const RESET_SETTINGS = Object.freeze(Object.fromEntries(
  Object.entries(PARAMETERS).map(([key, parameter]) => [key, parameter.reset]),
));
export const KNOB_RANGES = Object.freeze(Object.fromEntries(
  Object.entries(PARAMETERS).filter(([, parameter]) => !parameter.values)
    .map(([key, parameter]) => [key, Object.freeze([parameter.min, parameter.max, parameter.reset])]),
));

export function normalizeControlValue(key, value, fallback = PARAMETERS[key]?.initial) {
  const parameter = PARAMETERS[key];
  if (!parameter) throw new RangeError(`Unknown CLA-3A parameter: ${key}`);
  if (parameter.values) return parameter.values.includes(value) ? value : fallback;
  if (typeof value !== 'number' || !Number.isFinite(value)) return fallback;
  const bounded = Math.max(parameter.min, Math.min(parameter.max, value));
  const steps = Math.round((bounded - parameter.min) / parameter.step);
  const result = Number((parameter.min + steps * parameter.step).toFixed(2));
  return Object.is(result, -0) ? 0 : result;
}

export function normalizeSettings(settings = {}, fallback = INITIAL_SETTINGS) {
  return Object.fromEntries(Object.keys(PARAMETERS).map(key => [
    key, normalizeControlValue(key, settings?.[key], fallback[key]),
  ]));
}

// Keep the approved knob's geometry: its vertical FLAT position represents 100.
// The audio demo previously used the same offset with zero as its flat value.
export function hfControlOffset(value) {
  return 100 - normalizeControlValue('hifreq', value);
}

export function applyPreset(current, preset) {
  const previous = normalizeSettings(current);
  if (preset.type === 'Reset') {
    // Preset loads respect IgnoreOnPresetLoad. The explicit Reset command does not.
    return {...RESET_SETTINGS, mix:previous.mix};
  }
  if (!preset.parameters) return normalizeSettings(preset.settings, previous);
  const next = {...previous};
  for (const [key, parameter] of Object.entries(PARAMETERS)) {
    if (parameter.ignoreOnPresetLoad || !Object.hasOwn(preset.parameters, parameter.id)) continue;
    const raw = preset.parameters[parameter.id];
    const value = parameter.values ? parameter.values[raw] : raw;
    next[key] = normalizeControlValue(key, value, previous[key]);
  }
  // Internal calibration values and Auto Makeup are retained in preset data only.
  return next;
}

export function loadUserPresets(value) {
  if (!Array.isArray(value)) return [];
  return value.filter(preset => preset && typeof preset.name === 'string' && preset.name.trim()
    && preset.settings && typeof preset.settings === 'object' && !Array.isArray(preset.settings)
    && (preset.schemaVersion === undefined || preset.schemaVersion === 1 || preset.schemaVersion === PRESET_SCHEMA_VERSION))
    .map(preset => {
      const settings = {...preset.settings};
      // Previous app presets stored 0=FLAT; migrate in memory without overwriting storage.
      if (preset.schemaVersion !== PRESET_SCHEMA_VERSION && Number.isFinite(settings.hifreq)) {
        settings.hifreq = 100 - settings.hifreq;
      }
      return {...preset, schemaVersion:PRESET_SCHEMA_VERSION, settings:normalizeSettings(settings)};
    });
}

// Sparse entries preserve the original asterisks and short, older preset records.
// Auto Makeup is intentionally not connected to a control or DSP stage.
export const FACTORY_PRESETS = Object.freeze([
  {
    "name": "Start Me Up",
    "type": "RealWorld",
    "parameters": {
      "PARAM_METER": 1,
      "PARAM_OUT_GAIN": 5.5,
      "PARAM_GAIN_REDUCTION": 4,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 50,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Full Reset",
    "type": "Reset",
    "parameters": {}
  },
  {
    "name": "PA Default",
    "type": "RealWorld",
    "parameters": {
      "PARAM_THRESH": 10,
      "PARAM_RATIO": 3,
      "PARAM_ATTACK": 3,
      "PARAM_RELEASE": 6,
      "PARAM_ANALOG_ONOFF": 1,
      "PARAM_IN": 1,
      "PARAM_TEST1": 0,
      "PARAM_KNEE": 0,
      "PARAM_TEST2": 2,
      "PARAM_TEST_OUT": 6,
      "PARAM_RELEASE_VAR_LOG": 0.5,
      "PARAM_FEEDBACK": 1,
      "PARAM_MAKEUP": 0,
      "PARAM_MAKEUP_GAIN": 0,
      "PARAM_LINK_FILTER": 0,
      "PARAM_LINK_PERCENT": 6,
      "PARAM_METER": 1,
      "PARAM_CAL1": 0,
      "PARAM_CAL2": 0,
      "PARAM_METER1": 0,
      "PARAM_CAL3": 1,
      "PARAM_CAL4": 2,
      "PARAM_CAL5": 1,
      "PARAM_CAL6": 0,
      "PARAM_SIDE_CHAIN_IN_GAIN": 7,
      "PARAM_SIDE_CHAIN_FEEDBACK": 1,
      "PARAM_SIDE_CHAIN_OUT_GAIN": 0,
      "PARAM_OUT_GAIN": 4,
      "PARAM_ATTACK_GAIN": 1.5,
      "PARAM_RMS_ATTACK": 0,
      "PARAM_RMS_RELEASE": 10.6,
      "PARAM_RELEASE_1": 0.15,
      "PARAM_RELEASE_2": 4.107,
      "PARAM_ATTACK_1": 0.07100000000000001,
      "PARAM_ATTACK_2": 0.711,
      "PARAM_LOW_G0": -7.469999999999998,
      "PARAM_LOW_G1": 4.3,
      "PARAM_DC": 0.002,
      "PARAM_DISTORTION": 1,
      "PARAM_T": 7.42,
      "PARAM_DISTORTION_G": 0,
      "PARAM_DC_OUT": 0.002,
      "PARAM_T_OUT": 0.5,
      "PARAM_DISTORTION_G_OUT": 0,
      "PARAM_DISTORTION_CLIP": -1.31,
      "PARAM_DISTORTION_TOTAL_GAIN_OUT": 0,
      "PARAM_COMPRESSION_DISTORTION": 1,
      "PARAM_DISTORTION_GAIN_AFTER_COMPRESSION": -0.010000000000000009,
      "PARAM_DISTORTION_CLIP_POWER": 30,
      "PARAM_DISTORTION_SMOOTH": 0,
      "PARAM_OUT_ENVELOPE_ATTACK": 0,
      "PARAM_OUT_ENVELOPE_RELEASE": 18.2,
      "PARAM_DISTORTION_POWER": 1,
      "PARAM_ENVELOPE_POWER": 0.74,
      "PARAM_ENVELOPE_POWER_2": 0.045,
      "PARAM_NOISE_FREQ": 100,
      "PARAM_NOISE_Q": 0.948,
      "PARAM_G0": 14,
      "PARAM_G1": 14,
      "PARAM_G2": 14,
      "PARAM_TP_TRESH": 0.08999999999999998,
      "PARAM_TP_GAIN": 1,
      "PARAM_TP_POWER": 2.07,
      "PARAM_GAIN_REDUCTION": 3,
      "PARAM_HIGH_FREQ": 4991.6,
      "PARAM_HIGH_Q": 0.5700000000000001,
      "PARAM_G3": 14,
      "PARAM_G4": 12.8,
      "PARAM_G5": 10,
      "PARAM_NOISE_GAIN": -58,
      "PARAM_G6": 5.8,
      "PARAM_G7": 2.4,
      "PARAM_G8": -2.2,
      "PARAM_G9": -4.5,
      "PARAM_G10": -8.9,
      "PARAM_G11": -20,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_LIMITER_FACTOR": 0,
      "PARAM_G12": -46.5,
      "PARAM_G13": -50,
      "PARAM_G14": -50,
      "PARAM_G15": -50,
      "PARAM_G16": -50,
      "PARAM_G17": -50,
      "PARAM_G18": -50,
      "PARAM_G19": -50,
      "PARAM_G20": -50,
      "PARAM_G21": -50,
      "PARAM_FILTER1_FREQ": 16,
      "PARAM_FILTER1_Q": 0.419,
      "PARAM_FILTER1_G0": 2,
      "PARAM_FILTER1_G1": 0.6000000000000001,
      "PARAM_FILTER1_G2": 0,
      "PARAM_HF_CONTOUR": 50,
      "PARAM_FILTER2_FREQ": 20007,
      "PARAM_FILTER2_Q": 0.409,
      "PARAM_FILTER2_G0": 0,
      "PARAM_FILTER2_G1": 1.1,
      "PARAM_FILTER2_G2": 6,
      "PARAM_FILTER3_FREQ": 48,
      "PARAM_FILTER3_Q": 0.464,
      "PARAM_FILTER3_G0": -1.2999999999999998,
      "PARAM_FILTER3_G1": 0.7000000000000002,
      "PARAM_FILTER3_G2": 0,
      "PARAM_FILTER4_FREQ": 13324,
      "PARAM_FILTER4_Q": 0.5640000000000001,
      "PARAM_FILTER4_G0": 0.30000000000000004,
      "PARAM_FILTER4_G1": -3.2,
      "PARAM_FILTER4_G2": -47.2,
      "PARAM_FILTER5_FREQ": 2612,
      "PARAM_FILTER5_Q": 0.404,
      "PARAM_FILTER5_G0": 0,
      "PARAM_FILTER5_G1": 0.5,
      "PARAM_FILTER5_G2": 0,
      "PARAM_FILTER6_FREQ": 454,
      "PARAM_FILTER6_Q": 0.9980000000000001,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_THRESHOLD": -10.5,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_ON_OFF": 1,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_RATIO": 0.29,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_FREQ": 5050,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_THRESHOLD": -10.5,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_ON_OFF": 1,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_RATIO": 0.48,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_ATTACK": 0,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_RELEASE": 0.2,
      "PARAM_ANALOG": 2,
      "PARAM_G22": -50,
      "PARAM_G23": -50,
      "PARAM_G24": -50,
      "PARAM_G25": -50,
      "PARAM_FILTER7_FREQ": 9990,
      "PARAM_FILTER7_Q": 0.404,
      "PARAM_FILTER7_G0": 0,
      "PARAM_FILTER7_G1": 0,
      "PARAM_FILTER7_G2": 0,
      "PARAM_THD_SUB_FREQ": 40.3,
      "PARAM_THD_SUB_GAIN": 15,
      "PARAM_THD_LOW_FREQ": 169.9,
      "PARAM_THD_LOW_GAIN": 9.9,
      "PARAM_THD_HIGH_FREQ": 7136.3,
      "PARAM_THD_HIGH_GAIN": 8.5,
      "PARAM_THD_IN_GAIN": -34.8,
      "PARAM_THD_THRESH": 0,
      "PARAM_THD_EVEN_GAIN": 0.404,
      "PARAM_THD_OUT_GAIN": 0,
      "PARAM_WET_DRY_GAIN": 100,
      "PARAM_WET_DRY_TC_MS": 10,
      "PARAM_TRIM": 0,
      "PARAM_MAKEUP_GAIN_ONOFF": 1
    }
  },
  {
    "name": "Vox",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 5,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 100,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Sibilant Vox",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 5,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 50,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Rock Piano",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 4.5,
      "PARAM_GAIN_REDUCTION": 4.25,
      "PARAM_COMP_LIMITER": 1,
      "PARAM_HF_CONTOUR": 40,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Acoustic Guitar",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 5,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 90,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Electric Guitar",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 4.5,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 37,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Drum Group",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 5.25,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 100,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Hard Drum Group",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 6,
      "PARAM_GAIN_REDUCTION": 7,
      "PARAM_COMP_LIMITER": 1,
      "PARAM_HF_CONTOUR": 50,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Bass",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 5,
      "PARAM_GAIN_REDUCTION": 4.85,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 100,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Strings",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 4.25,
      "PARAM_GAIN_REDUCTION": 4.25,
      "PARAM_COMP_LIMITER": 1,
      "PARAM_HF_CONTOUR": 100,
      "PARAM_ANALOG": 1
    }
  },
  {
    "name": "Greg Price Kick",
    "type": "RealWorld",
    "parameters": {
      "PARAM_THRESH": 10,
      "PARAM_RATIO": 3,
      "PARAM_ATTACK": 3,
      "PARAM_RELEASE": 6,
      "PARAM_ANALOG_ONOFF": 1,
      "PARAM_IN": 1,
      "PARAM_TEST1": 0,
      "PARAM_KNEE": 0,
      "PARAM_TEST2": 2,
      "PARAM_TEST_OUT": 6,
      "PARAM_RELEASE_VAR_LOG": 0.5,
      "PARAM_FEEDBACK": 1,
      "PARAM_MAKEUP": 0,
      "PARAM_MAKEUP_GAIN": 0,
      "PARAM_LINK_FILTER": 0,
      "PARAM_LINK_PERCENT": 6,
      "PARAM_CAL1": 0,
      "PARAM_CAL2": 0,
      "PARAM_METER1": 0,
      "PARAM_CAL3": 1,
      "PARAM_CAL4": 2,
      "PARAM_CAL5": 1,
      "PARAM_CAL6": 0,
      "PARAM_SIDE_CHAIN_IN_GAIN": 7,
      "PARAM_SIDE_CHAIN_FEEDBACK": 1,
      "PARAM_SIDE_CHAIN_OUT_GAIN": 0,
      "PARAM_OUT_GAIN": 5,
      "PARAM_ATTACK_GAIN": 1.5,
      "PARAM_RMS_ATTACK": 0,
      "PARAM_RMS_RELEASE": 10.6,
      "PARAM_RELEASE_1": 0.15,
      "PARAM_RELEASE_2": 4.107,
      "PARAM_ATTACK_1": 0.07100000000000001,
      "PARAM_ATTACK_2": 0.711,
      "PARAM_LOW_G0": -7.469999999999998,
      "PARAM_LOW_G1": 4.3,
      "PARAM_DC": 0.002,
      "PARAM_DISTORTION": 1,
      "PARAM_T": 7.42,
      "PARAM_DISTORTION_G": 0,
      "PARAM_DC_OUT": 0.002,
      "PARAM_T_OUT": 0.5,
      "PARAM_DISTORTION_G_OUT": 0,
      "PARAM_DISTORTION_CLIP": -1.31,
      "PARAM_DISTORTION_TOTAL_GAIN_OUT": 0,
      "PARAM_COMPRESSION_DISTORTION": 1,
      "PARAM_DISTORTION_GAIN_AFTER_COMPRESSION": -0.010000000000000007,
      "PARAM_DISTORTION_CLIP_POWER": 30,
      "PARAM_DISTORTION_SMOOTH": 0,
      "PARAM_OUT_ENVELOPE_ATTACK": 0,
      "PARAM_OUT_ENVELOPE_RELEASE": 18.2,
      "PARAM_DISTORTION_POWER": 1,
      "PARAM_ENVELOPE_POWER": 0.74,
      "PARAM_ENVELOPE_POWER_2": 0.045,
      "PARAM_NOISE_FREQ": 100,
      "PARAM_NOISE_Q": 0.948,
      "PARAM_G0": 14,
      "PARAM_G1": 14,
      "PARAM_G2": 14,
      "PARAM_TP_TRESH": 0.08999999999999998,
      "PARAM_TP_GAIN": 1,
      "PARAM_TP_POWER": 2.07,
      "PARAM_GAIN_REDUCTION": 5,
      "PARAM_HIGH_FREQ": 4991.6,
      "PARAM_HIGH_Q": 0.5700000000000001,
      "PARAM_G3": 14,
      "PARAM_G4": 12.8,
      "PARAM_G5": 10,
      "PARAM_NOISE_GAIN": -58,
      "PARAM_G6": 5.8,
      "PARAM_G7": 2.4,
      "PARAM_G8": -2.2,
      "PARAM_G9": -4.5,
      "PARAM_G10": -8.9,
      "PARAM_G11": -20,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_LIMITER_FACTOR": 0,
      "PARAM_G12": -46.5,
      "PARAM_G13": -50,
      "PARAM_G14": -50,
      "PARAM_G15": -50,
      "PARAM_G16": -50,
      "PARAM_G17": -50,
      "PARAM_G18": -50,
      "PARAM_G19": -50,
      "PARAM_G20": -50,
      "PARAM_G21": -50,
      "PARAM_FILTER1_FREQ": 16,
      "PARAM_FILTER1_Q": 0.419,
      "PARAM_FILTER1_G0": 2,
      "PARAM_FILTER1_G1": 0.6000000000000001,
      "PARAM_FILTER1_G2": 0,
      "PARAM_HF_CONTOUR": 50,
      "PARAM_FILTER2_FREQ": 20007,
      "PARAM_FILTER2_Q": 0.409,
      "PARAM_FILTER2_G0": 0,
      "PARAM_FILTER2_G1": 1.1,
      "PARAM_FILTER2_G2": 6,
      "PARAM_FILTER3_FREQ": 48,
      "PARAM_FILTER3_Q": 0.464,
      "PARAM_FILTER3_G0": -1.2999999999999998,
      "PARAM_FILTER3_G1": 0.7000000000000002,
      "PARAM_FILTER3_G2": 0,
      "PARAM_FILTER4_FREQ": 13324,
      "PARAM_FILTER4_Q": 0.5640000000000001,
      "PARAM_FILTER4_G0": 0.30000000000000004,
      "PARAM_FILTER4_G1": -3.2,
      "PARAM_FILTER4_G2": -47.2,
      "PARAM_FILTER5_FREQ": 2612,
      "PARAM_FILTER5_Q": 0.404,
      "PARAM_FILTER5_G0": 0,
      "PARAM_FILTER5_G1": 0.5,
      "PARAM_FILTER5_G2": 0,
      "PARAM_FILTER6_FREQ": 454,
      "PARAM_FILTER6_Q": 0.9980000000000001,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_THRESHOLD": -10.5,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_ON_OFF": 1,
      "PARAM_SIDE_CHAIN_LOW_CLIPPER_RATIO": 0.29,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_FREQ": 5050,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_THRESHOLD": -10.5,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_ON_OFF": 1,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_RATIO": 0.48,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_ATTACK": 0,
      "PARAM_SIDE_CHAIN_HIGH_CLIPPER_RELEASE": 0.2,
      "PARAM_ANALOG": 1,
      "PARAM_G22": -50,
      "PARAM_G23": -50,
      "PARAM_G24": -50,
      "PARAM_G25": -50,
      "PARAM_FILTER7_FREQ": 9990,
      "PARAM_FILTER7_Q": 0.404,
      "PARAM_FILTER7_G0": 0,
      "PARAM_FILTER7_G1": 0,
      "PARAM_FILTER7_G2": 0,
      "PARAM_THD_SUB_FREQ": 40.3,
      "PARAM_THD_SUB_GAIN": 15,
      "PARAM_THD_LOW_FREQ": 169.9,
      "PARAM_THD_LOW_GAIN": 9.9,
      "PARAM_THD_HIGH_FREQ": 7136.3,
      "PARAM_THD_HIGH_GAIN": 8.5,
      "PARAM_THD_IN_GAIN": -34.8,
      "PARAM_THD_THRESH": 0,
      "PARAM_THD_EVEN_GAIN": 0.404,
      "PARAM_THD_OUT_GAIN": 0
    }
  },
  {
    "name": "Unity No Comp",
    "type": "RealWorld",
    "parameters": {
      "PARAM_OUT_GAIN": 4,
      "PARAM_GAIN_REDUCTION": 0,
      "PARAM_COMP_LIMITER": 0,
      "PARAM_HF_CONTOUR": 90,
      "PARAM_ANALOG": 1
    }
  }
].map(preset => Object.freeze({...preset, parameters:Object.freeze(preset.parameters)})));

export const RESET_PRESET_INDEX = FACTORY_PRESETS.findIndex(preset => preset.type === 'Reset');
export const STARTUP_SETTINGS = Object.freeze(applyPreset(INITIAL_SETTINGS, FACTORY_PRESETS[0]));

// Thirty original starting points for this recreation, separate from the 14 XML presets.
// Every entry explicitly assigns all existing controls. These are not Waves factory presets.
export const GENERATED_PRESETS = Object.freeze([
  ['Vocal Silk',4.65,3.2,100,100,-0.3,'COMP','OFF','GR'],
  ['Vocal Presence',5.1,4.6,65,100,-0.8,'COMP','OFF','GR'],
  ['Vocal Leveler',5.35,5.6,100,100,-1.2,'COMP','OFF','GR'],
  ['Intimate Vocal',4.8,4.05,85,90,-0.5,'COMP','OFF','GR'],
  ['Rock Vocal',6.1,6.65,75,95,-1.8,'COMP','OFF','GR'],
  ['Vocal Parallel',6.4,7.2,60,45,-0.8,'COMP','OFF','GR'],
  ['Bass Foundation',4.9,4.8,100,100,-0.8,'COMP','OFF','GR'],
  ['Bass Sustain',5.6,6.1,95,100,-1.4,'COMP','OFF','GR'],
  ['Picked Bass',4.8,4.2,50,90,-0.6,'COMP','OFF','GR'],
  ['Sub Bass Glue',4.6,3.4,100,80,-0.5,'COMP','OFF','GR'],
  ['Kick Focus',4.85,4.75,70,100,-0.7,'COMP','OFF','GR'],
  ['Snare Body',5.45,5.5,90,85,-1,'COMP','OFF','GR'],
  ['Drum Bus Glue',4.55,3.1,85,85,-0.4,'COMP','OFF','GR'],
  ['Parallel Drum Crush',6.6,8.2,100,30,-1.5,'LIMITER','OFF','GR'],
  ['Room Smash',7.1,8.7,75,65,-2.5,'LIMITER','OFF','GR'],
  ['Acoustic Warmth',4.55,3.6,90,90,-0.4,'COMP','OFF','GR'],
  ['Acoustic Pick Control',4.95,4.8,45,85,-0.8,'COMP','OFF','GR'],
  ['Electric Guitar Glue',4.75,3.9,100,100,-0.6,'COMP','OFF','GR'],
  ['Guitar Sustain',5.75,6.3,80,95,-1.5,'COMP','OFF','GR'],
  ['Guitar Bus Tamer',4.6,4.35,70,85,-0.8,'LIMITER','OFF','GR'],
  ['Piano Leveler',5.1,5.1,90,100,-0.9,'COMP','OFF','GR'],
  ['Piano Transient Tamer',4.7,4.6,60,85,-0.7,'LIMITER','OFF','GR'],
  ['Rhodes Velvet',4.6,3.25,100,90,-0.4,'COMP','OFF','GR'],
  ['Synth Pad Glue',4.5,2.7,100,100,-0.3,'COMP','OFF','OUT'],
  ['Synth Pluck Control',4.95,5.05,55,80,-0.9,'COMP','OFF','GR'],
  ['Strings Legato',4.65,3.55,100,100,-0.4,'COMP','OFF','GR'],
  ['Brass Tamer',4.85,4.85,65,90,-0.8,'LIMITER','OFF','GR'],
  ['Mix Bus Kiss',4.25,1.85,100,75,-0.2,'COMP','OFF','OUT'],
  ['Vintage 50Hz Glue',4.8,3.8,90,90,-0.6,'COMP','50Hz','GR'],
  ['Vintage 60Hz Punch',5.15,5.2,80,85,-1,'COMP','60Hz','GR'],
].map(([name,gain,peak,hifreq,mix,trim,mode,analog,vu]) => Object.freeze({
  name, type:'Generated', settings:Object.freeze({gain,peak,hifreq,mix,trim,mode,analog,vu}),
})));

export const BUILT_IN_PRESETS = Object.freeze([...FACTORY_PRESETS, ...GENERATED_PRESETS]);
