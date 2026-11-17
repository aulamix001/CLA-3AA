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
