#pragma once
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Unicode.h"
#include "cinder/audio/dsp/Fft.h"
#include "cinder/audio/dsp/RingBuffer.h"

#include "RF.hpp"
#include "DeviceRect.hpp"
#include "AudioBufferGraph.hpp"

#include "PCMAdaptor.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class EstudoSinalRFApp : public App {
public:

	EstudoSinalRFApp();
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void update() override;
	void draw() override;
	void resize() override;

private:

	void initShaders();

	std::shared_ptr<DeviceRect>						mDeviceRect;

	audio::InputNodeRef								mAudioInputNode;
	audio::OutputNodeRef							mAudioOutputNode;
	std::shared_ptr<PCMAdaptor::Audio::ReaderNode>	mAudioReaderNode;
	std::shared_ptr<PCMAdaptor::Audio::WriterNode>	mAudioWriterNode;
	ci::audio::Buffer								mAudioFrame;
	std::vector<float>								mAudioBuffer;
	
	std::shared_ptr<PCMAdaptor::Context>			mVideoContext;
	std::shared_ptr<PCMAdaptor::Video::Encoder2>	mVideoEncoder;
	std::shared_ptr<PCMAdaptor::Video::Decoder2>	mVideoDecoder;
	gl::Texture2dRef								mVideoFrame;
	std::vector<float>								mVideoBuffer;

	gl::GlslProgRef									mVideoMonitorProg;

	ci::Timer										mFrameEncodeTimer;
	ci::Timer										mAudioEnableTimer;

public:

	void drawFrame(gl::Texture2dRef tex, const Rectf& src, const Rectf& dst);
	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
