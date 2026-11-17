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

## Startup diagnostics (0.1.2)

This project uses the Steinberg VST3 SDK and WebView2, not JUCE. There is no
AudioProcessorEditor, BinaryData, APVTS attachment or OpenGLContext to inspect.
Processor/editor constructors do not load textures, start a browser, wait for
threads or take application locks. WebView2 starts asynchronously after the
child HWND is created. Parameter registration now checks all nine IDs.

The release build logs constructor entry/exit, elapsed time, PID/thread ID,
COM HRESULT, environment/controller calls and callbacks, local-resource checks,
navigation and HTML/JavaScript startup milestones. Logs use OutputDebugString
(DebugView/Visual Studio) and `%TEMP%\CLA3AStudio-startup-<PID>.log`.
No diagnostic calls are placed in the audio processing callback. Log-file
access happens only during lifecycle/startup diagnostics, never per sample.
If file writing is denied, debugger output remains available.

Missing module resources, JavaScript errors and rejected promises are reported
to the native editor. A startup timeout names the last observed stage; it does
not establish a deadlock or identify the host-side root cause by itself.
The browser becomes visible after successful navigation, without depending
solely on the JavaScript handshake. It closes before its parent HWND is destroyed.

The Windows CI opens a real editor window, pumps Windows messages and requires
the packaged UI to send `ready` within 30 seconds. The runner installs WebView2
only if needed, verifying the official bootstrapper's Microsoft signature.
This smoke test does not reproduce every DAW's threading, GPU or security policy.

## Editor compatibility update (0.1.3)

The native editor now opens `Resources/web/native.html` via a local file URL.
The build embeds its JavaScript, CSS, fonts and textures, so it makes no request
to `cla3a.local`, loads no separate JavaScript modules and needs no network.
The normal web build is still available in `index.html`.

Browser creation is queued after host attachment; navigation is queued after
the WebView2 controller callback returns. The log records both navigation
entry/return and NavigationStarting, which distinguishes an API call that has
not returned from an asynchronous load that has not completed.

This release uses WebView2 software rendering and a fresh, versioned browser
profile per host executable under `%TEMP%/CLA3AStudio-WebView2-0.1.3/`.
Factory presets, DSP and parameter IDs are unchanged. Browser-saved custom
presets from the former HTTPS origin are not automatically migrated to the
new file origin/profile; the old profile is left intact.

The Windows integration test requires **two editors open simultaneously** to
signal readiness, then closes and reopens one while the other remains open.
Passing on CI does not establish that the reported Studio Pro 8 stall is fixed
on the user's machine; `%TEMP%/CLA3AStudio-startup-<PID>.log` remains available.

## Native loading presentation (0.1.4)

A native brushed-gray loading screen is drawn synchronously before WebView2
startup. It uses GDI and system fonts, with no browser or image dependency.
An opaque child cover remains above the browser while its initial document
loads; a transparent WebView background also prevents a blank document flash.

`ready` now starts host-state synchronization without removing the cover.
After the first host-state update, the page waits for document resources,
fonts and two animation frames, then posts `painted`. Only that signal reveals
the interface. Startup logs distinguish JS readiness, resource/font readiness
and the first rendered-frame handoff. The Windows test checks that the native
loading cover paints non-black pixels before browser startup, and that it is
hidden only after painted readiness for both instances and a reopened editor.

The compatible 0.1.3 browser profile is retained, preserving custom presets
saved there. The loading screen improves the transition, not the underlying
WebView2 cold-start duration. CPU use and DAW-specific timing need separate
measurement; the cover is not a claim that Studio Pro 8 has been validated.

The native UI payload also removes a superseded metal-background reference and
shares the logo mask through a CSS variable, avoiding repeated base64 SVG data.
The texture shapes, DSP and all 44 built-in presets remain unchanged.
