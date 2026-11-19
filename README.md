# KLUAY - Vocal Stutters. Faster.

VST3 instrument for FL Studio (Windows / macOS). Loads a vocal, you pick a word,
it builds a beat-synced stutter (repeats, rate, fade in, pan, phone / chorus / backverb)
and you drag the result into the playlist.

## Build (no software needed)
1. Create a Public repository on github.com
2. Upload everything in this folder (including the hidden-looking `.github` folder)
3. Open the Actions tab -> Build -> wait ~15 min
4. Download artifact KLUAY-Windows (or KLUAY-macOS) -> contains KLUAY.vst3

## Install
Windows: copy KLUAY.vst3 to C:\Program Files\Common Files\VST3
macOS:   copy to ~/Library/Audio/Plug-Ins/VST3  (then: xattr -cr KLUAY.vst3)
FL Studio: Options > Manage plugins > Find installed plugins. KLUAY is under Generators.
