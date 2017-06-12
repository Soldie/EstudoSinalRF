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

namespace PCMAdaptor
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

	class VideoCodecBase
	{

	};

	class VideoEncoder
	{
	private:

		size_t bitDepth;
		size_t bitPeriod;
		size_t linePeriod;
		size_t samplesPerLine;
		size_t channels;
		ivec2 resolution;
		std::vector<uint16_t> samples; // para usar em conjunto com boost::crc
		std::vector<float> field;
		std::vector<float> frame;
		boost::crc_16_type crc16;

		std::vector<float>::iterator writeBits(std::vector<float>::iterator beg, size_t count, float lo, float hi, const void* data)
		{
			auto end = beg + count;

			auto bitShift = std::distance(beg, end); // a partir do MSB

			const uint64_t& value = *reinterpret_cast<const uint64_t*>(data);

			while (beg != end)
				*beg++ = value >> --bitShift & 0x1 ? hi : lo;

			return end;
		}

		std::vector<float>::iterator writeBits16(std::vector<float>::iterator beg, const void* data)
		{
			return writeBits(beg, 16, 0.1f ,0.9f, data);
		}

		std::vector<float>::iterator writeBits8(std::vector<float>::iterator beg, const void* data)
		{
			return writeBits(beg, 8, 0.1f, 0.9f, data);
		}

		uint16_t floatToFixed16(float value)
		{
			return static_cast<uint16_t>(32767.0f * value);
		}

	public:

		VideoEncoder()
		{
			bitDepth = 16u;
			bitPeriod = 5u;
			linePeriod = 2u;
			samplesPerLine = 3u;
			channels = 2u;
			resolution = ivec2(640, 490) / ivec2(bitPeriod,linePeriod); // 8 unidades de 16 bits

			samples.resize(6, 0u); // 3 amostras de 2ch = 6
			field.resize(resolution.x,0.0f);
			frame.resize(resolution.x*resolution.y);
		}

		size_t getBitPeriod(){return bitPeriod;}

		size_t getLinePeriod(){return linePeriod;}

		size_t getSamplesPerField(){ return samplesPerLine; }

		size_t getChannels(){ return channels; }

		ivec2 getResolution(){ return resolution; }
		
		bool encode(const ci::audio::Buffer& audioFrame, gl::Texture2dRef videoFrame)
		{
			const auto* ch0 = audioFrame.getChannel(0u);
			const auto* ch1 = audioFrame.getChannel(1u);

			auto frameIter = frame.begin();

			while (frameIter != frame.end())
			{
				// Converte amostras
				auto sampleIter = samples.begin();
				for (unsigned i = 0u; i < samplesPerLine; i++)
				{
					*sampleIter++ = floatToFixed16(*ch0++);
					*sampleIter++ = floatToFixed16(*ch1++);
				}

				crc16.reset();
				crc16.process_block(&*samples.begin(), &*(samples.end()-1));

				// Escreve campo
				sampleIter = samples.begin();
				auto fieldIter = field.begin();

				//uint16_t clockword = 0x5556;
				//fieldIter = writeBits16(fieldIter, &clockword);

				uint8_t firstBits = 0x0A;
				fieldIter = writeBits8(fieldIter, &firstBits);

				for (unsigned i = 0u; i < samplesPerLine; i++)
				{
					fieldIter = writeBits16(fieldIter, &*sampleIter++);
					fieldIter = writeBits16(fieldIter, &*sampleIter++);
				}

				uint16_t checksum = crc16.checksum();
				fieldIter = writeBits16(fieldIter, &checksum);

				uint8_t lastBits = 0x78;
				fieldIter = writeBits8(fieldIter, &lastBits);

				// Preenche quadro;
				frameIter = std::copy(field.begin(), field.end(), frameIter);
			}

			videoFrame->update(frame.data(), GL_RED, GL_FLOAT, 0, resolution.x, resolution.y);

			return true;
		}
	};

	class SchmittTrigger
	{
	public:

		struct Level{ float lo, hi; };

	private:

		Level signal;
		Level threshold;
		int state;

	public:

		SchmittTrigger()
			: signal({ -1.0f, 1.0f })
			, threshold({ -0.5, 0.5f })
		{}

		SchmittTrigger(Level signal, Level threshold)
			: signal(signal)
			, threshold(threshold)
		{}

		void process(std::vector<float>* buffer)
		{
			if (buffer)
			{
				float nextValue;
				auto valueIter = buffer->begin();
				while (valueIter != buffer->end())
				{
					nextValue = *valueIter;

					if (nextValue > threshold.hi && state != 1)
						state = 1;
					else if (nextValue < threshold.lo && state != -1)
						state = -1;

					if (state == 1)
						nextValue = signal.hi;
					else if (state == -1)
						nextValue = signal.lo;
					else
						nextValue = 0.0f;

					*valueIter++ = nextValue;
				}
			}
		}
	};

	class ClockParser
	{
	private:

		bool learning;
		bool transited;
		bool locked;
		std::array<bool, 2> levels;
		std::vector<size_t> periods;
		size_t periodAccum;
		size_t periodCurrent;
		size_t periodLocked;

		size_t calcLockedPeriod()
		{
			size_t value = 0u;
			while (!periods.empty())
			{
				value += periods.back();
				periods.pop_back();
			}
			value /= periods.capacity();
			return value;
		}

		void tryLock()
		{
			periods.push_back(periodAccum);
			if (periods.size() >= periods.capacity())
			{
				periodLocked = calcLockedPeriod();
				locked = true;
			}
		}

		void debugPrint()
		{
			std::cout
				<< "locked="<<locked << ", "
				<< "transited=" << transited << ", "
				<< "periodAccum="<< periodAccum << ", "
				<< "periodCurrent="<< periodCurrent << ", "
				<< "periodLocked="<< periodLocked << ", "
				<< "level=" << levels[0] << std::endl;
		}

	public:

		ClockParser(size_t samples = 2u) 
			: periods(samples, 0u)
		{
			reset();
		}

		void reset()
		{
			learning = transited = locked = false;
			levels[0] = levels[1] = false;
			periodAccum = periodCurrent = periodLocked = 0u;
			periods.clear();
		}

		bool hasTransited(){return transited;}

		bool isLearning(){return learning;}

		bool isLocked(){return locked;}

		size_t getCurrentPeriod(){ return periodCurrent; }

		size_t getLockedPeriod(){ return periodLocked; }

		void process(std::vector<float>* buffer)
		{
			if (buffer)
			{
				float nextValue;
				auto valueIter = buffer->begin();
				while (valueIter != buffer->end())
				{
					nextValue = *valueIter;

					levels[1] = levels[0];
					levels[0] = static_cast<bool>(nextValue);

					transited = levels[0] != levels[1];

					if (transited)
					{
						if (levels[0] && !learning)
						{
							learning = true;
						}
						else if (!levels[0] && learning)
						{
							if (!locked)
							{
								tryLock();
							}

							periodCurrent = periodAccum;
							periodAccum = 0u;
							learning = false;
						}
					}

					if (learning)
					{
						periodAccum++;
					}

					debugPrint();

					valueIter++;
				}
			}
		}
	};

	class ClockGenerator
	{
	private:

		float mInterval;
		float mTime;

	public:

		ClockGenerator() 
			: mInterval(0.0f) 
			, mTime(0.0f)
		{}

		ClockGenerator(float interval) 
			: mInterval(interval)
			, mTime(0.0f)
		{}

		float getInterval()
		{
			return mInterval;
		}

		float getPeriod()
		{
			return 1.0f / mInterval;
		}

		void reset()
		{
			mTime = 0.0f;
		}

		bool triggerEnabled = false;

		void process(std::vector<float>* buffer)
		{
			if (buffer)
			{
				if (mInterval == 0.0f)
				{
					std::fill(buffer->begin(), buffer->end(), 0.0f);
				}
				else
				{
					static auto integralPart = [](float value)
					{
						float intPart(0.0f);
						modf(value, &intPart);
						return intPart;
					};

					auto valueIter = buffer->begin();
					float nextValue;
					while (valueIter != buffer->end())
					{
						float period = 1.0f / mInterval;

						nextValue = integralPart(fmod(2.0f*mTime,2.0f));
						if(triggerEnabled)
						{
							nextValue = nextValue == integralPart(std::max(fmod(2.0f*(mTime-period),2.0f), 0.0f)) ? 0.0f : 1.0f;
						}
						*valueIter++ = nextValue;
						mTime += period;
					}
				}
			}
		}

	};

	class VideoDecoder
	{
	private:

	
		//float	lastBit, nextBit;
		//size_t	bitPeriod = 0u;
		//size_t	clockTransitions = 0u;
		//bool	clockLocked = false;

		ClockParser clockParser;

	public:

		//TODO: Buffer to texture  (View)
		//TODO: ClockParser // detecta periodo de clock
		//TODO: ClockSource // gerador de clock
		//TODO: TriggerSource // gerador de gatilho
		
		VideoDecoder()
		{
			//lastBit = nextBit = 0.0f;
			//bitPeriod = clockTransitions = 0u;
			//clockLocked = false;
		}

		bool decode(const gl::Texture2dRef output, const ci::audio::Buffer& frame)
		{
			//flag treinar clock
			//bitPeriod;

			//se bitPeriod

			//lastBit = nextBit = 0.0f;

			ci::Surface32f bitmap(output->createSource());

			ci::Surface32f::ConstIter iter = bitmap.getIter();

			while (iter.line()) {
				while (iter.pixel()) {

					//clockParser.mark((bool)iter.r());
				}
			}
			
			return true;
		}
	};

	class AudioNodeBase
	{
	protected:

		size_t mSamplesPerSecond;
		size_t mCapacitySeconds;
		std::vector<ci::audio::dsp::RingBuffer> mRingBuffer;
		ci::audio::Buffer mCopyBuffer;

		AudioNodeBase() = default;

	public:

		size_t getReadIndex()
		{
			return mRingBuffer.empty() ? 0u : mRingBuffer.at(0).getReadIndex();
		}

		size_t getWriteIndex()
		{
			return mRingBuffer.empty() ? 0u : mRingBuffer.at(0).getWriteIndex();
		}

		size_t getAvailableSamples()
		{
			return mRingBuffer.empty() ? 0u : mRingBuffer.at(0).getAvailableRead();
		}

		float getAvailableSeconds()
		{
			return mRingBuffer.empty() ? 0.0f : static_cast<float>(getAvailableSamples()) / mSamplesPerSecond;
		}

		size_t getCapacitySamples()
		{
			return mRingBuffer.empty() ? 0u : mRingBuffer.at(0).getSize();
		}

		size_t getCapacitySeconds()
		{
			return mRingBuffer.empty() ? 0u : getCapacitySamples() / mSamplesPerSecond;
		}
	};

	class AudioReaderNode 
		: public AudioNodeBase
		, public ci::audio::Node
	{
	private:

		void initialize() override
		{
			mCapacitySeconds = 1u;
			mSamplesPerSecond = 44100u;

			mRingBuffer.resize(getNumChannels());
			for (auto rb = mRingBuffer.begin(); rb != mRingBuffer.end(); rb++)
			{
				(*rb).resize(mSamplesPerSecond * mCapacitySeconds);
			}
		}

		void process(ci::audio::Buffer* buffer) override
		{
			size_t length = std::min(buffer->getNumFrames(), getFramesPerBlock());
			size_t channels = std::min(getNumChannels(), buffer->getNumChannels());
			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).write(buffer->getChannel(ch), length);
			}

			buffer->zero();
		}

	public:

		AudioReaderNode(const audio::Node::Format& fmt) : audio::Node(fmt)
		{}

		void read(ci::audio::Buffer& other)
		{
			size_t length = std::min(getCapacitySamples(), other.getNumFrames());
			size_t channels = std::min(getNumChannels(), other.getNumChannels());

			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).read(other.getChannel(ch), length);
			}
		}

	};

	class AudioWriterNode
		: public AudioNodeBase
		, public ci::audio::Node
	{
	private:

		void initialize() override
		{
			mCapacitySeconds = 1u;
			mSamplesPerSecond = 44100u;

			mRingBuffer.resize(getNumChannels());
			for (auto rb = mRingBuffer.begin(); rb != mRingBuffer.end(); rb++)
			{
				(*rb).resize(mSamplesPerSecond * mCapacitySeconds);
			}
		}

		void process(ci::audio::Buffer* buffer) override
		{
			buffer->zero();

			size_t length = std::min(buffer->getNumFrames(), getFramesPerBlock());
			size_t channels = std::min(getNumChannels(), buffer->getNumChannels());
			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).read(buffer->getChannel(ch), length);
			}
		}

	public:

		AudioWriterNode(const audio::Node::Format& fmt) : audio::Node(fmt)
		{}

		void write(const ci::audio::Buffer& other)
		{
			size_t length = std::min(getCapacitySamples(), other.getNumFrames());
			size_t channels = std::min(getNumChannels(), other.getNumChannels());

			for (size_t ch = 0u; ch < channels; ch++)
			{
				mRingBuffer.at(ch).write(other.getChannel(ch), length);
			}
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

	audio::InputDeviceNodeRef						mInputDeviceNode;
	std::shared_ptr<PCMAdaptor::AudioReaderNode>	mAudioReaderNode;
	std::shared_ptr<PCMAdaptor::AudioWriterNode>	mAudioWriterNode;
	ci::audio::Buffer								mAudioFrame;
	std::shared_ptr<PCMAdaptor::VideoEncoder>		mVideoEncoder;
	std::shared_ptr<PCMAdaptor::VideoDecoder>		mVideoDecoder;
	gl::Texture2dRef								mVideoFrame;

	ci::Timer										mFrameEncodeTimer;

public:

	DeviceRect* getDeviceRect(){ return mDeviceRect.get(); }

};
