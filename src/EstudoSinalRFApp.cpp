#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

EstudoSinalRFApp::EstudoSinalRFApp()
{}

void EstudoSinalRFApp::setup()
{
	mDeviceRect.reset(new DeviceRectImpl());

	try
	{
		//Context
		audio::Context* ctx = audio::Context::master();

		audio::Node::Format nodeFormat;
		nodeFormat.channels(1);
		nodeFormat.autoEnable();

		//Setup
		mInputDeviceNode = ctx->createInputDeviceNode(audio::Device::getDefaultInput(), nodeFormat);
		mAudioInputNode = ctx->makeNode<PCMVideo::AudioInputNode>(nodeFormat);

		//Input nodes
		mInputDeviceNode >> mAudioInputNode >> ctx->getOutput();

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	mFrameEncoder = std::make_shared<PCMVideo::FrameEncoder>();
	mAudioFrame.resize(mAudioInputNode->getFrameSize());

	gl::Texture2d::Format texFormat;
	texFormat.internalFormat(GL_RED);
	texFormat.mipmap(false);
	texFormat.minFilter(GL_NEAREST);
	texFormat.magFilter(GL_NEAREST);
	texFormat.wrapS(GL_CLAMP_TO_BORDER);
	texFormat.wrapT(GL_CLAMP_TO_BORDER);

	mVideoFrame = gl::Texture2d::create(mFrameEncoder->getFrameSize().x, mFrameEncoder->getFrameSize().y, texFormat);
}

void EstudoSinalRFApp::mouseDown(MouseEvent event)
{

}

void EstudoSinalRFApp::update()
{
	auto& rb = mAudioInputNode->getRingBuffer();

	if (mAudioInputNode->getAvailableSeconds() > 1.0f)
	{
		auto& buffer = mAudioInputNode->getBuffer();

		mFrameEncoder->update(buffer);

		//mVideoFrame->update(buffer.getChannel(0u), GL_RED, GL_FLOAT, 0, 128, 245);

		mFrameEncoder->render(mVideoFrame);
	}

	//mFrameEncoder->render(mVideoFrame);
}

void EstudoSinalRFApp::draw()
{
	struct ProgressBar
	{
		std::string label;
		Rectf bounds;
		Color color;
		float progress;
	};

	static auto drawProgressBar = [&](ProgressBar& pg)
	{
		gl::ScopedColor scpColor1(pg.color * 0.25f);
		gl::drawSolidRect(pg.bounds);
		gl::ScopedBlendAdditive scpBlendAdd;
		gl::ScopedColor scpColor2(pg.color);
		gl::ScopedLineWidth scpLineWidth(2.0f);
		gl::drawStrokedRect(pg.bounds);
		gl::drawLine(
			pg.bounds.getUpperLeft() + glm::vec2(pg.bounds.getWidth()*pg.progress, 0.0f),
			pg.bounds.getLowerLeft() + glm::vec2(pg.bounds.getWidth()*pg.progress, 0.0f));
		gl::drawString(pg.label, pg.bounds.getUpperLeft(), pg.color);
	};

	static auto drawFrame = [&](gl::Texture2dRef tex, const Area& src, const Rectf& dst)
	{
		gl::ScopedColor scpColor1(Color::white());
		gl::draw(tex, src, dst);
		gl::ScopedBlendAdditive scpBlendAdd;
		gl::drawStrokedRect(dst);
	};

	gl::clear();

	drawFrame(
		mVideoFrame,
		mVideoFrame->getBounds(),
		Rectf(mVideoFrame->getBounds()).scaled(mFrameEncoder->getOversampleFactor()).getOffset(vec2(0, 48))
	);

	auto& rb = mAudioInputNode->getRingBuffer();
	float readProgress = static_cast<float>(rb.getReadIndex()) / rb.getSize();
	float writeProgress = static_cast<float>(rb.getWriteIndex()) / rb.getSize();

	drawProgressBar(ProgressBar{ 
		"Read",
		Rectf(0, 0, getWindowWidth() / 2, 32),
		Color(0.1f, 0.15f, 1.0f),
		readProgress 
	});

	drawProgressBar(ProgressBar{
		"Write", 
		Rectf(getWindowWidth() / 2, 0, getWindowWidth(), 32),
		Color(0.1f, 0.15f, 1.0f),
		writeProgress
	});
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(1)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
