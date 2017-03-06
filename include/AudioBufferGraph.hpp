#pragma once
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"

using namespace ci;
using namespace ci::app;
using namespace std;

//TODO: implementar classe padrão factory  (AudioBufferProgramBuilder) para gerar programa com parametros personalizados.

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

	AudioBufferGraph(const audio::Buffer& buffer, const vector<string>& labels);

	void setGraph(const audio::Buffer& other);

	void draw(const audio::Buffer& buffer, const Rectf& bounds){ 
		setGraph(buffer);
		draw(bounds);
	}

	void draw(const Rectf& bounds);

};
