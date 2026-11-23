# UBSan: float-cast-overflow in `MyGUI::texture_utility::toNativeColour`

- **Target:** `fuzz_layout` (also reachable from any skin/layout XML that sets a colour)
- **Reproducer:** `repro.layout`, a TextBox with `TextColour` = `2 0 0`
- **Report:** `MyGUI_TextureUtility.cpp:35:19: runtime error: 510 is outside the range of representable values of type 'unsigned char'`

`Colour::parse` accepts any float, and `toNativeColour` converts `component * 255` straight to
`uint8`. A component outside [0, 1] makes that float-to-integer conversion undefined behaviour.
The colour comes from untrusted layout or skin XML.

**Fix (one line per component):** clamp before converting, e.g.
`uint8(std::clamp(_colour.red, 0.0f, 1.0f) * 255)`.
