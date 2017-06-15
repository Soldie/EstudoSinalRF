#pragma once
#include <cinder/gl/Texture.h>
#include <cinder/audio/Buffer.h>
#include <boost/crc.hpp>
#include <PCMAdaptor/DSP.hpp>
#include "Context.hpp"

using namespace ci;
using namespace glm;

namespace PCMAdaptor
{
	static int16_t floatToFixedS16(float value)
	{
		return static_cast<int16_t>(32767.0f * value);
	}

	static float fixedS16toFloat(int16_t value)
	{
		return static_cast<float>(value) / 32767.0f;
	}

	namespace Video
	{
		//class Frame
		//class AudioFrame
		//class VideoFrame
		//class Context
		//TODO: BufferToTexture  (View)
		//bool process(const Frame& input, Frame& output) = 0;

		//class AudioVideoAdapter
		//class VideoAudioAdapter

		/*
		class Frame
		{
		virtual void read(std::vector<float>* buffer) = 0;
		virtual void write(std::vector<float>* buffer) = 0;
		};
		*/

		class Encoder
		{
		private:

			///size_t bitDepth;
			//size_t bitPeriod;
			//size_t linePeriod;
			//size_t samplesPerLine;
			//size_t channels;
			//ivec2 resolution;

			Context* context;

			std::vector<int16_t> samples; // para usar em conjunto com boost::crc
			std::vector<float> field;
			std::vector<float> frame;
			boost::crc_16_type crc16;

			static std::vector<float>::iterator writeBits(std::vector<float>::iterator beg, size_t count, float lo, float hi, const void* data)
			{
				auto end = beg + count;

				//auto bitShift = std::distance(beg, end); // a partir do MSB

				const uint64_t& value = *reinterpret_cast<const uint64_t*>(data);

				while (beg != end)
				{
					int nextBit = std::distance(beg, end) - 1;
					*beg++ = ((value >> nextBit) & 0x1) ? hi : lo;
				}

				return end;
			}

			static std::vector<float>::iterator writeBits16(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 16, 0.0f, 1.0f, data);
			}

			static std::vector<float>::iterator writeBits8(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 8, 0.0f, 1.0f, data);
			}

		public:

			Encoder(Context* context)
				: context(context)
			{
				//bitDepth = 16u;
				///bitPeriod = 5u;
				//linePeriod = 2u;
				//samplesPerLine = 3u;
				//channels = 2u;
				//resolution = ivec2(640, 490) / ivec2(bitPeriod, linePeriod); // 8 unidades de 16 bits

				auto inputSize = context->getInputSize();
				auto inputLength = context->getInputLength();
				auto sampleCount = context->getSamplesPerLine();

				samples.resize(sampleCount, 0u); // 3 amostras de 2ch = 6
				field.resize(inputSize.x, 0.0f);
				frame.resize(inputLength);
			}

			//size_t getBitPeriod(){ return bitPeriod; }

			//size_t getLinePeriod(){ return linePeriod; }

			//size_t getSamplesPerField(){ return samplesPerLine; }

			//size_t getChannels(){ return channels; }

			//ivec2 getResolution(){ return resolution; }

			bool process(const ci::audio::Buffer& audioFrame, gl::Texture2dRef videoFrame)
			{
				const auto outputSize = context->getInputSize();
				const auto blocksPerLine = context->getBlocksPerLine();

				const auto* ch0 = audioFrame.getChannel(0u);
				const auto* ch1 = audioFrame.getChannel(1u);

				auto frameIter = frame.begin();

				//vec2 _testPatternNextPos;

				while (frameIter != frame.end())
				{
					// Converte amostras
					auto sampleIter = samples.begin();
					int16_t sampleInt = 0;
					for (unsigned i = 0u; i < blocksPerLine; i++)
					{
						*sampleIter++ = sampleInt = floatToFixedS16(*ch0++);
						*sampleIter++ = sampleInt = floatToFixedS16(*ch1++);
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

					for (unsigned i = 0u; i < blocksPerLine; i++)
					{
						fieldIter = writeBits16(fieldIter, &*sampleIter++);
						fieldIter = writeBits16(fieldIter, &*sampleIter++);
					}
					
					uint16_t checksum = crc16.checksum();
					fieldIter = writeBits16(fieldIter, &checksum);
					
					/*
					for (unsigned i = 0u; i < blocksPerLine; i++)
					{
						for (unsigned j = 0u; j < context->getChannels(); j++)
						{
							for (unsigned k = 0; k < 16; k++)
							{
								*fieldIter++ = _testPatternNextPos.y;
							}
						}
					}

					for (unsigned k = 0; k < 16; k++)
					{
						*fieldIter++ = _testPatternNextPos.y;
					}

					_testPatternNextPos.y += 1.0f;
					*/

					static const uint8_t lastBits = 0x78;
					fieldIter = writeBits8(fieldIter, &lastBits);

					// Preenche quadro;
					frameIter = std::copy(field.begin(), field.end(), frameIter);
				}

				videoFrame->update(frame.data(), GL_RED, GL_FLOAT, 0, outputSize.x, outputSize.y);

				return true;
			}
		};

