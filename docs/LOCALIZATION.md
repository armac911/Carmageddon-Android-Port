# Localisation design and contributions

English is the explicit default. Android locale never selects a game language.
Launcher preference `language/code` accepts only en/es/it/fr and is exported on
each Play as DETHRACE_LANGUAGE. The game resets its selection during startup.
There is no change to installed DATA/MUSIC, save formats or original FLIC files.

Edit `localization/catalog.json`, then run `python3 localization/generate.py`.
Commit the JSON, generated native header and Java class together. Catalogue
strings are immutable and bundled into the APK, separate from user game data.
Missing entries return the original string. Never insert printf format specifiers
into an indexed override unless its production call sites explicitly support them;
keep HUD strings short because the original engine uses fixed-size buffers.
The empty-slot message (index 15) must be uppercase ASCII and at most 12
characters: save slots use the original rolling-letter path rather than UTF-8.
`{game}` is the launcher's single substituted field (also used for a numeric size).
Keep it exactly once in translations of a template containing it.

`misc_indices` is limited to meanings audited at their engine call sites. Other
strings use exact English-key lookup. Recognised FLIC button captions are drawn
inside existing mouse hit rectangles after animation processing; unknown captions
or captions that do not fit retain original artwork. Mouse/controller behaviour
is unchanged. The original TRNSLATE.TXT machinery remains available to imported
localised data, and no replacement game font files are distributed.

Accented Latin UTF-8 uses original ASCII glyph shapes plus palette-coloured
composed marks. Clever-text font switching remains separate from UTF-8 byte
sequences. This is an initial font adaptation, not a general Unicode renderer.
Please report accent placement/caption-fit problems with the language, edition,
4:3/16:9 setting and a screenshot. Fonts and wording need device/native-speaker review.

Run `python3 test/android/test_localization.py` for production catalogue/render
checks under AddressSanitizer/UBSan plus launcher Java checks. The CI uses a JDK;
no licensed game assets are required for these tests. Translations were drafted
with AI as part of the project's existing full AI development disclosure.
