// Resource XML: xml::Document + ResourceManager::loadFromXmlNode (skins, image sets, manual fonts,
// pointers, layers, layouts, lists, ...). The input is one MyGUI resource file.
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform();
	fuzz::Session session;
	const std::string xml(reinterpret_cast<const char*>(data), size);
	try
	{
		if (!fuzz::loadXml(xml))
			return 0;
		// Exercise what was loaded: instantiate every skin/layout resource once and draw a frame.
		fuzz::registerFixedFont();
		if (!MyGUI::LayerManager::getInstance().isExist("Main"))
			fuzz::createLayer("Main");
		std::vector<std::string> skins;
		for (const auto& item : MyGUI::ResourceManager::getInstance().getResources())
		{
			auto* resource = item.second;
			if (resource->isType<MyGUI::ResourceSkin>() || resource->isType<MyGUI::ResourceLayout>())
				skins.push_back(resource->getResourceName());
			if (auto* font = resource->castType<MyGUI::IFont>(false))
			{
				for (MyGUI::Char c : {MyGUI::Char('A'), MyGUI::Char(' '), MyGUI::Char(0x416)})
					font->getGlyphInfo(c);
			}
			if (auto* set = resource->castType<MyGUI::ResourceImageSet>(false))
			{
				for (size_t g = 0; g < set->getGroups().size(); ++g)
				{
					set->getIndexInfo(g, size_t(0));
					set->getIndexInfo(g, "normal");
				}
			}
		}
		for (const auto& skin : skins)
		{
			auto* widget = MyGUI::Gui::getInstance().createWidget<MyGUI::EditBox>(
				skin, MyGUI::IntCoord(10, 10, 200, 100), MyGUI::Align::Default, "Main");
			widget->setCaption("Fuzz #FF0000text\nline");
		}
		fuzz::platform().render.render();
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
