#pragma once
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Unicode.h"
#include "RF.hpp"
#include "DeviceRect.hpp"
#include "AudioBufferGraph.hpp"
#include "cinder\audio\dsp\Fft.h"

using namespace ci;
using namespace ci::app;
using namespace std;

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
	void gerarQuadroVideoPCM(gl::Texture2dRef& tex);

	gl::Texture2dRef mDataImageFrame;

private:

	unique_ptr<DeviceRect> mDeviceRect;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
