// Headless MyGUI context shared by the fuzz harnesses (modelled on UnitTests/Common and
// UnitTests/UnitTest_Fonts/FontTestContext.h). The platform objects (log, render, data managers)
// live for the whole process and hold no input-derived state; a fresh MyGUI::Gui is initialised
// and shut down around every input, so each input's result depends only on that input.
#ifndef MAYHEM_FUZZ_COMMON_H_
#define MAYHEM_FUZZ_COMMON_H_

#include "MyGUI.h"
#include "MyGUI_DummyRenderManager.h"
#include "MyGUI_DummyDataManager.h"
#include "MyGUI_DataStream.h"
#include "MyGUI_IVertexBuffer.h"
#include "MyGUI_LogManager.h"
#include "MyGUI_OverlappedLayer.h"
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fuzz
{

	// In-memory texture: stores the bytes MyGUI uploads without a graphics driver.
	class Texture : public MyGUI::ITexture
	{
	public:
		explicit Texture(const std::string& _name) : mName(_name) {}
		const std::string& getName() const override { return mName; }
		int getWidth() const override { return mWidth; }
		int getHeight() const override { return mHeight; }
		MyGUI::PixelFormat getFormat() const override { return mFormat; }
		MyGUI::TextureUsage getUsage() const override { return mUsage; }
		size_t getNumElemBytes() const override { return mFormat == MyGUI::PixelFormat::L8A8 ? 2 : 4; }
		bool isLocked() const override { return mLocked; }
		void destroy() override { mPixels.clear(); }
		void setShader(const std::string&) override {}
		void createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) override
		{
			mWidth = _width;
			mHeight = _height;
			mUsage = _usage;
			mFormat = _format;
			mPixels.assign(size_t(_width) * size_t(_height) * getNumElemBytes(), 0);
		}
		void* lock(MyGUI::TextureUsage) override
		{
			mLocked = true;
			return mPixels.data();
		}
		void unlock() override { mLocked = false; }
		// Image decoding belongs to the render platform; pretend every image is 256x256.
		void loadFromFile(const std::string&) override
		{
			mWidth = 256;
			mHeight = 256;
		}
		void saveToFile(const std::string&) override {}

	private:
		std::string mName;
		int mWidth{0};
		int mHeight{0};
		MyGUI::PixelFormat mFormat{MyGUI::PixelFormat::R8G8B8A8};
		MyGUI::TextureUsage mUsage{MyGUI::TextureUsage::Default};
		bool mLocked{false};
		std::vector<MyGUI::uint8> mPixels;
	};

	class VertexBuffer : public MyGUI::IVertexBuffer
	{
	public:
		void setVertexCount(size_t _count) override { mVertices.resize(_count); }
		size_t getVertexCount() const override { return mVertices.size(); }
		MyGUI::Vertex* lock() override { return mVertices.data(); }
		void unlock() override {}
		std::vector<MyGUI::Vertex> mVertices;
	};

	class RenderManager : public MyGUI::DummyRenderManager
	{
	public:
		MyGUI::IVertexBuffer* createVertexBuffer() override { return new VertexBuffer(); }
		void destroyVertexBuffer(MyGUI::IVertexBuffer* _buffer) override { delete _buffer; }
		void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture*, size_t _count) override
		{
			auto* buffer = static_cast<VertexBuffer*>(_buffer);
			// Touch every submitted vertex so ASan sees an out-of-range count.
			for (size_t i = 0; i < _count; ++i)
				checksum += buffer->mVertices.at(i).colour;
		}
		MyGUI::ITexture* createTexture(const std::string& _name) override
		{
			auto& slot = mTextures[_name];
			slot = std::make_unique<Texture>(_name);
			return slot.get();
		}
		void destroyTexture(MyGUI::ITexture* _texture) override
		{
			if (_texture != nullptr)
				mTextures.erase(_texture->getName());
		}
		MyGUI::ITexture* getTexture(const std::string& _name) override
		{
			auto it = mTextures.find(_name);
			return it == mTextures.end() ? nullptr : it->second.get();
		}
		bool isFormatSupported(MyGUI::PixelFormat _format, MyGUI::TextureUsage) override
		{
			return _format == MyGUI::PixelFormat::R8G8B8A8 || _format == MyGUI::PixelFormat::L8A8;
		}
		void render()
		{
			onFrameEvent(0.016f);
			begin();
			onRenderToTarget(this, false);
			end();
		}
		void clearTextures() { mTextures.clear(); }
		MyGUI::uint32 checksum{0};

	private:
		std::map<std::string, std::unique_ptr<Texture>> mTextures;
	};

	// A DataStream that owns its in-memory istringstream.
	class OwnedStream : public MyGUI::DataStream
	{
	public:
		explicit OwnedStream(const std::string& _bytes) : mOwned(_bytes) { mStream = &mOwned; }

	private:
		std::istringstream mOwned;
	};

	// Serves the current input's bytes for one file name; every other file is absent.
	class DataManager : public MyGUI::DummyDataManager
	{
	public:
		MyGUI::IDataStream* getData(const std::string& _name) const override
		{
			auto it = files.find(_name);
			if (it == files.end())
				return nullptr;
			return new OwnedStream(it->second);
		}
		bool isDataExist(const std::string& _name) const override { return files.count(_name) != 0; }
		std::map<std::string, std::string> files;
	};

	struct Platform
	{
		MyGUI::LogManager log;
		RenderManager render;
		DataManager data;
		Platform()
		{
			render.initialise();
			render.setViewSize(800, 600);
		}
	};

	inline Platform& platform()
	{
		static Platform* instance = new Platform();
		return *instance;
	}

	// A fresh Gui per input.
	class Session
	{
	public:
		Session() { gui.initialise(""); }
		~Session()
		{
			gui.shutdown();
			platform().data.files.clear();
			platform().render.clearTextures();
		}
		MyGUI::Gui gui;
	};

	inline bool loadXml(const std::string& _xml, MyGUI::Version _version = MyGUI::Version(1, 2))
	{
		std::istringstream stream(_xml);
		MyGUI::xml::Document document;
		if (!document.open(stream))
			return false;
		MyGUI::ResourceManager::getInstance().loadFromXmlNode(document.getRoot(), "", _version);
		return true;
	}

	inline MyGUI::ILayer* createLayer(const std::string& _name)
	{
		auto* layer = MyGUI::LayerManager::getInstance().createLayerAt(_name, "OverlappedLayer", 0);
		layer->castType<MyGUI::OverlappedLayer>()->setPick(true);
		return layer;
	}

	// Simple fixed-advance font (UnitTests/Common/FixedFont.h).
	class FixedFont : public MyGUI::IFont
	{
	public:
		const MyGUI::GlyphInfo* getGlyphInfo(MyGUI::Char) const override { return &mGlyph; }
		MyGUI::ITexture* getTextureFont() const override { return nullptr; }
		int getDefaultHeight() const override { return 20; }

	private:
		MyGUI::GlyphInfo mGlyph{0, 10, 20, 10};
	};

	inline void registerFixedFont()
	{
		auto* font = new FixedFont();
		font->setResourceName("FuzzFixedFont");
		MyGUI::ResourceManager::getInstance().addResource(font);
		MyGUI::FontManager::getInstance().setDefaultFont(font->getResourceName());
	}

	// Consumes bytes from the front of the input.
	class Reader
	{
	public:
		Reader(const uint8_t* _data, size_t _size) : mData(_data), mSize(_size) {}
		bool empty() const { return mPos >= mSize; }
		uint8_t byte()
		{
			return mPos < mSize ? mData[mPos++] : 0;
		}
		uint32_t u16() { return uint32_t(byte()) | (uint32_t(byte()) << 8); }
		std::string bytes(size_t _max)
		{
			size_t n = std::min<size_t>(_max, mSize - std::min(mPos, mSize));
			std::string s(reinterpret_cast<const char*>(mData + mPos), n);
			mPos += n;
			return s;
		}
		// A length-prefixed string.
		std::string str() { return bytes(byte()); }
		std::string rest() { return bytes(mSize); }

	private:
		const uint8_t* mData;
		size_t mSize;
		size_t mPos{0};
	};

}

#endif
