# CLA-3A Studio

A responsive, interactive recreation of the CLA-3A compressor interface, built with vanilla JavaScript, CSS, SVG, and Vite. The compact 640 × 390 panel follows the supplied reference, with proportional scaling on smaller screens.

## Run

```sh
npm ci
npm run dev
```

The development server listens on port 5173 and all network interfaces. For restricted home directories, use `npm ci --cache /tmp/cla-npm-cache`.

```sh
npm run build
npm run preview
```

## Controls

- Drag knobs vertically or horizontally. Hold Shift for fine adjustments; double-click to reset.
- Keyboard: focus a knob and use arrow keys; Home/End select its minimum/maximum.
- Navigate factory presets with the arrows or preset dropdown.
- Setup A/B compares two independent settings slots; A→B or B→A copies the current slot.
- Undo/redo records changes. Ctrl/Cmd+Z undoes; Ctrl/Cmd+Shift+Z redoes.
- Save stores named presets in this browser's localStorage.
- VU Display selects input, gain reduction, or output metering.
- The top-right menu opens an audio demo with locally generated keys, bass, or drums. Bypass compares the dry signal. Closing the demo stops playback.

## Audio scope

This is an interface recreation, not an official Waves product or an emulation of proprietary Waves DSP. The optional audio demonstration uses synthesized sources and the browser's native DynamicsCompressorNode. Gain, Peak Reduction, COMP/LIMITER, Mix, Trim, and Analog affect the demo. HI FREQ approximates a change in compressor sensitivity; it is not a frequency-selective sidechain implementation. The VU needle reads live compressor reduction or analyser levels during playback. No microphone access, uploads, or external audio files are required.

Fonts are hosted locally; their SIL Open Font License notices are in `public/fonts/`.

## Validation

Production build verified with `npm run build`. Chromium smoke checks covered knob adjustment, undo/redo, comparison copying, presets, save persistence across reload, switches, audio start/stop, bypass, and desktop/mobile layouts (1440px and 390px) without horizontal overflow or JavaScript errors.

## VST3 0.1.1: parameter and meter mapping

The native meter reads the processed DAW signal, not knob positions. With audio
playing, **GR** shows compressor attenuation in dB: no reduction rests at 0;
increasing Peak Reduction moves the needle left as attenuation increases.
The needle follows the printed nonlinear dB scale. **IN/OUT** use RMS levels,
calibrated so -18 dBFS RMS equals 0 VU. Values below -20 VU or above +3 VU
reach the scale limits. The GR scale reaches its limit at 20 dB reduction.

- Gain is post-compression makeup: 4.08 is unity; one unit adds 6 dB.
- Peak Reduction lowers the detector threshold; it does not indicate dB itself.
- HI FREQ at 100 is flat. Lower values emphasize high-frequency detector input
  and reduce low-frequency sensitivity; this is not an audio-path EQ.
- COMP uses a 3:1 ratio; LIMITER uses 100:1 with a narrower knee.
- Mix blends dry/wet before Trim. At 0%, OUT is dry (plus Trim), while GR
  still measures compression inside the wet path.
- Trim adjusts final output by -18 to +18 dB, without changing IN or GR.
- Analog selects intentional 50/60 Hz hum or OFF. Bypass passes the dry signal
  and indicates zero GR. Host automation uses discrete switch values.

The browser audio demo is a separate Web Audio approximation, not the native
DSP. Both use the same needle-scale calibration. Native startup now displays
loading/failure information instead of leaving only an unexplained black area;
this does not establish a loading-time improvement on any particular DAW.

Run `node --test tests/*.test.js` for preset and printed-meter calibration tests.
The native suite additionally sweeps Peak Reduction and checks GR against
measured attenuation, IN/OUT calibration, Gain, Trim, Mix, bypass and HF contour.
