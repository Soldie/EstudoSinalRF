#pragma once

//#define GLM_FORCE_SSE2 // tentar ativar suporte SIMD com glm.

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Unicode.h"
#include "cinder/audio/dsp/Fft.h"

#include "RF.hpp"
#include "DeviceRect.hpp"
#include "AudioBufferGraph.hpp"

#include <boost/crc.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;



typedef shared_ptr<class PCMVideoEncoder> PCMVideoEncoderRef;

class PCMVideoEncoder
{
protected:

	size_t bitsPerSample;
	size_t samplesPerBlock;
	size_t periodPerBit;

	vector<float>		bitmap;
	vector<uint16_t>	block;
	vector<uint16_t>	stream;

	boost::crc_16_type	crc;

	gl::Texture2dRef			mFrame;
	gl::PboRef					mPbo;
	signals::ScopedConnection	mSignalDraw;

protected:

	
	void writeBitmap(vector<float>::iterator&, vector<float>::iterator&, uint16_t);
	void updateStream(size_t samplesPerBlock, size_t blocksPerFrame, vector<uint16_t>& values);
	void updateFrame(gl::Texture2dRef& tex);

protected:

	// FIXME: OpenCL está interferindo no shading do OpenGL.
	void setupCL();
	void setup();
	void draw();

public:

	PCMVideoEncoder() = default;
	static PCMVideoEncoderRef create();

};



class EstudoSinalRFApp : public App {
public:

	EstudoSinalRFApp();
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

private:

	audio::MonitorNodeRef			mMonitorNode;
	audio::InputDeviceNodeRef		mInputDeviceNode;
	audio::Buffer					mHSyncBuffer;
	RF::HSyncDetectorRef			mHSyncDetector;
	AudioBufferGraph				mAudioBufferGraph;

	audio::dsp::Fft mFft;
	audio::BufferSpectral mBufferSpectral;
	audio::GenSineNodeRef mGenSineNode;
	audio::GainNodeRef mGainNode;

	void gerarSinalTeste(float* data, size_t length);
	void gerarSinalTeste(audio::Buffer& buffer);

	PCMVideoEncoderRef mPCMVideoEncoder;

private:

	unique_ptr<DeviceRect> mDeviceRect;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
