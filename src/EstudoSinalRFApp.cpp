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
		mAudioReaderNode = ctx->makeNode<PCMAdaptor::AudioReaderNode>(nodeFormat);
		mAudioWriterNode = ctx->makeNode<PCMAdaptor::AudioWriterNode>(nodeFormat);
		mInputDeviceNode >> mAudioReaderNode >> mAudioWriterNode >> ctx->getOutput();

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	mVideoEncoder = std::make_shared<PCMAdaptor::VideoEncoder>();

	mVideoDecoder = std::make_shared<PCMAdaptor::VideoDecoder>();

	mAudioFrame = ci::audio::Buffer(3u * 245u, 2u);

	mVideoFrame = gl::Texture2d::create(
		mVideoEncoder->getResolution().x,
		mVideoEncoder->getResolution().y,
		gl::Texture2d::Format()
		.internalFormat(GL_R32F)
		.minFilter(GL_NEAREST)
		.magFilter(GL_NEAREST)
		.wrapS(GL_CLAMP_TO_BORDER)
		.wrapT(GL_CLAMP_TO_BORDER));


	auto printSignal = [](std::vector<float>& signal)
	{
		std::copy(signal.begin(), signal.end(), std::ostream_iterator<float>(std::cout));
		std::cout << std::endl;
	};

	std::vector<float> signal(64,0.0f);
	for (auto level = signal.begin(); level != signal.end(); level++)
	{
		size_t n = std::distance(signal.begin(), level);
		*level = static_cast<float>(std::min(std::max((n / 4u) % 2u, 0u), 1u));
	}
	printSignal(signal);

	PCMAdaptor::ClockParser clockParser;
	PCMAdaptor::ClockGenerator clockGenerator(16.0f);

	clockGenerator.reset();
	clockGenerator.triggerEnabled = false;
	clockGenerator.process(&signal);
	printSignal(signal);

	clockGenerator.reset();
	clockGenerator.triggerEnabled = true;
	clockGenerator.process(&signal);
	printSignal(signal);

	clockGenerator.reset();
	clockGenerator.triggerEnabled = false;
	clockGenerator.process(&signal);
	clockParser.reset();
	clockParser.process(&signal);
}

void EstudoSinalRFApp::mouseDown(MouseEvent event)
{

}

void EstudoSinalRFApp::update()
{
	mFrameEncodeTimer.start();

	if (mAudioReaderNode->getAvailableSeconds() > 0.1f)
	{
		mAudioReaderNode->read(mAudioFrame);

		mVideoEncoder->encode(mAudioFrame,mVideoFrame);

		mAudioFrame.zero();

		mVideoDecoder->decode(mVideoFrame,mAudioFrame);

		//mAudioWriterNode->write(mAudioFrame);
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
		Rectf(mVideoFrame->getBounds()).scaled(uvec2(mVideoEncoder->getBitPeriod(), mVideoEncoder->getLinePeriod())));

	float readAvailable = static_cast<float>(mAudioReaderNode->getAvailableSamples()) / mAudioReaderNode->getCapacitySamples();
	float readProgress	= static_cast<float>(mAudioReaderNode->getReadIndex()) / mAudioReaderNode->getCapacitySamples();
	float writeProgress = static_cast<float>(mAudioReaderNode->getWriteIndex()) / mAudioReaderNode->getCapacitySamples();
	float cpuLoad = static_cast<float>(mFrameEncodeTimer.getSeconds());

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
