// Layout files: xml::Document -> ResourceLayout (what LayoutManager::_load builds for a
// <MyGUI type="Layout"> file) -> createLayout -> WidgetManager/FactoryManager and every widget's
// property parser, against the stock MyGUI_Media skins/theme/layers (embedded at build time from
// Media/MyGUI_Media by mayhem/build.sh, served by the in-memory DataManager).
//
// The Gui and the stock media are loaded ONCE per process: reloading ~100 KB of skin XML per input
// made this target ~40 ms/exec and left it at 0 edges in Mayhem. Each input stays isolated from
// the next: the input is parsed into a private ResourceLayout that is never registered with the
// ResourceManager (so it cannot add or replace global skins/fonts/layers), and every root widget it
// created is destroyed afterwards, including those left behind when an exception aborts a layout.
#include "fuzz_common.h"
#include "fuzz_media.h"
#include <set>

namespace
{
	MyGUI::Gui* gGui = nullptr;

	void initOnce()
	{
		if (gGui != nullptr)
			return;
		fuzz::platform();
		auto& files = fuzz::platform().data.files;
		for (const auto& media : fuzz_media::kFiles)
			files[media.name] = std::string(reinterpret_cast<const char*>(media.data), media.size);
		gGui = new MyGUI::Gui();
		gGui->initialise("");
		fuzz::registerFixedFont();
		MyGUI::ResourceManager::getInstance().load("MyGUI_Core.xml");
		fuzz::registerFixedFont();
	}

	std::set<MyGUI::Widget*> rootWidgets()
	{
		std::set<MyGUI::Widget*> result;
		for (auto* widget : gGui->getRootWidgets())
			result.insert(widget);
		return result;
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	initOnce();
	const std::set<MyGUI::Widget*> before = rootWidgets();
	try
	{
		std::istringstream stream(std::string(reinterpret_cast<const char*>(data), size));
		MyGUI::xml::Document document;
		if (document.open(stream))
		{
			MyGUI::ResourceLayout layout(document.getRoot(), "fuzz.layout");
			MyGUI::VectorWidgetPtr widgets = layout.createLayout();
			fuzz::platform().render.render();
			for (auto* widget : widgets)
			{
				widget->setSize(widget->getWidth() / 2 + 1, widget->getHeight() / 2 + 1);
				widget->setVisible(true);
			}
			fuzz::platform().render.render();
			MyGUI::LayoutManager::getInstance().unloadLayout(widgets);
		}
	}
	catch (const MyGUI::Exception&)
	{
	}
	MyGUI::VectorWidgetPtr leftover;
	for (auto* widget : rootWidgets())
		if (before.count(widget) == 0)
			leftover.push_back(widget);
	if (!leftover.empty())
		MyGUI::WidgetManager::getInstance().destroyWidgets(leftover);
	return 0;
}