		class Decoder
		{
		private:

			Context* context;
			std::vector<float> signal;
			std::vector<float> clock;

			void resizeBuffers(size_t length)
			{
				signal.resize(length);
				clock.resize(length);
			}

			void printBuffer(std::vector<float>::iterator beg, std::vector<float>::iterator end)
			{
				std::copy(beg, end, std::ostream_iterator<float>(std::cout));
				std::cout << std::endl;
			};

			void makeCopy(ci::Channel32f::Iter& channelIter)
			{
				auto signalIter = signal.begin();
				while (channelIter.pixel() || signalIter != signal.end())
				{
					*signalIter++ = channelIter.v();
				}
			};

			void makeClock()
			{
				//DSP::ClockParser clockParser;
				//clockParser.process(&signal);

				//TODO: DSP::Oscillator e depois DSP::SquareGenerator
				//DSP::ClockOscillator clockOscillator(static_cast<float>(clockParser.getLockedPeriod() * 2u), 0.85f);

				DSP::ClockOscillator clockOscillator(5.0f * 2.0f, 0.85f);
				clockOscillator.process(&clock);

				//printBuffer(clock.begin(), clock.begin() + 64);

				DSP::SchmittTrigger schmittTrigger(DSP::SchmittTrigger::Level{ 0.00f, 1.00f }, DSP::SchmittTrigger::Level{ 0.25f, 0.85f });
				schmittTrigger.process(&clock);

				DSP::EdgeTrigger edgeTrigger;
				edgeTrigger.process(&clock);

				//printBuffer(clock.begin(), clock.begin() + 64);
				//printBuffer(signal.begin(), signal.begin() + 64);
			};

			void makeDecode()
			{
				auto clockIter = clock.begin();
				auto outputIter = signal.begin();

				int nextBit = 0;

				while (clockIter != clock.end())
				{
					bool clockBit = *clockIter > 0.5f;

					if (clockBit)
					{
						// TODO: pegar constantes de um contexto
						//nota: ler 96 bits (16 * 6) a partir do 8º disparo/bit

						int value = 0;
						int bitShift = context->getBitDepth();
						int bitOffset = nextBit++ - 8;

						if (bitOffset >= 0 && bitOffset < 96)
						{
							auto nextIndex = std::distance(clock.begin(), clockIter);
							bool streamBit = signal.at(nextIndex) > 0.5f;
							std::cout << streamBit;
							//*outputIter++ = signal.at(std::distance(clock.begin(), clockIter));
						}
					}
					clockIter++;
				}
				std::cout << std::endl;
			};

		public:

			Decoder(Context* context)
				: context(context)
			{

			}

			bool process(const gl::Texture2dRef input, ci::audio::Buffer& output)
			{
				if (!input || output.isEmpty())
				{
					return false;
				}

				output.zero();

				ci::Surface32f surface(input->createSource());

				if (signal.size() != surface.getWidth())
				{
					resizeBuffers(surface.getWidth());
				}

				ci::Channel32f::Iter channelIter = surface.getChannelRed().getIter();

				while (channelIter.line())
				{
					makeCopy(channelIter);
					makeClock();
					makeDecode();
				}

				return true;
			}
		};







		class BitSymbolValueParser
		{
		private:

			int16_t value;
			int symbolMaxRead;

		public:

			BitSymbolValueParser()
				: value(0u)
				, symbolMaxRead(16u)
			{}

