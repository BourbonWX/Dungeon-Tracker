# Credits

Felucca is Leo Kuroshita's ([@kurogedelic](https://github.com/kurogedelic), [Hügelton Instruments](https://hugelton.com)). It uses and
thanks the work below; each keeps its own licence (see [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and [LICENSES/](LICENSES/)).

- **[Hügelton Instruments](https://hugelton.com)** (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)):
  Felucca itself; the PHASE engine; the DRUM voices and kits; the Hügelton Sample
  Pack (the drum samples, CC0 1.0); the [Fukiai](https://github.com/hugelton/Fukiai) icon
  font ([MIT](LICENSES/MIT-Fukiai.txt)); the ANALOG, LOFI, TRIO, WHEEL and NOISE engines
  ([Felucca-Engines](https://github.com/hugelton/Felucca-Engines), [MIT](LICENSES/MIT-Felucca-Engines.txt))
- Fonts: [Inter Tight](https://github.com/rsms/inter-tight) by The Inter Project Authors, [SIL OFL 1.1](LICENSES/OFL-InterTight.txt);
  the browser emulator's labels: [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) by The DotGothic16 Project Authors, [SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)
- Samples: [Versilian Studios](https://versilian-studios.com/) [VSCO-2 Community Edition](https://github.com/sgossner/VSCO-2-CE) and [VCSL](https://github.com/sgossner/VCSL), CC0 1.0: the SAMPLE sets, also SLICE's PIANO and PIANO HD; the LOFI KIT is VCSL's drums, shaker and woodblock through `tools/make_lofi_kit.py`; the DUNGEON KIT is VCSL's bass drum, log drum, anvil, brake drum, sleigh bells, claves, ocean drum, tubular bell, ratchet and gong through `tools/make_dungeon_kit.py` ([attribution](assets/samples-cc0/ATTRIBUTION.txt))
- WAVES engine: the oscillator models and waves of [Ambika](https://github.com/pichenettes/ambika) by
  Emilie Gillet, Mutable Instruments (GPL-3.0): `firmware/src/mutable/` (the waves copied verbatim, `eng_waves.c` the
  models written again as a Felucca engine)
- VOICE engine: after [klattsch](https://github.com/tgies/klattsch) by Tony Gies (MIT); formant data from Klatt (1980) and Hillenbrand et al. (1995)
- PHYS engine: models ported from [DaisySP](https://github.com/electro-smith/DaisySP) by Electrosmith and Emilie Gillet ([MIT](LICENSES/MIT-DaisySP.txt)) and from Emilie Gillet's [eurorack](https://github.com/pichenettes/eurorack) code ([MIT](LICENSES/MIT-Rings.txt))
- FM6 engine: msfa from [Dexed](https://github.com/asb2m10/dexed) by Google Inc. and Pascal Gauthier ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt))
- Second core: its start, mailbox and guards from [X0X](https://github.com/charlesvestal/fm1-x0x) by [charlesvestal](https://github.com/charlesvestal)
  with the dual-core findings of [Melodee](https://github.com/keremimo/melodee) by [keremimo](https://github.com/keremimo); its audio split,
  measurements and hardening after [Jangada](https://github.com/zednaked/jangada) by [zednaked](https://github.com/zednaked) (all GPL-3.0)
- Browser emulator: after [X0X](https://github.com/charlesvestal/fm1-x0x) by [charlesvestal](https://github.com/charlesvestal) (GPL-3.0), a Felucca fork whose browser build showed the way
- Package format and boot files: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) ([Apache-2.0](LICENSES/Apache-2.0.txt); three of its files are in every package, none in this tree)
- Contributions: [keremimo](https://github.com/keremimo) (white-key scales, #2), [ChanceTheMaker](https://github.com/ChanceTheMaker)
  (TRS MIDI, bend, sustain and clock, palettes, favourites, editor display settings: #8, #10, #11, #12),
  [andreahaku](https://github.com/andreahaku) (sample recording and trim, #29; SLICE manual slices and tests, #27, #22),
  [spinkham](https://github.com/spinkham) (the boot fix for 0.9 projects, #111; the emulator's MIDI knobs, #182),
  [ugai](https://github.com/ugai) (the Try page's horizontal knob drag, #240),
  [zednaked](https://github.com/zednaked) (ratchets, #100),
  [jasonpersinger](https://github.com/jasonpersinger) (the ROOM reverb click fix, #121),
  [renebohne](https://github.com/renebohne) (MIDI Program Change, #179),
  [gospodindilly-ops](https://github.com/gospodindilly-ops) (the second core, #247)
