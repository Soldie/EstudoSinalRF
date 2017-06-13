#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

EstudoSinalRFApp::EstudoSinalRFApp()
{}

void EstudoSinalRFApp::setup()
{
	mVideoEncoder = std::make_shared<PCMAdaptor::Video::Encoder>();

	mVideoDecoder = std::make_shared<PCMAdaptor::Video::Decoder>();

	

	mAudioFrame = ci::audio::Buffer(3u * 245u, 2u);

	auto mVideoTexFormat = gl::Texture2d::Format()
		.internalFormat(GL_R32F)
		.minFilter(GL_NEAREST)
		.magFilter(GL_NEAREST)
		.wrapS(GL_CLAMP_TO_BORDER)
		.wrapT(GL_CLAMP_TO_BORDER);

	mVideoFrame = gl::Texture2d::create(
		mVideoEncoder->getResolution().x,
		mVideoEncoder->getResolution().y,
		mVideoTexFormat);

	mVideoFbo = gl::Fbo::create(640, 490, gl::Fbo::Format().colorTexture(mVideoTexFormat));

	/*
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
	*/

	try
	{
		//Context
		audio::Context* ctx = audio::Context::master();
		audio::Node::Format nodeFormat;
		nodeFormat.autoEnable();

		//Device
		audio::Device::Format deviceFormat;
		deviceFormat.framesPerBlock(128).sampleRate(44100u);
		audio::DeviceRef inputDevice = audio::Device::getDefaultInput();
		inputDevice->updateFormat(deviceFormat);
		audio::DeviceRef outputDevice = audio::Device::getDefaultOutput();
		outputDevice->updateFormat(deviceFormat);
		
		//Nodes
		mAudioInputNode = ctx->createInputDeviceNode(inputDevice, nodeFormat);
		mAudioOutputNode = ctx->createOutputDeviceNode(outputDevice, nodeFormat);
		mAudioReaderNode = ctx->makeNode<PCMAdaptor::Audio::ReaderNode>(nodeFormat);
		mAudioWriterNode = ctx->makeNode<PCMAdaptor::Audio::WriterNode>(nodeFormat);
		mAudioInputNode >> mAudioReaderNode >> mAudioWriterNode >> mAudioOutputNode;

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}


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

		mVideoEncoder->process(mAudioFrame,mVideoFrame);

		//mAudioFrame.zero();

		{
			gl::ScopedFramebuffer scpFbo(mVideoFbo);
			gl::ScopedViewport scpViewport(vec2(), mVideoFbo->getSize());
			gl::ScopedMatrices scpMatrices;
			gl::ScopedColor scpColor;
			gl::setMatricesWindow(mVideoFbo->getSize());
			gl::clear();
			uvec2 videoFrameScale(mVideoEncoder->getBitPeriod(), mVideoEncoder->getLinePeriod());
			gl::draw(mVideoFrame, Rectf(mVideoFrame->getBounds()).scaled(videoFrameScale));
		}

		mVideoDecoder->process(mVideoFbo->getColorTexture(), mAudioFrame);

		mAudioWriterNode->write(mAudioFrame);
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

	static auto draw = [&](ProgressBar& pg, Rectf bounds)
	{
		gl::ScopedColor scpColor1(pg.color * 0.25f);
		gl::drawSolidRect(bounds);
		//gl::ScopedBlendAdditive scpBlendAdd;
		gl::ScopedColor scpColor2(pg.color);
		gl::ScopedLineWidth scpLineWidth(1.0f);
		gl::drawStrokedRect(bounds);
		gl::drawLine(
			bounds.getUpperLeft() + glm::vec2(bounds.getWidth() * pg.progress, 0.0f),
			bounds.getLowerLeft() + glm::vec2(bounds.getWidth() * pg.progress, 0.0f));
		gl::drawString(pg.label, bounds.getUpperLeft(), pg.color);
	};

	static auto drawFrame = [&](gl::Texture2dRef tex, const Area& src, const Rectf& dst)
	{
		gl::ScopedColor scpColor1(Color::white());
		gl::draw(tex, src, dst);
		gl::ScopedBlendAdditive scpBlendAdd;
		gl::drawStrokedRect(dst);
	};

	gl::clear();
	gl::Texture2dRef videoFboTex = mVideoFbo->getColorTexture();
	drawFrame(videoFboTex,videoFboTex->getBounds(),videoFboTex->getBounds());

	float iBufferAvailable = static_cast<float>(mAudioReaderNode->getAvailableSamples()) / mAudioReaderNode->getCapacitySamples();
	float iReadProgress	= static_cast<float>(mAudioReaderNode->getReadIndex()) / mAudioReaderNode->getCapacitySamples();
	float iWriteProgress = static_cast<float>(mAudioReaderNode->getWriteIndex()) / mAudioReaderNode->getCapacitySamples();
	float oBufferAvailable = static_cast<float>(mAudioWriterNode->getAvailableSamples()) / mAudioWriterNode->getCapacitySamples();
	float oReadProgress = static_cast<float>(mAudioWriterNode->getReadIndex()) / mAudioWriterNode->getCapacitySamples();
	float oWriteProgress = static_cast<float>(mAudioWriterNode->getWriteIndex()) / mAudioWriterNode->getCapacitySamples();
	//float cpuLoad = static_cast<float>(mFrameEncodeTimer.getSeconds());

	Rectf barBounds(0.0f, 0.0f, getWindowWidth() * 0.25f, 32.0f);
	Color barColor(0.25f, 0.5f, 1.0f);

	std::vector<ProgressBar> progressBars
	{
		ProgressBar{ "Input.available", barBounds, barColor, iBufferAvailable },
		ProgressBar{ "Input.position.read", barBounds, barColor, iReadProgress },
		ProgressBar{ "Input.position.write", barBounds, barColor, iWriteProgress },
		ProgressBar{ "Output.available", barBounds, barColor, oBufferAvailable },
		ProgressBar{ "Output.position.write", barBounds, barColor, oWriteProgress },
		ProgressBar{ "Output.position.read", barBounds, barColor, oReadProgress }
	};

	if (!progressBars.empty())
	{
		Rectf nextBarBounds(getWindowWidth() * 0.75f, 0.0f, getWindowWidth(), 32.0f);
		for (auto bar : progressBars)
		{
			draw(bar, nextBarBounds);
			nextBarBounds.offset(vec2(0, bar.bounds.getHeight()));
		}
	}
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(0)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(30.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
