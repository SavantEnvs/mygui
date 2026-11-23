// TrueType fonts: a <Resource type="ResourceTrueTypeFont"> whose Source is the fuzzed font file,
// served by the in-memory DataManager. ResourceTrueTypeFont::initialise loads the face with FreeType
// (FT_New_Memory_Face), renders the requested code ranges into the glyph atlas, computes kerning,
// tab/space widths and the substitute glyph; then glyphs are looked up and text is laid out.
// Input: [u8 size][u8 resolution][u8 flags][u8 code range start][u8 code range length][rest: font file]
#include "fuzz_common.h"
#include "MyGUI_ResourceTrueTypeFont.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform();
	fuzz::Session session;
	fuzz::Reader r(data, size);
	const int fontSize = r.byte() % 48 + 1;
	const int resolution = r.byte() % 4 * 48 + 48;
	const uint8_t flags = r.byte();
	const int first = 32 + r.byte();
	const int count = r.byte() % 128;
	static const char* hinting[] = {"use_native", "force_auto", "disable_auto", "disable_all"};
	try
	{
		fuzz::platform().data.files["fuzz.ttf"] = r.rest();
		std::ostringstream xml;
		xml << "<MyGUI type=\"Resource\"><Resource type=\"ResourceTrueTypeFont\" name=\"FuzzFont\">"
			<< "<Property key=\"Source\" value=\"fuzz.ttf\"/>"
			<< "<Property key=\"Size\" value=\"" << fontSize << "\"/>"
			<< "<Property key=\"Resolution\" value=\"" << resolution << "\"/>"
			<< "<Property key=\"AutoDpi\" value=\"false\"/>"
			<< "<Property key=\"Antialias\" value=\"" << ((flags & 1) ? "true" : "false") << "\"/>"
			<< "<Property key=\"Kerning\" value=\"" << ((flags & 2) ? "true" : "false") << "\"/>"
			<< "<Property key=\"Hinting\" value=\"" << hinting[(flags >> 2) & 3] << "\"/>"
			<< "<Property key=\"OffsetHeight\" value=\"" << int((flags >> 4) & 3) - 1 << "\"/>"
			<< ((flags & 0x40) ? "<Property key=\"SubstituteCode\" value=\"63\"/>" : "")
			<< "<Codes><Code range=\"" << first << " " << first + count << "\"/>"
			<< ((flags & 0x80) ? "<Code range=\"1025 1105\"/><Code hide=\"128 159\"/>" : "<Code range=\"63\"/>")
			<< "</Codes></Resource></MyGUI>";
		fuzz::loadXml(xml.str(), MyGUI::Version(1, 1));
		auto* resource = MyGUI::FontManager::getInstance().getByName("FuzzFont");
		if (resource == nullptr)
			return 0;
		auto* font = resource->castType<MyGUI::ResourceTrueTypeFont>(false);
		if (font == nullptr)
			return 0;
		font->getDefaultHeight();
		font->getTextureFont();
		for (int c = first; c <= first + count; ++c)
			font->getGlyphInfo(MyGUI::Char(c));
		font->getGlyphInfo(MyGUI::Char(0x416));
		font->getGlyphInfo(MyGUI::Char(' '));
		font->getGlyphInfo(MyGUI::Char('\t'));

		// Lay out and draw a line in the font.
		const char* skin = R"(<MyGUI type="Resource"><Resource type="ResourceSkin" name="FuzzText" size="400 100">
			<BasisSkin type="SimpleText" offset="0 0 400 100" align="Stretch"/></Resource></MyGUI>)";
		fuzz::loadXml(skin);
		fuzz::createLayer("Main");
		auto* text = session.gui.createWidget<MyGUI::TextBox>(
			"FuzzText", MyGUI::IntCoord(0, 0, 400, 100), MyGUI::Align::Default, "Main");
		text->setFontName("FuzzFont");
		text->setCaption("AV Wa?\tTo \xD0\x96 fi");
		text->getTextSize();
		fuzz::platform().render.render();
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
