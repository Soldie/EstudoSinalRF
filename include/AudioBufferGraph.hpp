#pragma once
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class AudioBufferGraph
{
private:

	audio::Buffer				mAudioBuffer;
	vector<string>				mLabels;
	vector<gl::VaoRef>			mVao;
	vector<gl::VboRef>			mBuf;
	vector<gl::GlslProgRef>		mPrg;
	vector<gl::Texture2dRef>	mTex;
	gl::Texture2dRef			mGraphTexture;
	Font						mFont;

private:

	void initTexture();

	void initShader();

public:

	AudioBufferGraph() = default;

	AudioBufferGraph(audio::Buffer& buffer, vector<string>& labels);

	void setGraph(audio::Buffer& other);

	void draw(Rectf& windowBounds);

};