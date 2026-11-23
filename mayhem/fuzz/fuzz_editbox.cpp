// EditBox public API driven by an op stream: captions with colour tags, insert/erase/colour
// intervals, selection, max length, password/multiline/word wrap, undo/redo and injected keys.
// Reaches MyGUI_EditBox.cpp, MyGUI_TextIterator.cpp, EditText and TextView.
#include "fuzz_common.h"

namespace
{
	const char* kSkin = R"(<MyGUI type="Resource" version="1.1">
	<Resource type="ResourceSkin" name="FuzzEditClient" size="200 80">
		<BasisSkin type="EditText" offset="0 0 200 80" align="Stretch"/>
	</Resource>
	<Resource type="ResourceLayout" name="FuzzEditBox" version="3.2.0">
		<Widget type="Widget" skin="Default" position="0 0 200 80" name="Root">
			<Widget type="TextBox" skin="FuzzEditClient" position="0 0 200 80" align="Stretch" name="Client">
				<Property key="FontName" value="FuzzFixedFont"/>
				<Property key="FontHeight" value="20"/>
				<Property key="TextAlign" value="Left Top"/>
			</Widget>
		</Widget>
	</Resource>
</MyGUI>)";

	MyGUI::UString text(fuzz::Reader& r)
	{
		return MyGUI::UString(r.str());
	}

	size_t index(fuzz::Reader& r)
	{
		uint32_t v = r.u16();
		return v == 0xFFFF ? MyGUI::ITEM_NONE : size_t(v);
	}

	void key(MyGUI::KeyCode _key, MyGUI::Char _char = 0)
	{
		auto& input = MyGUI::InputManager::getInstance();
		input.injectKeyPress(_key, _char);
		input.injectKeyRelease(_key);
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform();
	fuzz::Session session;
	try
	{
		fuzz::createLayer("Main");
		fuzz::registerFixedFont();
		fuzz::loadXml(kSkin, MyGUI::Version(1, 1));
		auto* edit = session.gui.createWidget<MyGUI::EditBox>(
			"FuzzEditBox", MyGUI::IntCoord(0, 0, 200, 80), MyGUI::Align::Default, "Main");
		auto& input = MyGUI::InputManager::getInstance();
		input.setKeyFocusWidget(edit);
		fuzz::Reader r(data, size);
		for (int ops = 0; !r.empty() && ops < 256; ++ops)
		{
			switch (r.byte() % 24)
			{
			case 0: edit->setCaption(text(r)); break;
			case 1: { auto t = text(r); edit->insertText(t, index(r)); break; }
			case 2: edit->addText(text(r)); break;
			case 3: { size_t s = index(r); edit->eraseText(s, index(r)); break; }
			case 4:
			{
				size_t s = index(r), c = index(r);
				edit->setTextIntervalColour(s, c, MyGUI::Colour(r.byte() / 255.f, r.byte() / 255.f, r.byte() / 255.f));
				break;
			}
			case 5: { size_t s = index(r); edit->setTextSelection(s, index(r)); break; }
			case 6: edit->deleteTextSelection(); break;
			case 7: edit->getTextSelection(); break;
			case 8: { size_t s = index(r); edit->getTextInterval(s, index(r)); break; }
			case 9: edit->setTextCursor(index(r)); break;
			case 10: edit->setMaxTextLength(r.u16()); break;
			case 11: edit->setEditPassword(r.byte() & 1); break;
			case 12: edit->setEditMultiLine(r.byte() & 1); break;
			case 13: edit->setEditWordWrap(r.byte() & 1); break;
			case 14: edit->setEditReadOnly(r.byte() & 1); break;
			case 15: edit->setOverflowToTheLeft(r.byte() & 1); break;
			case 16: edit->setTextSelectionColour(MyGUI::Colour(r.byte() / 255.f, 0.5f, 0.5f)); break;
			case 17:
			{
				static const MyGUI::KeyCode::Enum keys[] = {
					MyGUI::KeyCode::Delete, MyGUI::KeyCode::Backspace, MyGUI::KeyCode::ArrowLeft,
					MyGUI::KeyCode::ArrowRight, MyGUI::KeyCode::ArrowUp, MyGUI::KeyCode::ArrowDown,
					MyGUI::KeyCode::Home, MyGUI::KeyCode::End, MyGUI::KeyCode::PageUp, MyGUI::KeyCode::PageDown,
					MyGUI::KeyCode::Return, MyGUI::KeyCode::Tab};
				key(keys[r.byte() % (sizeof(keys) / sizeof(keys[0]))]);
				break;
			}
			case 18:
			{
				// Ctrl/Shift + key: undo, redo, select all, copy, cut, paste, and shift-selection moves.
				static const MyGUI::KeyCode::Enum keys[] = {
					MyGUI::KeyCode::Z, MyGUI::KeyCode::Y, MyGUI::KeyCode::A, MyGUI::KeyCode::C,
					MyGUI::KeyCode::X, MyGUI::KeyCode::V, MyGUI::KeyCode::ArrowLeft, MyGUI::KeyCode::End};
				uint8_t b = r.byte();
				MyGUI::KeyCode mod = (b & 0x80) ? MyGUI::KeyCode::LeftShift : MyGUI::KeyCode::LeftControl;
				input.injectKeyPress(mod);
				key(keys[b % (sizeof(keys) / sizeof(keys[0]))]);
				input.injectKeyRelease(mod);
				break;
			}
			case 19: key(MyGUI::KeyCode::None, MyGUI::Char(r.u16() | (uint32_t(r.byte() & 0x1F) << 16))); break;
			case 20: edit->setSize(int(r.byte()) + 1, int(r.byte()) + 1); break;
			case 21: edit->setFontHeight(int(r.byte() % 64)); break;
			case 22: fuzz::platform().render.render(); break;
			case 23: edit->setTextAlign(MyGUI::Align(MyGUI::Align::Enum(r.byte() & MyGUI::Align::Stretch))); break;
			}
		}
		// Read the state back.
		edit->getCaption();
		edit->getOnlyText();
		edit->getTextLength();
		fuzz::platform().render.render();
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
