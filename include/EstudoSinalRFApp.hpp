#pragma once
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Unicode.h"
#include "RF.hpp"
#include "DeviceRect.hpp"
#include "AudioBufferGraph.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class EstudoSinalRFApp : public App {
public:

	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

private:

	audio::MonitorNodeRef			mMonitorNode;
	audio::InputDeviceNodeRef		mInputDeviceNode;
	vector<audio::dsp::RingBuffer>	mHSyncRingBuffer;
	audio::Buffer					mHSyncBuffer;
	RF::HSyncDetector				mHSyncDetector;
	AudioBufferGraph				mAudioBufferGraph;

	void gerarSinalTeste(float* data, size_t length);
	void gerarSinalTeste(audio::Buffer& buffer);

private:

	unique_ptr<DeviceRect> mDeviceRect;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
