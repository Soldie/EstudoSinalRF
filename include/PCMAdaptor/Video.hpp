#pragma once
#include <cinder/gl/Texture.h>
#include <cinder/audio/Buffer.h>
#include <boost/crc.hpp>
#include <PCMAdaptor/DSP.hpp>

using namespace ci;
using namespace glm;

namespace PCMAdaptor
{
	namespace Video
	{
		//class Frame
		//class AudioFrame
		//class VideoFrame
		//class Context
		//TODO: BufferToTexture  (View)
		//bool process(const Frame& input, Frame& output) = 0;

		class Encoder
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

			static std::vector<float>::iterator writeBits(std::vector<float>::iterator beg, size_t count, float lo, float hi, const void* data)
			{
				auto end = beg + count;

				auto bitShift = std::distance(beg, end); // a partir do MSB

				const uint64_t& value = *reinterpret_cast<const uint64_t*>(data);

				while (beg != end)
					*beg++ = value >> --bitShift & 0x1 ? hi : lo;

				return end;
			}

			static std::vector<float>::iterator writeBits16(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 16, 0, 1, data);
			}

			static std::vector<float>::iterator writeBits8(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 8, 0, 1, data);
			}

			static uint16_t floatToFixed16(float value)
			{
				return static_cast<uint16_t>(32767.0f * value);
			}

		public:

			Encoder()
			{
				bitDepth = 16u;
				bitPeriod = 5u;
				linePeriod = 2u;
				samplesPerLine = 3u;
				channels = 2u;
				resolution = ivec2(640, 490) / ivec2(bitPeriod, linePeriod); // 8 unidades de 16 bits

				samples.resize(6, 0u); // 3 amostras de 2ch = 6
				field.resize(resolution.x, 0.0f);
				frame.resize(resolution.x*resolution.y);
			}

			size_t getBitPeriod(){ return bitPeriod; }

			size_t getLinePeriod(){ return linePeriod; }

			size_t getSamplesPerField(){ return samplesPerLine; }

			size_t getChannels(){ return channels; }

			ivec2 getResolution(){ return resolution; }

			bool process(const ci::audio::Buffer& audioFrame, gl::Texture2dRef videoFrame)
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
					crc16.process_block(&*samples.begin(), &*(samples.end() - 1));

					// Escreve campo
					sampleIter = samples.begin();
					auto fieldIter = field.begin();

					//static const uint16_t clockword = 0x5556;
					//fieldIter = writeBits16(fieldIter, &clockword);

					static const uint8_t firstBits = 0x0A;
					fieldIter = writeBits8(fieldIter, &firstBits);

					for (unsigned i = 0u; i < samplesPerLine; i++)
					{
						fieldIter = writeBits16(fieldIter, &*sampleIter++);
						fieldIter = writeBits16(fieldIter, &*sampleIter++);
					}

					uint16_t checksum = crc16.checksum();
					fieldIter = writeBits16(fieldIter, &checksum);

					static const uint8_t lastBits = 0x78;
					fieldIter = writeBits8(fieldIter, &lastBits);

					// Preenche quadro;
					frameIter = std::copy(field.begin(), field.end(), frameIter);
				}

				videoFrame->update(frame.data(), GL_RED, GL_FLOAT, 0, resolution.x, resolution.y);

				return true;
			}
		};

		class Decoder
		{
		private:

		public:

			Decoder()
			{}

			bool process(const gl::Texture2dRef input, ci::audio::Buffer& output)
			{
			
			}
		};
	}
}
