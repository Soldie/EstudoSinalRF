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
		nodeFormat.autoEnable();

		//Device
		audio::DeviceRef inputDevice = audio::Device::getDefaultInput();
		inputDevice->updateFormat(audio::Device::Format().framesPerBlock(1024).sampleRate(44100u));

		//Nodes
		mInputDeviceNode = ctx->createInputDeviceNode(inputDevice, nodeFormat);
		mAudioInputNode = ctx->makeNode<PCMVideo::AudioInputNode>(nodeFormat);
		mInputDeviceNode >> mAudioInputNode >> ctx->getOutput();

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	mFrameEncoder = std::make_shared<PCMVideo::FrameEncoder>();

	mVideoFrame = gl::Texture2d::create(
		mFrameEncoder->getResolution().x,
		mFrameEncoder->getResolution().y,
		gl::Texture2d::Format()
		.internalFormat(GL_R32F)
		.minFilter(GL_NEAREST)
		.magFilter(GL_NEAREST)
		.wrapS(GL_CLAMP_TO_BORDER)
		.wrapT(GL_CLAMP_TO_BORDER));
}

void EstudoSinalRFApp::mouseDown(MouseEvent event)
{

}

void EstudoSinalRFApp::update()
{
	mFrameEncodeTimer.start();

	if (mAudioInputNode->getAvailableSeconds() > 0.1f)
	{
		auto& line = mAudioInputNode->readLine();

		mFrameEncoder->update(line);

		mFrameEncoder->render(mVideoFrame);
	}

	mFrameEncodeTimer.stop();
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

	static auto draw = [&](ProgressBar& pg)
	{
		gl::ScopedColor scpColor1(pg.color * 0.25f);
		gl::drawSolidRect(pg.bounds);
		//gl::ScopedBlendAdditive scpBlendAdd;
		gl::ScopedColor scpColor2(pg.color);
		gl::ScopedLineWidth scpLineWidth(1.0f);
		gl::drawStrokedRect(pg.bounds);
		gl::drawLine(
			pg.bounds.getUpperLeft() + glm::vec2(pg.bounds.getWidth() * pg.progress, 0.0f),
			pg.bounds.getLowerLeft() + glm::vec2(pg.bounds.getWidth() * pg.progress, 0.0f));
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
		Rectf(mVideoFrame->getBounds()).scaled(uvec2(mFrameEncoder->getBitPeriod(), mFrameEncoder->getFieldPeriod())));

	float readAvailable = static_cast<float>(mAudioInputNode->getAvailableSamples()) / mAudioInputNode->getCapacity();
	float readProgress	= static_cast<float>(mAudioInputNode->getReadIndex()) / mAudioInputNode->getCapacity();
	float writeProgress = static_cast<float>(mAudioInputNode->getWriteIndex()) / mAudioInputNode->getCapacity();
	float cpuLoad = mFrameEncodeTimer.getSeconds();

	Rectf barBounds(getWindowWidth() * 0.75f, 0.0f, getWindowWidth(), 32.0f);
	Color barColor(0.25f, 0.5f, 1.0f);

	draw(ProgressBar{ "Read available", barBounds, barColor, readAvailable });

	barBounds.offset(vec2(0, barBounds.getHeight()));

	draw(ProgressBar{ "Read position", barBounds, barColor, readProgress });

	barBounds.offset(vec2(0, barBounds.getHeight()));

	draw(ProgressBar{ "Write position", barBounds, barColor, writeProgress });

	barBounds.offset(vec2(0, barBounds.getHeight()));

	draw(ProgressBar{ "CPU load = " + std::to_string(0.0f), barBounds, barColor, 0.0f });
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(0)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(30.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
