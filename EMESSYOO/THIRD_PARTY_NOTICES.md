# Third-Party Notices

This document lists external components that ship with or are required to build EMESSYOO (MSU-1 Prep Studio) and summarizes their licensing obligations.

## JUCE Framework

- **Component**: [JUCE](https://juce.com) (modules located in `JUCE/modules` when fetched)
- **License**: Dual-licensed under **AGPLv3** _or_ the commercial [JUCE 8 End User Licence Agreement](https://juce.com/legal/juce-8-licence/). See `JUCE/LICENSE.md` for the upstream notice and dependency list.
- **Usage in EMESSYOO**: JUCE powers the GUI, audio engine, exporters, and tooling. The EMESSYOO source tree expects a JUCE checkout to be placed in `./JUCE` (cloned by `setup.bat` / `setup.sh`).
- **Redistribution Guidance**:
  - If you publish EMESSYOO under the MIT License while statically linking JUCE, you must satisfy JUCE's license. Most open-source users do this by complying with AGPLv3, which requires that the entire combined work be released under AGPLv3 when distributed. If you prefer not to release under AGPLv3, you need a commercial JUCE licence.
  - The GitHub repository does **not** bundle JUCE to avoid redistributing AGPL/commercial code. Contributors should run the setup script to fetch JUCE locally.

## Transitive JUCE Dependencies

JUCE bundles several third-party libraries (FLAC, Ogg Vorbis, zlib, etc.) whose licences are documented in `JUCE/LICENSE.md` and in the respective subdirectories (see "The JUCE Framework Dependencies" section). Review those files if you redistribute JUCE modules yourself.

## Audio Test Assets

Sample audio files used for local tests should not be committed. When shipping example audio, ensure you have redistribution rights for each track.

---

_Last updated: 2025-11-16_
