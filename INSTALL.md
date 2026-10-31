# Install Diabolus Portative on your OP-XY

No command line, no GitHub workflow: one web page that runs in your browser.

> **Read this first.** Open-XY replaces your OP-XY's operating system with a
> community build. It is not made or supported by teenage engineering.
> Modified firmware can cause data loss or leave the device needing a
> restore. Diabolus Portative has passed its computer-side tests but **has
> not yet been tested on an OP-XY**. Install it only if you are comfortable
> restoring your device.

## What you need

- An OP-XY on **OS 1.2.1** (no other version works).
- A Mac or Windows computer with **Chrome, Edge or Opera** (the page needs Web MIDI and WebUSB).
- A USB cable. On a Mac, [OpenMTP](https://openmtp.ganeshrvel.com/) helps if you copy files yourself; quit Field Kit while you work.

## 1. Back up

Copy your projects, samples and presets off the OP-XY. If you already run an
Open-XY build, also keep a copy of its `/os.ldr` and `/content/op1_factory.json`.

## 2. Download the installer page

Download **`Diabolus-Portative-installer.html`** from the
[latest release](https://github.com/SammiBabe666/Diabolus-Portative/releases/latest)
and open it in Chrome, Edge or Opera (on a Mac: right-click, Open With, Chrome).
It works offline. Your firmware never leaves your computer.

## 3. Follow the page

The page is the Open-XY builder with Diabolus Portative in it. It walks you through:

1. **The OS file.** Choose Mac or Windows, then **Help me get the OS file**.
   The page reads the original OS 1.2.1 from your own OP-XY (or shows you
   where to copy it from), and helps you switch on the dev menu if needed.
   No firmware is included in the download: it is always your own.
2. **Machines.** Diabolus Portative is ticked. You can play it right there
   with the on-screen keys and knobs before installing anything.
3. **Extensions.** Optional; continuing with none is fine.
4. **Install.** The page builds your firmware and guides the install. On a Mac
   it can send the files over USB; on Windows it downloads a ZIP to copy over.
   The last step is on the OP-XY: **"5. write os to boot partition"**, then
   switch it off and on. The boot screen shows `OPEN ...-1.2.1`.

The twelve factory presets are installed with it. Pick Diabolus Portative as
a synth on any track.

**Keep the `layout.json`** from the download: load it into the page next time
so your projects keep finding their machines.

## Already using other Open-XY machines?

This page only holds Diabolus Portative. Installing it replaces your current
Open-XY build, so machines from it (for example Daath) won't be there.
Don't open or save projects that use them until you go back. To combine
machines, use the main builder at [open-xy.com](https://open-xy.com) once
Diabolus Portative is listed there, or build your own collection
(below).

## Going back

- **To your previous Open-XY build:** put your backed-up `/os.ldr` and
  `/content/op1_factory.json` back in file transfer mode, then
  "write os to boot partition" again.
- **To stock:** install teenage engineering's official OS 1.2.1 update, as you
  would any OP-XY update.

## Build it yourself instead

With the [Open-XY SDK](https://github.com/etcetc000/open-xy) beside this
repository (see its [build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)):

```sh
python ../open-xy/sdk.py toolchain fetch
python ../open-xy/sdk.py build --machine . --profiling off --stock-os <your-private-folder>/stock-1.2.1/os.ldr --out <your-private-folder>/diabolus-001
```

Copy the files from `<out>/candidate/` as the build prints, plus this
repository's `presets/*.json` into `/content/presets/diabolus/` (command-line
builds leave presets out). Or make your own installer page with
`python ../open-xy/sdk.py kit --personal --machine . --stock-os ... --out ...`.

## Tell us how it went

Problems or sounds you love: open an
[issue](https://github.com/SammiBabe666/Diabolus-Portative/issues).
