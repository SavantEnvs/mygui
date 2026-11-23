// TextIterator + UString public API on '#RRGGBB' colour-tag markup (no Gui needed beyond the
// headless context): movement, tag get/set, insert, erase, newline handling, max-length cuts,
// getOnlyText/toTagsString and UTF-8/16/32 conversions; plus the value parsers on slices.
#include "fuzz_common.h"
#include "MyGUI_TextIterator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform(); // LogManager singleton for MYGUI_LOG
	fuzz::Reader r(data, size);
	const uint8_t ops = r.byte();
	const std::string slice = r.str();
	const std::string body = r.rest();
	try
	{
		MyGUI::UString text(body);
		text.asUTF32();
		text.asWStr();
		text.asUTF8();
		MyGUI::TextIterator::getOnlyText(text);
		MyGUI::TextIterator::toTagsString(text);

		MyGUI::VectorChangeInfo history;
		MyGUI::TextIterator it(text, &history);
		size_t steps = 0;
		while (it.moveNext() && steps++ < 4096)
		{
			MyGUI::UString colour;
			it.getTagColour(colour);
			switch ((ops + steps) % 8)
			{
			case 0: it.setTagColour(MyGUI::Colour(0.25f, 0.5f, 0.75f)); break;
			case 1: it.setTagColour(MyGUI::UString(slice)); break;
			case 2: it.saveStartPoint(); break;
			case 3: it.getFromStart(); break;
			case 4: it.eraseFromStart(); break;
			case 5: it.insertText(MyGUI::UString(slice), ops & 1); break;
			case 6: it.clearTagColour(); break;
			default: it.getPosition(); break;
			}
		}
		it.getText();
		it.getSize();
		it.cutMaxLength(ops);
		it.cutMaxLengthFromBeginning(ops / 2);
		MyGUI::TextIterator it2(text);
		it2.setText(MyGUI::UString(slice), ops & 2);
		MyGUI::UString copy(body);
		it2.clearNewLine(copy);
		it2.clearText();

		MyGUI::Colour::parse(slice);
		MyGUI::IntCoord::parse(slice);
		MyGUI::FloatRect::parse(slice);
		MyGUI::Align::parse(slice);
		MyGUI::Version::parse(slice);
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
