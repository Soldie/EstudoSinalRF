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

		size_t bitDepth;
		size_t bitPeriod;
		size_t fieldPeriod;
		size_t fieldIndex;
		size_t samplesPerField;
		ivec2 resolution;
		std::vector<uint16_t> samples; // para usar em conjunto com boost::crc
		std::vector<float> field;
		std::vector<float> frame;
		boost::crc_16_type crc16;

		std::vector<float>::iterator writeBits(std::vector<float>::iterator beg, size_t count, const void* data)
		{
			auto end = beg + count;

			auto bitShift = std::distance(beg, end); // a partir do MSB

			const uint64_t& value = *reinterpret_cast<const uint64_t*>(data);

			while (beg != end)
				*beg++ = static_cast<float>(value >> --bitShift & 0x1);

			return end;
		}

		std::vector<float>::iterator writeBits16(std::vector<float>::iterator beg, const void* data)
		{
			return writeBits(beg, 16, data);
		}

		uint16_t floatToUInt16(float value)
		{
			return static_cast<uint16_t>(32767.0f * value);
		}

	public:

		FrameEncoder()
		{
			bitDepth = 16u;
			bitPeriod = 5u;
			fieldPeriod = 2u;
			samplesPerField = 3u;
			resolution = ivec2(640, 490) / ivec2(bitPeriod,fieldPeriod); // 8 unidades de 16 bits

			samples.resize(6, 0u); // 3 amostras de 2ch = 6
			field.resize(resolution.x,0.0f);
			frame.resize(resolution.x*resolution.y);

			/*
			auto frameIter = frame.begin();
			while (frameIter != frame.end())
			{
				frameIter = std::copy(field.begin(), field.end(), frameIter);
			}
			*/

			fieldIndex = 0u;
		}

		ivec2 getResolution(){return resolution;}

		size_t getBitPeriod(){return bitPeriod;}

		size_t getFieldPeriod(){return fieldPeriod;}

		void update(const ci::audio::Buffer& line)
		{
			const auto* ch0 = line.getChannel(0u);
			const auto* ch1 = line.getChannel(1u);

			auto frameIter = frame.begin();

			while (frameIter != frame.end())
			{
				// Converte amostras
				auto sampleIter = samples.begin();
				for (unsigned i = 0u; i < samplesPerField; i++)
				{
					*sampleIter++ = floatToUInt16(*ch0++);
					*sampleIter++ = floatToUInt16(*ch1++);
				}

				crc16.reset();
				crc16.process_block(&*samples.begin(), &*(samples.end()-1));

				// Escreve campo
				sampleIter = samples.begin();
				auto fieldIter = field.begin();

				uint16_t clockword = 0x5556;
				fieldIter = writeBits16(fieldIter, &clockword);

				for (unsigned i = 0u; i < samplesPerField; i++)
				{
					fieldIter = writeBits16(fieldIter, &*sampleIter++);
					fieldIter = writeBits16(fieldIter, &*sampleIter++);
				}

				uint16_t checksum = crc16.checksum();
				fieldIter = writeBits16(fieldIter, &checksum);

				// Preenche quadro;
				frameIter = std::copy(field.begin(), field.end(), frameIter);
			}
		}

		void render(gl::Texture2dRef tex)
		{
			tex->update(frame.data(), GL_RED, GL_FLOAT, 0, resolution.x, resolution.y);

			//tex->update(field.data(), GL_RED, GL_FLOAT, 0, resolution.x, 1u, glm::ivec2(0, fieldIndex));
			//fieldIndex++;
			//fieldIndex %= resolution.y;
		}
	};

	class FrameDecoder
	{
		void process(const vector<float>& buffer)
		{

		}
	};

	class AudioInputNode : public ci::audio::Node
	{
	private:

		size_t mSamplesPerSecond;
		size_t mCapacitySeconds;
		std::vector<ci::audio::dsp::RingBuffer> mRingBuffer;
		ci::audio::Buffer mCopyBuffer;

		void initialize() override
		{
			mCapacitySeconds = 1u;
			mSamplesPerSecond = 44100u;

			mRingBuffer.resize(getNumChannels());
			for (auto rb = mRingBuffer.begin(); rb != mRingBuffer.end(); rb++)
			{
				(*rb).resize(mSamplesPerSecond * mCapacitySeconds);
			}

			// (3 amostras * 2ch) * 245 linhas * 30 campos = 44100hz

			mCopyBuffer = ci::audio::Buffer(3u * 245u, getNumChannels());
		}

		void process(ci::audio::Buffer* buffer) override
		{
			size_t length = std::min(buffer->getNumFrames(), getFramesPerBlock());
			size_t channels = std::min(getNumChannels(), buffer->getNumChannels());
			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).write(buffer->getChannel(ch), length);
			}
		}

	public:

		AudioInputNode(const audio::Node::Format& fmt) : audio::Node(fmt)
		{}

		const ci::audio::Buffer& readLine()
		{
			size_t length = mCopyBuffer.getNumFrames();
			size_t channels = std::min(getNumChannels(), mRingBuffer.size());
			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).read(mCopyBuffer.getChannel(ch), length);
			}
			return mCopyBuffer;
		}

		size_t getReadIndex()
		{
			return mRingBuffer.at(0).getReadIndex();
		}

		size_t getWriteIndex()
		{
			return mRingBuffer.at(0).getWriteIndex();
		}

		size_t getAvailableSamples()
		{
			return mRingBuffer.at(0).getAvailableRead();
		}

		size_t getCapacity()
		{
			return mRingBuffer.at(0).getSize();
		}

		size_t getCapacitySeconds()
		{
			return getCapacity() / mSamplesPerSecond;
		}

		float getAvailableSeconds()
		{
			return static_cast<float>(getAvailableSamples()) / mSamplesPerSecond;
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
	gl::Texture2dRef							mVideoFrame;

	ci::Timer									mFrameEncodeTimer;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
