// Text layout and rendering: TextBox (SimpleText) and a multi-line word-wrapping EditBox (EditText)
// with the stock generated manual font (Media/MyGUI_Media/MyGUI_GeneratedFonts.xml, embedded), fed a
// caption with colour tags and #{language} tags. Reaches TextView line breaking, EditText vertex
// generation, LanguageManager::replaceTags and ResourceManualFont glyph lookup.
// Input: [u8 width][u8 height][u8 align][u8 font height][u8 flags][rest: caption]
#include "fuzz_common.h"
#include "fuzz_media.h"

namespace
{
	const char* kSkin = R"(<MyGUI type="Resource" version="1.1">
	<Resource type="ResourceSkin" name="FuzzSimpleText" size="200 80">
		<BasisSkin type="SimpleText" offset="0 0 200 80" align="Stretch"/>
	</Resource>
	<Resource type="ResourceSkin" name="FuzzEditClient" size="200 80">
		<BasisSkin type="EditText" offset="0 0 200 80" align="Stretch"/>
	</Resource>
	<Resource type="ResourceLayout" name="FuzzEditBox" version="3.2.0">
		<Widget type="Widget" skin="Default" position="0 0 200 80" name="Root">
			<Widget type="TextBox" skin="FuzzEditClient" position="0 0 200 80" align="Stretch" name="Client"/>
		</Widget>
	</Resource>
</MyGUI>)";
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform();
	fuzz::Session session;
	fuzz::Reader r(data, size);
	const int width = r.byte() + 1, height = r.byte() + 1;
	const MyGUI::Align align(MyGUI::Align::Enum(r.byte() & MyGUI::Align::Stretch));
	const int fontHeight = r.byte() % 64;
	const uint8_t flags = r.byte();
	const MyGUI::UString caption(r.rest());
	try
	{
		auto& files = fuzz::platform().data.files;
		for (const auto& media : fuzz_media::kFiles)
			files[media.name] = std::string(reinterpret_cast<const char*>(media.data), media.size);
		MyGUI::ResourceManager::getInstance().load("MyGUI_GeneratedFonts.xml");
		fuzz::loadXml(kSkin, MyGUI::Version(1, 1));
		fuzz::createLayer("Main");
		MyGUI::LanguageManager::getInstance().addUserTag("Tag", "#00FF00tag#{Other}");
		MyGUI::LanguageManager::getInstance().addUserTag("Other", "other\nline");
		const std::string font = "DejaVuSansFontGenerated_15";

		auto* text = session.gui.createWidget<MyGUI::TextBox>(
			"FuzzSimpleText", MyGUI::IntCoord(0, 0, width, height), MyGUI::Align::Default, "Main");
		text->setFontName(font);
		text->setFontHeight(fontHeight);
		text->setTextAlign(align);
		text->setTextShadow(flags & 1);
		text->setCaptionWithReplacing(caption.asUTF8());
		text->getTextSize();

		auto* edit = session.gui.createWidget<MyGUI::EditBox>(
			"FuzzEditBox", MyGUI::IntCoord(0, 0, width, height), MyGUI::Align::Default, "Main");
		edit->setFontName(font);
		edit->setFontHeight(fontHeight);
		edit->setEditMultiLine(flags & 2);
		edit->setEditWordWrap(flags & 4);
		edit->setEditPassword(flags & 8);
		edit->setTextAlign(align);
		edit->setCaption(caption);
		edit->getTextSize();
		fuzz::platform().render.render();
		text->setSize(height, width);
		edit->setSize(height, width);
		edit->setTextCursor(caption.size() / 2);
		fuzz::platform().render.render();
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
