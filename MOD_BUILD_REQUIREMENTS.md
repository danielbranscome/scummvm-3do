# BUILD_REQUIREMENTS.md — `ScummVM_mod.app` build dependencies

What this fork's `make bundle` target needs on macOS to produce a working
`ScummVM_mod.app`. Read before rebuilding on a fresh Mac (or after a
Homebrew upgrade that may have changed package contents).

This file is specific to the `feature/3do-dialogue-audio` branch on the
`myfork` (`danielbranscome/scummvm-3do`) remote, used by the `011_SH`
project for its narrator-VO mod. Upstream ScummVM has its own build docs;
this exists because the user's environment hit two specific gotchas
documented below.

## Required Homebrew packages

```bash
brew install sdl2 libpng jpeg flac mad faad2 freetype mpg123 libvpx
brew install pkg-config           # only needed if not already present
xcode-select --install            # for clang++ + the macOS SDK
```

The full list of dependencies actually picked up by `./configure` includes
several more (libvorbis, libogg, bzip2, zlib — but those come bundled with
macOS or as transitive Homebrew installs). The list above is the **minimum
explicit set** required for the `bundle` target.

## Build path

```bash
cd 011_SH
./build_scummvm_mod.sh
```

The script:
1. Verifies Xcode CLI tools + Homebrew are present.
2. Installs missing packages from the list above.
3. Runs `./configure --enable-engine=sherlock --disable-all-engines
   --with-sdl-prefix=/opt/homebrew/opt/sdl2 --disable-fluidsynth
   --prefix=/tmp/scummvm_mod_build`.
4. Runs `make -j<ncpu>`.
5. Runs `make bundle`.
6. Renames `ScummVM.app` → `ScummVM_mod.app`, updates `Info.plist`
   display name.

The `--disable-fluidsynth` flag is the trigger for an upstream
`ports.mk` bug — see "Patches applied" below.

## Patches applied to upstream `ports.mk`

Both patches live on `feature/3do-dialogue-audio`. If you sync this fork
against upstream ScummVM later, watch for conflicts here.

### Patch 1 — `libmad.a` / `libfaad.a` not-shipped fallback

Upstream `ports.mk` lines for the `bundle` target reference static
archives at fixed paths:

```makefile
ifdef USE_MAD
OSX_STATIC_LIBS += $(STATICLIBPATH)/lib/libmad.a
endif
```

As of 2026, Homebrew's `mad` and `faad2` formulae ship **only** the
`.dylib` files — no `.a` archive. Upstream's `bundle` target therefore
fails with `clang++: error: no such file or directory:
'/opt/homebrew/lib/libmad.a'`.

**Patch:** make the static path conditional on the file actually existing
(`$(wildcard $(STATICLIBPATH)/lib/libmad.a)`), fall back to `-lmad` (and
`-lfaad`) for dynamic linking if not. The resulting `.app` becomes
Homebrew-dependent on a Mac that runs it — fine for personal-use mod;
if you want a fully-self-contained app, build static archives from source.

### Patch 2 — `USE_TTS` / `-lreadline` blocks miswired under `USE_FLUIDSYNTH`

Upstream `ports.mk` nests the `USE_TTS` (AVFoundation framework) and the
`-lreadline` blocks **inside** the `ifdef USE_FLUIDSYNTH` conditional.
With `--disable-fluidsynth` (this project's configure), neither block
fires. Result: `make bundle` failed at link time with:

```
Undefined symbols for architecture arm64:
  "_AVSpeechUtteranceDefaultSpeechRate", referenced from:
      AVFAudioTextToSpeechManager::startNextSpeech() ...
  "_OBJC_CLASS_$_AVSpeechSynthesizer", ...
ld: symbol(s) not found for architecture arm64
```

**Patch:** lifted both blocks to be siblings of `USE_FLUIDSYNTH` rather
than children of it (they have no logical dependency on FluidSynth).

## Build verification (smoke test)

After `make bundle` succeeds:

1. **Code signature:** `codesign --verify --verbose ScummVM_mod.app` →
   `valid on disk` + `satisfies its Designated Requirement`. (The
   `build_scummvm_mod.sh` workflow modifies `Info.plist` with PlistBuddy
   AFTER the build's codesign step, which invalidates the original
   signature. Re-running `codesign -s - --deep --force ScummVM_mod.app`
   restores it.)

2. **Dynamic-link inspection:** `otool -L
   ScummVM_mod.app/Contents/MacOS/scummvm` should show:

   ```
   /opt/homebrew/opt/mad/lib/libmad.0.dylib
   /opt/homebrew/opt/faad2/lib/libfaad.2.dylib
   /System/Library/Frameworks/AVFoundation.framework/...
   ```

3. **Runtime audio test:** launch the app, point it at the HOLMES game
   directory, start *Sherlock Holmes: Serrated Scalpel*, click on a
   character to trigger `talkTo`. If voice dialogue plays, the
   `b873de86` 3DO-dialogue-audio integration is functional and the
   build is good.

## Reverting to a fully-static build

If a future Homebrew or upstream ScummVM change breaks this fork's
patches, you can bypass them by building static `libmad.a` and
`libfaad.a` from source and dropping them into `/opt/homebrew/lib/`:

```bash
# libmad
curl -L https://downloads.sourceforge.net/mad/libmad-0.15.1b.tar.gz -o /tmp/libmad.tgz
cd /tmp && tar xf libmad.tgz && cd libmad-0.15.1b
./configure --prefix=/opt/homebrew --disable-shared --enable-static
make && sudo cp .libs/libmad.a /opt/homebrew/lib/

# libfaad
curl -L https://github.com/knik0/faad2/archive/refs/tags/2.11.2.tar.gz -o /tmp/libfaad.tgz
cd /tmp && tar xf libfaad.tgz && cd faad2-2.11.2
cmake -DBUILD_SHARED_LIBS=OFF . && make
sudo cp libfaad/libfaad.a /opt/homebrew/lib/
```

After installing, Patch 1 above is a no-op (the wildcard resolves) and
the bundle uses the static archives. Patch 2 is still required.

## File layout at fresh build time

```
scummvm/
├── ports.mk                  # patched (this branch)
├── BUILD_REQUIREMENTS.md     # this file
├── ScummVM_mod.app/          # produced by make bundle + rename (gitignored)
├── scummvm                   # produced by make (gitignored)
├── scummvm-static            # produced by make bundle (gitignored)
└── ...                       # standard ScummVM source tree
```
