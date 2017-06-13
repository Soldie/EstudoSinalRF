#pragma once
#include <cinder/audio/audio.h>

using namespace ci;

namespace PCMAdaptor
{
	namespace Audio
	{
		class NodeBase
		{
		protected:

			size_t mSamplesPerSecond;
			size_t mCapacitySeconds;
			std::vector<ci::audio::dsp::RingBuffer> mRingBuffer;
			ci::audio::Buffer mCopyBuffer;

			NodeBase() = default;

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

		class ReaderNode
			: public NodeBase
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

			ReaderNode(const audio::Node::Format& fmt) : audio::Node(fmt)
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

		class WriterNode
			: public NodeBase
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

			WriterNode(const audio::Node::Format& fmt) : audio::Node(fmt)
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
}
