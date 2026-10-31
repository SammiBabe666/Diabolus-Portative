# Diabolus Portative: final report

Repository: https://github.com/SammiBabe666/Diabolus-Portative (branch `main`, commits `4afc340` and `5dddf5b`).
Both CI jobs passed on the first commit (Linux tests and preview, Windows native compile).

**Suggested repo description** (GitHub wouldn't let me set it from here, so it needs pasting in by hand):
> A sample-free medieval portative pipe organ for the OP-XY: an Open-XY machine with its own cathedral reverb and delay, 12 dark presets and a floating nun.

## File tree

```
.github/workflows/machine.yml   CI, pinned to SDK commit 892ad936
AGENTS.md  LICENSE (MIT)  README.md
machine.json  definition.json   metadata, 8 params, 12 presets, resources
core.h        Voice layout (7,240 bytes per voice)
engine.cpp    the organ, reverb and delay
display.h / display.cpp         the screen: header, knobs, breath, floating nun and candles
panel.svg.fragment              artwork (generated)
generators/panel.py             draws the artwork
generators/presets.py           writes presets/, definition.json params, tests/presets.h
presets/*.json                  12 factory presets
tests/  sound, presets, continuity, bounds, display (+ check.h, presets.h)
docs/diabolus.gif               screen GIF
docs/diabolus-witch-house.mp4   the witch house tour
docs/REPORT.md, docs/artwork/    this report, artwork drafts
tools/media/                    how the GIF and MP4 were made
```

## Architecture

- **Pipes:** one cosine at the 16' pitch builds all nine partials (16', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1 1/7', 1') with double-angle products, so the ranks are locked in tune. The weights come from STOPS, BODY, BITE and DRIFT, ramp every 8 samples, and are normalized.
- **Speech:** the envelope is folded into the weights, with upper partials speaking first. The chiff is filtered noise shaped by a difference of two decays.
- **Effects, inside the machine:** they run at 1/8 rate (5.5 kHz) after a 2.2 kHz prefilter.
  - **Delay:** 320 ms, with a damped loop and level-limited feedback (85% at most).
  - **Reverb:** a 4-line Hadamard FDN with 2 allpass diffusers and 1.5 kHz damping. RT runs from 2.2 to 6.5 s.
  - **Output:** the wet signal is interpolated back to 44.1 kHz and passed through a soft-knee limiter.
- **Voices:** the reverb and delay are per voice, because synths get no shared bus.
- **Code:** fixed-point only, with no float and no division in the audio path.

## Parameters

| Page 1 | Page 2 |
| --- | --- |
| STOPS: ranks drawn | ATTACK: 3 ms to 1.5 s |
| BODY: hollow to full + 16' | REVERB: chapel to cathedral |
| BITE: soft to nasal | DELAY: echo level |
| DRIFT: beating + wind wander | FEEDBACK: 0 to 85% |

## Presets

Vespers (default), Devils March, Ashen, Black Cantor, Reliquary, Unlit, Adversary, Midnight, Endless, Hollow Saint, Last Office, Diabolus.

The apostrophe was dropped from "Devil's" because the SDK rejects it in labels. Black Cantor, Unlit, Endless and Last Office turn on the stock tremolo LFO at its stock settings.

## Display

The screen is white line art on black, with no grey. It has:
- a two-page header with real units (ms/s for ATTACK, % for FEEDBACK);
- three Gothic bays, lancet windows, the organ with 14 pipes, a horned mask and bellows;
- a nun at the keys;
- five candles on prickets;
- the DIABOLUS PORTATIVE title.

Every control shows on screen: STOPS lights the knobs, BODY raises a 16' bourdon, BITE grows the mask's horns, DRIFT leans the candles, ATTACK opens the bellows, REVERB fills the vault with arcs, DELAY and FEEDBACK send echoes. While notes are held, breath rises from the pipes, the nun lifts up to 14 px and rocks, and the candles (each one outline with its flame) float 13 to 24 px, bob and sway. On average 18 nodes change per frame while playing, none at rest, within the Open-XY guide's budget of about 20. Everything settles exactly back about half a second after release. Reduced motion keeps it all still.

## Measured vs estimated

**Measured on a computer:**
- **Host tests pass.** All 5 test files (sound, presets, continuity, bounds, display) pass through `sdk.py test`.
- **Native Blackfin compile:** 153.0 of 2032 KiB of module memory (Diabolus 85.9 KiB).
- **MP4 audio:** no dropouts. Every 50 ms window of the encoded audio is within ±2% of the source. The only sharp onsets in the organ are the designed chiff.

**Estimated only:**
- **CPU:** the SDK's host estimate is unreliable in this cloud container. The same unchanged engine read anywhere from 10,000 to 23,000 instructions per voice block across runs, against an aim of 15,000. My direct timing of the engine hasn't changed since the earlier runs that read about 11,000.
- **The real number** comes from the hardware CPU test kit.

**Not tested on hardware:** nothing has run on an OP-XY yet. That includes the sound, the screen and the CPU cost.

## Commands

```sh
python ../open-xy/sdk.py test --machine . --host-cxx g++
python ../open-xy/sdk.py preview --machine . --out ../previews/diabolus-001
python ../open-xy/sdk.py compile --machine . --out ../builds/diabolus-module
python ../open-xy/sdk.py build --machine . --profiling on --stock-os <private>/stock-1.2.1/os.ldr --out <private>/diabolus-cpu-001
python generators/presets.py .   # presets
python generators/panel.py .     # artwork
```

## Blockers and risks

- **Hardware CPU pending.** Run the kit three times and send back the JSON files. If the heaviest setting is over 3.5%, the effects rate or rank count is where to cut.
- **Firmware risk.** Installing a development OS can leave the OP-XY needing a restore. Back up `/os.ldr`, `/content/op1_factory.json` and your projects first. It's built for OS 1.2.1 only.
- **The kit's os.ldr contains your firmware.** Keep it private. It is not in the repository or the archive.
- **The MP4 approximates the device.** The stock amp release and tremolo are emulated, so the device will differ slightly.

## Open-XY guideline update (SDK PR #260)

Diabolus follows the device and screen rules from etcetc000/open-xy#260:
outlines enclose area, no turns near straight back, points at least 1/8 px
apart, every path one subpath of at most 64 points, parts that meet share an
outline or a pivot, text at least 8 px, captions that fit their column, every
control visible on screen, a redraw of about 20 nodes a frame at most, and
local copies of the SDK's inline sine and rotation so it links beside other
machines. The CPU test kit restarts the OP-XY between runs, as the updated
profiling guide requires. Still open from that checklist: photos and a CPU
readout from a real OP-XY.
