#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

EstudoSinalRFApp::EstudoSinalRFApp()
{}

void EstudoSinalRFApp::setup()
{
	mVideoContext = std::make_shared<PCMAdaptor::Context>();
	mVideoEncoder = std::make_shared<PCMAdaptor::Video::Encoder>(mVideoContext.get());
	mVideoDecoder = std::make_shared<PCMAdaptor::Video::Decoder2>(mVideoContext.get());

	auto inputSize = mVideoContext->getInputSize();
	auto outputSize = mVideoContext->getOutputSize();

	auto texFormat = gl::Texture2d::Format()
		.internalFormat(GL_R32F)
		.minFilter(GL_NEAREST)
		.magFilter(GL_NEAREST)
		.wrapS(GL_CLAMP_TO_BORDER)
		.wrapT(GL_CLAMP_TO_BORDER);

	mAudioFrame = ci::audio::Buffer(mVideoContext->getBlocksPerLine() * inputSize.y, mVideoContext->getChannels());

	mVideoFrame = gl::Texture2d::create(inputSize.x, inputSize.y, texFormat);

	mVideoOutputFbo = gl::Fbo::create(outputSize.x, outputSize.y, gl::Fbo::Format().colorTexture(texFormat));

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
		mAudioWriterNode = ctx->makeNode<PCMAdaptor::Audio::WriterNode>(nodeFormat.autoEnable(false));
		mAudioInputNode >> mAudioReaderNode >> mAudioWriterNode >> mAudioOutputNode;

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	mAudioEnableTimer.start();
}

void EstudoSinalRFApp::mouseDown(MouseEvent event)
{

}

void EstudoSinalRFApp::update()
{
	audio::Context* ctx = audio::Context::master();

	/*
	if (!mAudioEnableTimer.isStopped())
	{
		if (mAudioEnableTimer.getSeconds() > 1.0f)
		{
			if (!ctx->isEnabled())
			{
				ctx->enable();
				mAudioEnableTimer.stop();
			}
		}
	}
	*/

	if (ctx->isEnabled())
	{
		mFrameEncodeTimer.start();

		if (mAudioReaderNode->getAvailableSeconds() > 0.01f)
		{
			mAudioReaderNode->read(mAudioFrame);

			//TODO: transformar entrada audio::Buffer para std::vector<float>

			mVideoEncoder->process(mAudioFrame, mVideoFrame);

			//TODO: transformar saída std::vector<float> para gl::Texture2d e retornar tamanho escrito

			{
				gl::ScopedFramebuffer scpFbo(mVideoOutputFbo);
				gl::ScopedViewport scpViewport(mVideoOutputFbo->getSize());
				gl::ScopedMatrices scpMatrices;
				gl::ScopedColor scpColor;
				gl::setMatricesWindow(mVideoOutputFbo->getSize());
				gl::clear();
				auto bitSize = mVideoContext->getBitSize();
				gl::draw(mVideoFrame, Rectf(mVideoFrame->getBounds()).scaled(bitSize));
			}

			mVideoDecoder->process(mVideoOutputFbo->getColorTexture(), mAudioFrame);

			if (!mAudioWriterNode->isEnabled())
			{
				mAudioWriterNode->enable();
			}

			mAudioWriterNode->write(mAudioFrame);
		}

		mFrameEncodeTimer.stop();
	}
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

	static auto drawProgressBar = [&](ProgressBar& pg, Rectf bounds)
	{
		gl::ScopedColor scpColor;
		gl::color(pg.color * 0.25f);
		gl::drawSolidRect(bounds);
		//gl::ScopedBlendAdditive scpBlendAdd;
		gl::color(pg.color);
		gl::ScopedLineWidth scpLineWidth(1.0f);
		gl::drawStrokedRect(bounds);
		gl::drawLine(
			bounds.getUpperLeft() + glm::vec2(bounds.getWidth() * pg.progress, 0.0f),
			bounds.getLowerLeft() + glm::vec2(bounds.getWidth() * pg.progress, 0.0f));
		gl::drawString(pg.label, bounds.getUpperLeft(), pg.color);
	};

	static auto drawTextureFrame = [&](gl::Texture2dRef tex, const Rectf& src, const Rectf& dst)
	{
		gl::ScopedColor scpColor;
		gl::color(Color::white());
		gl::draw(tex, Area(src), dst);
		gl::ScopedBlendAdditive scpBlendAdd;
		gl::drawStrokedRect(dst);
	};

	gl::clear();

	auto videoFrameTex = gl::Texture2dRef();
	auto videoBoundsDst = Rectf();
		
	videoFrameTex = mVideoOutputFbo->getColorTexture();
	videoBoundsDst = videoFrameTex->getBounds();
	drawTextureFrame(videoFrameTex, videoFrameTex->getBounds(), videoBoundsDst);

	videoFrameTex = mVideoDecoder->getFramebuffer()->getColorTexture();
	videoBoundsDst = videoFrameTex->getBounds().getOffset(videoBoundsDst.getUpperRight());
	drawTextureFrame(videoFrameTex, videoFrameTex->getBounds(), videoBoundsDst);

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
			drawProgressBar(bar, nextBarBounds);
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
