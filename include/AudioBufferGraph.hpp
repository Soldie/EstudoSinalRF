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

	audio::Buffer		mAudioBuffer;
	gl::Texture2dRef	mGraphTexture;
	vector<string>		mLabels;

	void initialize()
	{
		gl::Texture2d::Format texFormat;
		texFormat.internalFormat(GL_R32F);
		texFormat.dataType(GL_FLOAT);
		texFormat.minFilter(GL_NEAREST);
		texFormat.magFilter(GL_NEAREST);
		texFormat.mipmap();

		mGraphTexture = gl::Texture2d::create(
			nullptr,
			GL_RED,
			mAudioBuffer.getNumFrames(),
			mAudioBuffer.getNumChannels(),
			texFormat);
	}

public:

	AudioBufferGraph() = default;

	AudioBufferGraph(const AudioBufferGraph& copy)
	{
		mAudioBuffer = copy.mAudioBuffer;
		mGraphTexture = copy.mGraphTexture;
		mLabels = copy.mLabels;
	}

	AudioBufferGraph(audio::Buffer& buffer, vector<string>& labels)
		: mAudioBuffer(buffer)
		, mLabels(buffer.getNumChannels(), "Channel")
	{
		initialize();
		update(buffer);
		size_t numFrames = min(mLabels.size(), labels.size());
		copy(labels.begin(), labels.begin() + numFrames, mLabels.begin());
	}

	void update(audio::Buffer& other)
	{
		if (mAudioBuffer.isEmpty() || other.isEmpty()) return;

		mAudioBuffer.copy(other);

		mGraphTexture->update(
			mAudioBuffer.getData(),
			GL_RED,
			GL_FLOAT,
			0,
			mAudioBuffer.getNumFrames(),
			mAudioBuffer.getNumChannels());
	}

	void draw(Rectf& windowBounds)
	{
		AppBase& app = *App::get();

		gl::Texture2dRef& tex = mGraphTexture;

		if (mGraphTexture){

			gl::pushMatrices();
			gl::setMatricesWindow(app.getWindowSize(), false);
			gl::draw(tex, windowBounds);
			gl::popMatrices();
			gl::color(Color::white());

			try
			{
				//TODO: Usar shader de grade
				vec2 stepPer = vec2(windowBounds.getSize()) / vec2(tex->getSize());
				{
					gl::ScopedColor scpColor(Color::black());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (unsigned i = 0u; i < mAudioBuffer.getNumFrames(); i++){
						gl::drawLine(vec2(nextPos.x, windowBounds.getY1()), vec2(nextPos.x, windowBounds.getY2()));
						nextPos.x += stepPer.x;
					}
				}
				{
					gl::ScopedColor scpColor(Color::white());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (auto str = mLabels.begin(); str != mLabels.end(); str++){
						gl::drawLine(vec2(windowBounds.getX1(), nextPos.y), vec2(windowBounds.getX2(), nextPos.y));
						gl::drawString(*str, nextPos);
						nextPos.y += stepPer.y;
					}
				}
			}
			catch (...)
			{
				std::exception_ptr eptr = std::current_exception(); // capture

				try {
					if (eptr) {
						std::rethrow_exception(eptr);
					}
				}
				catch (const std::exception& e) {
					app.console() << __FUNCTION__ << ": " << e.what() << endl;
				}
			}
		}

		gl::drawStrokedRect(windowBounds);
	}

};