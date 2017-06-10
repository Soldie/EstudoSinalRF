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

#include <boost/crc.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;

namespace PCMVideo
{
	class Format
	{
	private:

		size_t mSamplesPerLine;
		size_t mLinesPerField;
		size_t mFieldsPerSeconds;
		size_t mSamplesPerFrame;
		size_t mSamplesPerSecond;
		size_t mBufferSecondsCapacity;

	public:

		Format& samplesPerLine(size_t value){ mSamplesPerLine = value; return *this; }
		Format& linesPerField(size_t value){ mLinesPerField = value; return *this; }
		Format& fieldsPerSeconds(size_t value){ mFieldsPerSeconds = value; return *this; }
		Format& samplesPerFrame(size_t value){ mSamplesPerFrame = value; return *this; }
		Format& samplesPerSecond(size_t value){ mSamplesPerSecond = value; return *this; }
		Format& bufferSecondsCapacity(size_t value){ mBufferSecondsCapacity = value; return *this; }

		size_t getSamplesPerLine(size_t value){ return mSamplesPerLine; }
		size_t getLinesPerField(size_t value){ return mLinesPerField; }
		size_t getFieldsPerSeconds(size_t value){ return mFieldsPerSeconds; }
		size_t getSamplesPerFrame(size_t value){ return mSamplesPerFrame; }
		size_t getSamplesPerSecond(size_t value){ return mSamplesPerSecond; }
		size_t getBufferSecondsCapacity(size_t value){ return mBufferSecondsCapacity; }

	};

	class FrameEncoder
	{
	private:

		int bitDepth;
		ivec2 oversampleFactor;
		ivec2 frameSize;
		std::vector<float> frameBuffer;
		std::vector<float> frameField;

		//boost::crc<16> crc;

		std::vector<float>::iterator pushValue(std::vector<float>::iterator beg, std::vector<float>::iterator end, uint64_t value)
		{
			size_t bitShift = std::distance(beg, end);

			while (beg != end)
				*beg++ = static_cast<float>((value >> --bitShift) & 0x1);

			return beg;
		}

		std::vector<float>::iterator pushValueU(std::vector<float>::iterator beg, uint16_t value)
		{
			return pushValue(beg, beg + 16, static_cast<uint64_t>(value));
		}

		std::vector<float>::iterator pushValueF(std::vector<float>::iterator beg, float value)
		{
			return pushValue(beg, beg + 16, static_cast<uint64_t>(32767.0f * value));
		}

	public:

		FrameEncoder()
		{
			bitDepth = 16;
			oversampleFactor = ivec2(5, 2);
			frameSize = ivec2(640, 490) / oversampleFactor; // 8 unidades de 16 bits
			frameBuffer.resize(frameSize.x*frameSize.y);
			frameField.resize(frameSize.x);
		}

		ivec2 getFrameSize()
		{
			return frameSize;
		}

		ivec2 getOversampleFactor()
		{
			return oversampleFactor;
		}

		void update(const ci::audio::Buffer& buffer)
		{
			auto frameCol = frameBuffer.begin();
			auto frameEnd = frameBuffer.end();

			const auto* inputCol = buffer.getChannel(0u);

			//TODO: otimizar processo, principalmente em relação ao clock)
			//TODO: acelerar escrita das amostras

			while (frameCol != frameEnd)
			{
				frameCol = pushValueU(frameCol, 0x5556);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueF(frameCol, *inputCol++);
				frameCol = pushValueU(frameCol, 0x6AAA); 
			}
		}

		void render(gl::Texture2dRef tex)
		{
			tex->update(frameBuffer.data(), GL_RED, GL_FLOAT, 0, frameSize.x, frameSize.y);
		}
	};

	class FrameDecoder
	{
		void process(const vector<float>& buffer)
		{

		}
	};

	class AudioNodeBase
	{
	protected:

		// 3 * 490 * 30 = 44100Hz
		// 3 * 245 * 60 = 44100Hz
		// 3 * 480 * 30 = 43200Hz
		// 3 * 240 * 60 = 43200Hz
		//
		// 3 amostras de 1Ch, periodo de bit maior
		// 3 amostras de 2Ch, periodo de bit menor

		AudioNodeBase() = default;

		size_t samplesPerLine;
		size_t linesPerField;
		size_t fieldsPerSeconds;
		size_t samplesPerFrame;
		size_t samplesPerSecond;
		size_t bufferSecondsCapacity;
		ci::audio::dsp::RingBuffer mRingBuffer;
		ci::audio::Buffer mCopyBuffer;

		void initialize()
		{
			samplesPerLine = 3u;
			linesPerField = 245u;
			fieldsPerSeconds = 60u;
			samplesPerFrame = samplesPerLine * linesPerField;
			samplesPerSecond = samplesPerFrame * fieldsPerSeconds;
			bufferSecondsCapacity = 10u;

			mRingBuffer.resize(samplesPerSecond * bufferSecondsCapacity);
			mCopyBuffer = ci::audio::Buffer(samplesPerSecond);
		}
	};

	class AudioInputNode
		: public AudioNodeBase
		, public ci::audio::Node
	{
	private:

		bool frameAvailable = false;
		ci::signals::Signal<void()> mFrameAvailableSignal;

		void initialize() override
		{
			AudioNodeBase::initialize();
		}

		void process(ci::audio::Buffer* buffer) override
		{
			if (mRingBuffer.write(buffer->getChannel(0), std::min(buffer->getNumFrames(), this->getFramesPerBlock())))
			{
				if (mRingBuffer.getAvailableRead() >= samplesPerFrame && !frameAvailable)
				{
					mFrameAvailableSignal.emit();
					frameAvailable = true;
				}
				else if (frameAvailable)
				{
					frameAvailable = false;
				}
			}
		}

	public:

		AudioInputNode(const audio::Node::Format& fmt) : audio::Node(fmt)
		{}

		ci::audio::dsp::RingBuffer& getRingBuffer()
		{
			return mRingBuffer;
		}

		const ci::audio::Buffer& getBuffer()
		{
			mRingBuffer.read(mCopyBuffer.getChannel(0u), mCopyBuffer.getNumFrames());
			return mCopyBuffer;
		}

		size_t getReadIndex()
		{
			return mRingBuffer.getReadIndex();
		}

		size_t getWriteIndex()
		{
			return mRingBuffer.getWriteIndex();
		}

		size_t getAvailableSamples()
		{
			return mRingBuffer.getAvailableRead();
		}

		size_t getAvailableFrames()
		{
			return mRingBuffer.getAvailableRead() / samplesPerFrame;
		}

		float getAvailableSeconds()
		{
			return static_cast<float>(mRingBuffer.getAvailableRead()) / samplesPerSecond;
		}

		size_t getFrameSize()
		{
			return samplesPerFrame;
		}

		ci::signals::ScopedConnection attachAvailableFramesHandle(std::function<void()>& fn)
		{
			return mFrameAvailableSignal.connect(fn);
		}
	};

}

class EstudoSinalRFApp : public App {
public:

	EstudoSinalRFApp();
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

private:

	unique_ptr<DeviceRect> mDeviceRect;

	audio::InputDeviceNodeRef					mInputDeviceNode;
	std::shared_ptr<PCMVideo::AudioInputNode>	mAudioInputNode;
	std::shared_ptr<PCMVideo::FrameEncoder>		mFrameEncoder;
	std::shared_ptr<PCMVideo::FrameDecoder>		mFrameDecoder;
	std::vector<float>							mAudioFrame;
	gl::Texture2dRef							mVideoFrame;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
