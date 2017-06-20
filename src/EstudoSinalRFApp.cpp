#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

EstudoSinalRFApp::EstudoSinalRFApp()
{}

void EstudoSinalRFApp::initShaders()
{
	mDeviceRect = std::make_shared<DeviceRectImpl>();

	try
	{
		mVideoMonitorProg = gl::GlslProg::create(gl::GlslProg::Format()
			.vertex(CI_GLSL(440,
				in vec2 coord;
				in vec2 texCoord;
				uniform mat4 mvp;
				out vec2 iTexCoord;
				void main()
				{
					gl_Position = mvp * vec4(coord, 0, 1);
					iTexCoord = texCoord;
				}))
			.fragment(CI_GLSL(440,
				in vec2 iTexCoord;
				uniform sampler2D uTex0;
				out vec4 oFrag;
				void main()
				{
					float signal = texture(uTex0, iTexCoord).r;
					oFrag = vec4(mix(vec3(0, 0.1, 0.5), vec3(0, 0.8, 1), signal), 1.0);
				})));

	}
	catch (gl::GlslProgExc e)
	{
		std::cout << e.what() << std::endl;
	}
}

void EstudoSinalRFApp::setup()
{
	initShaders();
	
	mVideoContext = std::make_shared<PCMAdaptor::Context>();

	auto texFormat = gl::Texture2d::Format()
		.internalFormat(GL_RGBA32F)
		//.minFilter(GL_NEAREST)
		//.magFilter(GL_NEAREST)
		.wrapS(GL_CLAMP_TO_BORDER)
		.wrapT(GL_CLAMP_TO_BORDER);

	auto iSize = mVideoContext->getInputSize();
	auto oSize = mVideoContext->getOutputSize();

	mVideoEncoder = std::make_shared<PCMAdaptor::Video::Encoder2>(mVideoContext.get());
	mVideoDecoder = std::make_shared<PCMAdaptor::Video::Decoder2>(mVideoContext.get());

	mAudioBuffer = std::vector<float>(mVideoContext->getBlocksPerLine() * iSize.y * mVideoContext->getChannels());
	mVideoBuffer = std::vector<float>(mVideoContext->getOutputLength());
	
	mAudioFrame = ci::audio::Buffer(mVideoContext->getBlocksPerLine() * iSize.y, mVideoContext->getChannels());
	mVideoFrame = gl::Texture2d::create(oSize.x, oSize.y, texFormat);
	mVideoFrame->setTopDown(true);
	mVideoFrame->setLabel("Encoder output");

	try
	{
		//Context
		audio::Context* ctx = audio::Context::master();
		audio::Node::Format nodeFormat;
		nodeFormat.autoEnable();

		//Device
		audio::Device::Format deviceFormat;
		deviceFormat.framesPerBlock(64).sampleRate(44100u);
		audio::DeviceRef inputDevice = audio::Device::getDefaultInput();
		inputDevice->updateFormat(deviceFormat);
		audio::DeviceRef outputDevice = audio::Device::getDefaultOutput();
		outputDevice->updateFormat(deviceFormat);
		
		//Nodes
		mAudioInputNode = ctx->createInputDeviceNode(inputDevice, nodeFormat);
		mAudioOutputNode = ctx->createOutputDeviceNode(outputDevice, nodeFormat);
		mAudioReaderNode = ctx->makeNode<PCMAdaptor::Audio::ReaderNode>(nodeFormat.autoEnable(false));
		mAudioWriterNode = ctx->makeNode<PCMAdaptor::Audio::WriterNode>(nodeFormat.autoEnable(true));
		
		//Node chain
		mAudioInputNode >> mAudioReaderNode >> mAudioWriterNode >> mAudioOutputNode;

		//ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	mAudioEnableTimer.start();
}

void EstudoSinalRFApp::resize()
{

}

void EstudoSinalRFApp::mouseDown(MouseEvent event)
{

}

void EstudoSinalRFApp::keyDown(KeyEvent event)
{
	if (event.getCode() == KeyEvent::KEY_F1)
	{
		mAudioReaderNode->clear();
		mAudioWriterNode->clear();
	}
}

void EstudoSinalRFApp::update()
{
	audio::Context* ctx = audio::Context::master();

	if (!mAudioEnableTimer.isStopped())
	{
		if (mAudioEnableTimer.getSeconds() > 1.0f)
		{
			if (!ctx->isEnabled())
			{
				ctx->enable();
				mAudioReaderNode->enable();
				mAudioEnableTimer.stop();
			}
		}
	}
	
	if (ctx->isEnabled())
	{
		mFrameEncodeTimer.start();

		if (mAudioReaderNode->getAvailableSamples() >= mAudioInputNode->getFramesPerBlock())
		{
			mAudioReaderNode->read(mAudioFrame);

			ci::audio::dsp::interleave(
				mAudioFrame.getData(), 
				mAudioBuffer.data(), 
				mAudioFrame.getNumFrames(), 
				mAudioFrame.getNumChannels(), 
				mAudioFrame.getNumFrames());
			
			mVideoEncoder->process(mAudioBuffer, mVideoBuffer);

			mVideoFrame->update(
				mVideoBuffer.data(),
				GL_RED,
				GL_FLOAT,
				0,
				mVideoFrame->getWidth(),
				mVideoFrame->getHeight());

			//////////////////////////////////////////////////////

			mVideoDecoder->process(mVideoBuffer, mAudioBuffer);

			ci::audio::dsp::deinterleave(
				mAudioBuffer.data(),
				mAudioFrame.getData(),
				mAudioFrame.getNumFrames(),
				mAudioFrame.getNumChannels(),
				mAudioFrame.getNumFrames());

			mAudioWriterNode->write(mAudioFrame);
		}

		mFrameEncodeTimer.stop();
	}
}


void EstudoSinalRFApp::drawFrame(gl::Texture2dRef tex, const Rectf& src, const Rectf& dst)
{
	gl::ScopedColor scpColor;
	gl::color(Color::white());
	//gl::draw(tex, Area(src), dst);

	if (mVideoMonitorProg)
	{
		gl::ScopedGlslProg scpProg(mVideoMonitorProg);
		gl::ScopedVao scpVao(mDeviceRect->getVao());
		gl::ScopedTextureBind scpTex0(tex, 0);

		glm::vec2 windowSize = getWindowSize();
		glm::mat4 mvp = glm::ortho(0.0f, windowSize.x, windowSize.y, 0.0f);
		mvp = glm::translate(mvp, glm::vec3(dst.getUpperLeft(), 0.0f));
		mvp = glm::scale(mvp, glm::vec3(src.getSize(), 0));
		mvp = glm::scale(mvp, glm::vec3(src.getSize() / dst.getSize(), 0));

		mVideoMonitorProg->uniform("mvp", mvp);
		mVideoMonitorProg->uniform("uTex0", 0);

		gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	gl::ScopedBlendAdditive scpBlendAdd;
	gl::drawStrokedRect(dst);
	gl::drawString(tex->getLabel(), dst.getUpperLeft());
};


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

	gl::clear();

	auto videoFrameTex = gl::Texture2dRef();
	auto videoBoundsDst = Rectf();
		
	videoFrameTex = mVideoFrame;// mVideoOutputFbo->getColorTexture();
	videoBoundsDst = videoFrameTex->getBounds();
	drawFrame(videoFrameTex, videoFrameTex->getBounds(), videoBoundsDst);

	videoFrameTex = mVideoDecoder->getProcessedTexture();
	videoBoundsDst = videoFrameTex->getBounds().getOffset(videoBoundsDst.getUpperRight());
	drawFrame(videoFrameTex, videoFrameTex->getBounds(), videoBoundsDst);

	float iBufferAvailable	= static_cast<float>(mAudioReaderNode->getAvailableSamples()) / mAudioReaderNode->getCapacitySamples();
	float iReadProgress		= static_cast<float>(mAudioReaderNode->getReadIndex()) / mAudioReaderNode->getCapacitySamples();
	float iWriteProgress	= static_cast<float>(mAudioReaderNode->getWriteIndex()) / mAudioReaderNode->getCapacitySamples();
	float oBufferAvailable	= static_cast<float>(mAudioWriterNode->getAvailableSamples()) / mAudioWriterNode->getCapacitySamples();
	float oReadProgress		= static_cast<float>(mAudioWriterNode->getReadIndex()) / mAudioWriterNode->getCapacitySamples();
	float oWriteProgress	= static_cast<float>(mAudioWriterNode->getWriteIndex()) / mAudioWriterNode->getCapacitySamples();

	Rectf barBounds(0.0f, 0.0f, getWindowWidth() * 0.25f, 32.0f);
	Color barColor(0.25f, 0.5f, 1.0f);

	std::vector<ProgressBar> progressBars
	{
		ProgressBar{ "Input.available"		, barBounds, barColor, iBufferAvailable },
		ProgressBar{ "Input.position.read"	, barBounds, barColor, iReadProgress },
		ProgressBar{ "Input.position.write"	, barBounds, barColor, iWriteProgress },
		ProgressBar{ "Output.available"		, barBounds, barColor, oBufferAvailable },
		ProgressBar{ "Output.position.read"	, barBounds, barColor, oReadProgress },
		ProgressBar{ "Output.position.write", barBounds, barColor, oWriteProgress }
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
	settings->setFrameRate(60.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
