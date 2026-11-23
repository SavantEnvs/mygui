// Language files: LanguageManager::loadUserTags (a tag file read through the DataManager), the
// <MyGUI type="Language"> resource route, then replaceTags() on a line taken from the input.
// Input: [u8 n][n bytes: the line to translate][rest: the language/tag file].
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	fuzz::platform();
	fuzz::Session session;
	fuzz::Reader r(data, size);
	const std::string line = r.str();
	const std::string file = r.rest();
	try
	{
		fuzz::platform().data.files["fuzz_tags.xml"] = file;
		auto& language = MyGUI::LanguageManager::getInstance();
		language.loadUserTags("fuzz_tags.xml");
		language.replaceTags(MyGUI::UString(line));
		// The same bytes as a language resource; any <Source> it names resolves to the input too.
		std::istringstream stream(file);
		MyGUI::xml::Document document;
		if (document.open(stream))
		{
			MyGUI::ResourceManager::getInstance().loadFromXmlNode(document.getRoot(), "", MyGUI::Version(1, 2));
		}
		language.replaceTags(MyGUI::UString(line));
		language.getTag(MyGUI::UString(line));
		language.clearUserTags();
	}
	catch (const MyGUI::Exception&)
	{
	}
	return 0;
}