			std::vector<float>::iterator operator()(std::vector<float>::iterator beg, std::vector<float>::iterator end)
			{
				value = 0u;

				//std::memset(&value,0,sizeof value);

				int symbolsAvailable = std::distance(beg, end);

				if (symbolsAvailable > symbolMaxRead)
				{
					end = beg + symbolMaxRead;
				}

				while (beg != end)
				{
					int bitIndex = std::distance(beg, end)-1;
					value |= (*beg++ >= 0.5f ? 0 : 1) << bitIndex;
				}

				return end;
			}

			int16_t getLastValue(){ return value; }
		};





		class Decoder2
		{
		private:

			Context* context;
			gl::FboRef mFrameBuffer;
			gl::GlslProgRef mFrameProgram;

			std::vector<float> symbols;
			std::vector<int16_t> values;
			BitSymbolValueParser symbolValueParser;

			static const int clockOffset = 4;
			static const int blockOffset = 8; // clockOffset + 4;
			static const int crc16Offset = 104;	// blockOffset + 96;
			static const int whiteRefOffset = 120;	// crc16Offset + 16;

			void makeCopy(const gl::Texture2dRef input)
			{
				gl::ScopedFramebuffer scpFbo(mFrameBuffer);
				gl::ScopedViewport scpViewport(mFrameBuffer->getSize());
				gl::ScopedMatrices scpMatrices;
				gl::ScopedColor scpColor;
				gl::setMatricesWindow(mFrameBuffer->getSize(), false); // <== Nota: Invertido!
				gl::clear();

				if (input)
				{
					gl::draw(input, input->getBounds(), mFrameBuffer->getBounds());
				}
			}

			void makeDecode(ci::audio::Buffer& output)
			{
				output.zero();

				auto* ch0 = output.getChannel(0u);
				auto* ch1 = output.getChannel(1u);

				ci::Surface32f surface(mFrameBuffer->getColorTexture()->createSource());
				ci::Surface32f::Iter chnIter = surface.getIter();
				
				std::vector<float>::iterator symbolBeg, symbolEnd;
				std::vector<int16_t>::iterator valueBeg, valueEnd;
				
				while (chnIter.line())
				{
					// 6 amostras (3 blocos de 2 canais) + 1 valor de CRC16 = 7!
					valueBeg = values.begin();
					valueEnd = values.begin() + 7;

					// 16 simbolos = 16 bits!
					symbolBeg = symbols.begin();
					symbolEnd = symbols.begin() + 16;

					while (chnIter.pixel())
					{
						// amostras
						if (chnIter.mX >= blockOffset)
						{
							if (valueBeg != valueEnd)
							{
								float symbolRead = chnIter.r();
								*symbolBeg++ = symbolRead;
								if (symbolBeg == symbolEnd)
								{
									symbolBeg = symbols.begin();
									symbolValueParser(symbolBeg, symbolEnd);
									int16_t valueRead = symbolValueParser.getLastValue();
									*valueBeg++ = valueRead;
								}
							}
						}
					}

					uint16_t checksum = static_cast<uint16_t>(values.at(6));

					//TODO: calcular checksum das amostras capturadas.
					//TODO: replicar amostras anteriores caso checksum calculado seja incompativel com o checksum lido.

					valueBeg = values.begin();
					valueEnd = values.begin() + 6;

					while (valueBeg != valueEnd)
					{
						*ch0++ = fixedS16toFloat(*valueBeg++);
						*ch1++ = fixedS16toFloat(*valueBeg++);
					}
				}
			}

		public:

			Decoder2(Context* context)
				: context(context)
			{
				ivec2 inputSize = context->getInputSize();

				gl::Texture2d::Format texFormat;
				texFormat.internalFormat(GL_RG32F);
				texFormat.minFilter(GL_NEAREST);
				texFormat.magFilter(GL_NEAREST);
				texFormat.wrapS(GL_CLAMP_TO_BORDER);
				texFormat.wrapT(GL_CLAMP_TO_BORDER);

				mFrameBuffer = gl::Fbo::create(inputSize.x, inputSize.y, gl::Fbo::Format().colorTexture(texFormat));

				//mFrameProgram = gl::GlslProg::create();

				// Nota: Espaço suficiente para 32 unidades.
				symbols.resize(32,100.0f);
				values.resize(32,0);
			}

			gl::FboRef getFramebuffer(){ return mFrameBuffer; }
			
			bool process(const gl::Texture2dRef input, ci::audio::Buffer& output)
			{
				if (mFrameBuffer)
				{
					makeCopy(input);
					makeDecode(output);
					return true;
				}
				return false;
			}
		};
	}
}
