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
	namespace Video
	{
		static int16_t floatToFixedS16(float value)
		{
			return static_cast<int16_t>(32767.0f * value);
		}

		static float fixedS16toFloat(int16_t value)
		{
			return static_cast<float>(value) / 32767.0f;
		}

		class SymbolValueReader
		{
		private:

			int16_t value;
			int symbolMaxRead;

		public:

			SymbolValueReader()
				: value(0u)
				, symbolMaxRead(16u)
			{}

			std::vector<float>::iterator operator()(std::vector<float>::iterator beg, std::vector<float>::iterator end)
			{
				value = 0u;

				int symbolsAvailable = std::distance(beg, end);

				if (symbolsAvailable > symbolMaxRead)
				{
					end = beg + symbolMaxRead;
				}

				while (beg != end)
				{
					int bitIndex = std::distance(beg, end)-1;
					value |= (*beg++ < 0.5f ? 0 : 1) << bitIndex;
				}

				return end;
			}

			int16_t getLastValue(){ return value; }
		};

		class Encoder2
		{
		private:

			Context*				context;
			gl::FboRef				mFrameBuffer;
			gl::Texture2dRef		mFrameInput;
			gl::PboRef				mFramePboPack;
			std::vector<int16_t>	samples;
			std::vector<float>		symbols;
			std::vector<float>		frame;
			boost::crc_16_type		crc16;

			static std::vector<float>::iterator writeBits(std::vector<float>::iterator beg, size_t count, float lo, float hi, const void* data)
			{
				auto end = beg + count;

				const uint64_t& value = *reinterpret_cast<const uint64_t*>(data);

				while (beg != end)
				{
					int nextBit = std::distance(beg, end) - 1;
					*beg++ = ((value >> nextBit) & 0x1) ? hi : lo;
				}

				return end;
			}

			std::vector<float>::iterator writeBits16(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 16, 0.0f, 1.0f, data);
			}

			std::vector<float>::iterator writeBits8(std::vector<float>::iterator beg, const void* data)
			{
				return writeBits(beg, 8, 0.0f, 1.0f, data);
			}

		public:

			Encoder2(Context* context) : context(context)
			{
				auto inputSize = context->getInputSize();
				auto inputLength = context->getInputLength();
				auto outputSize = context->getOutputSize();
				auto outputLength = context->getOutputLength();
				auto samplesPerLine = context->getSamplesPerLine();
				
				gl::Texture2d::Format texFormat;
				texFormat.internalFormat(GL_R32F);
				texFormat.minFilter(GL_NEAREST);
				texFormat.magFilter(GL_NEAREST);
				texFormat.wrapS(GL_CLAMP_TO_BORDER);
				texFormat.wrapT(GL_CLAMP_TO_BORDER);

				mFrameInput = gl::Texture2d::create(inputSize.x, inputSize.y, texFormat);
				mFrameBuffer = gl::Fbo::create(outputSize.x, outputSize.y, gl::Fbo::Format().colorTexture(texFormat));
				mFramePboPack = gl::Pbo::create(GL_PIXEL_PACK_BUFFER, outputLength*sizeof(float),nullptr,GL_DYNAMIC_DRAW);

				samples.resize(samplesPerLine, 0u);
				symbols.resize(inputSize.x, 0.0f);
				frame.resize(inputLength);
			}

			void makeOutput(std::vector<float>& output)
			{
				// Cópia de entrada
				mFrameInput->update(
					frame.data(), 
					GL_RED, 
					GL_FLOAT, 
					0, 
					mFrameInput->getWidth(),
					mFrameInput->getHeight());

				// Redimensionamento
				{
					gl::ScopedFramebuffer scpFbo(mFrameBuffer);
					gl::ScopedViewport scpViewport(mFrameBuffer->getSize());
					gl::ScopedMatrices scpMatrices;
					gl::ScopedColor scpColor;
					gl::setMatricesWindow(mFrameBuffer->getSize());
					gl::clear();

					const auto bitSize = context->getBitSize();

					if (mFrameInput)
					{
						gl::draw(mFrameInput, Rectf(mFrameInput->getBounds()).scaled(bitSize));
					}
				}

				// Cópia de saída
				if (mFramePboPack)
				{
					auto fboTex = mFrameBuffer->getColorTexture();
					gl::ScopedTextureBind scpFboTex(fboTex);
					gl::ScopedBuffer scpPbo(mFramePboPack);
					glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0);

					float* ptr = static_cast<float*>(mFramePboPack->map(GL_READ_ONLY));

					if (ptr)
					{
						size_t length = fboTex->getWidth() * fboTex->getHeight();
						length = std::max(length, output.size());
						std::copy(ptr, ptr + length, output.begin());
						mFramePboPack->unmap();
					}
				}
			}

			void makeEncode(const std::vector<float>& input)
			{
				const auto blocksPerLine = context->getBlocksPerLine();

				const auto* audioInputPtr = input.data();

				std::vector<int16_t>::iterator sampleBeg, sampleEnd;
				std::vector<float>::iterator symbolBeg, symbolEnd, frameBeg, frameEnd;

				frameBeg = frame.begin();
				frameEnd = frame.end();

				static const uint8_t firstBits = 0x0A;
				static const uint8_t lastBits = 0x78;
				static const uint8_t zero = 0;

				uint8_t clockH;
				uint8_t clockV;
				uint8_t parity = 0;
				uint16_t checksum;

				while (frameBeg != frameEnd)
				{
					// Converte amostras
					sampleBeg = samples.begin();
					sampleEnd = samples.end();

					while (sampleBeg != sampleEnd)
					{
						*sampleBeg++ = floatToFixedS16(*audioInputPtr++);
						*sampleBeg++ = floatToFixedS16(*audioInputPtr++);
					}

					sampleBeg = samples.begin();
					sampleEnd = samples.begin() + 6;

					crc16.reset();
					crc16.process_block(&*sampleBeg, &*(sampleEnd - 1));

					clockH = 0xA;
					clockV = parity++ % 2 ? 0x0 : 0x1;
					checksum = crc16.checksum();

					// Escreve campo
					symbolBeg = symbols.begin();
					symbolBeg = writeBits(symbolBeg, 4, 0.00f, 0.00f, &zero);
					symbolBeg = writeBits(symbolBeg, 4, 0.25f, 0.85f, &clockH);

					while (sampleBeg != sampleEnd)
					{
						symbolBeg = writeBits(symbolBeg, 16, 0.25f, 0.85f, &*sampleBeg++);
						symbolBeg = writeBits(symbolBeg, 16, 0.25f, 0.85f, &*sampleBeg++);
					}

					symbolBeg = writeBits(symbolBeg, 16, 0.25f, 0.85f, &checksum);
					symbolBeg = writeBits(symbolBeg, 1, 0.25f, 0.85f, &clockV);
					symbolBeg = writeBits(symbolBeg, 3, 1.00f, 1.00f, &zero);
					symbolBeg = writeBits(symbolBeg, 4, 0.00f, 0.00f, &zero);

					// Preenche quadro;
					frameBeg = std::copy(symbols.begin(), symbols.end(), frameBeg);
				}
			}

			bool process(const std::vector<float>& input, std::vector<float>& output)
			{
				if (mFrameBuffer)
				{
					makeEncode(input);
					makeOutput(output);
					return true;
				}
				return false;
			}
		};


		class Decoder2
		{
		private:

			Context*				context;
			gl::FboRef				mFrameBuffer;
			gl::Texture2dRef		mFrameInput;
			gl::PboRef				mFramePboUnpack;
			std::vector<int16_t>	values;
			std::vector<float>		symbols;
			SymbolValueReader		symbolValueReader;
			boost::crc_16_type		crc16;

			static const int clockOffset = 4;
			static const int blockOffset = 8; // clockOffset + 4;
			static const int crc16Offset = 104;	// blockOffset + 96;
			static const int whiteRefOffset = 120;	// crc16Offset + 16;

			void makeInput(const std::vector<float>& input)
			{
				mFrameInput->update(
					input.data(),
					GL_RED,
					GL_FLOAT,
					0,
					mFrameInput->getWidth(),
					mFrameInput->getHeight());

				{
					gl::ScopedFramebuffer scpFbo(mFrameBuffer);
					gl::ScopedViewport scpViewport(mFrameBuffer->getSize());
					gl::ScopedMatrices scpMatrices;
					gl::ScopedColor scpColor;
					gl::setMatricesWindow(mFrameBuffer->getSize());
					gl::clear();
					gl::draw(mFrameInput, mFrameInput->getBounds(), mFrameBuffer->getBounds());
				}
			}

			void makeDecode(std::vector<float>& output)
			{
				auto* audioBufferPtr = output.data();

				ci::Surface32f surface(mFrameBuffer->getColorTexture()->createSource());
				ci::Surface32f::Iter chnIter = surface.getIter();
				
				std::vector<float>::iterator symbolBeg, symbolEnd;
				std::vector<int16_t>::iterator valueBeg, valueEnd;

				uint16_t checksumRead, checksumCalc;
				
				while (chnIter.line())
				{
					// 6 amostras (3 blocos de 2 canais) + 1 valor de CRC16 = 7!
					valueBeg = values.begin();
					valueEnd = values.begin() + 7;

					// 16 simbolos de 1 bit.
					symbolBeg = symbols.begin();
					symbolEnd = symbols.begin() + 16;

					while (chnIter.pixel())
					{
						if (chnIter.mX >= blockOffset)
						{
							if (valueBeg != valueEnd)
							{
								*symbolBeg++ = chnIter.r();

								if (symbolBeg == symbolEnd)
								{
									symbolBeg = symbols.begin();
									symbolValueReader(symbolBeg, symbolEnd);
									*valueBeg++ = symbolValueReader.getLastValue();
								}
							}
						}
					}

					valueBeg = values.begin();
					valueEnd = values.begin() + 6;

					crc16.reset();
					crc16.process_block(&*valueBeg, &*(valueEnd-1));

					checksumRead = static_cast<uint16_t>(*(values.end()-1));
					checksumCalc = static_cast<uint16_t>(crc16.checksum());

					bool checksumOk = checksumRead == checksumCalc;

					//TODO: preencher com amostras anteriores caso checksum seja incompativel.

					if (checksumOk)
					{
						while (valueBeg != valueEnd)
						{
							*audioBufferPtr++ = fixedS16toFloat(*valueBeg++);
							*audioBufferPtr++ = fixedS16toFloat(*valueBeg++);
						}
					}
					else
					{
						while (valueBeg != valueEnd)
						{
							*audioBufferPtr++ = 
							*audioBufferPtr++ = 0.0f;
							valueBeg += 2;
						}
					}
				}
			}

		public:

			Decoder2(Context* context)
				: context(context)
			{
				auto inputSize = context->getInputSize();
				//auto inputLength = context->getInputLength();
				auto outputSize = context->getOutputSize();
				//auto outputLength = context->getOutputLength();

				gl::Texture2d::Format texFormat;
				texFormat.internalFormat(GL_R32F);
				texFormat.minFilter(GL_NEAREST);
				texFormat.magFilter(GL_NEAREST);
				texFormat.wrapS(GL_CLAMP_TO_BORDER);
				texFormat.wrapT(GL_CLAMP_TO_BORDER);
				texFormat.label("Decoder input");

				mFrameInput = gl::Texture2d::create(outputSize.x, outputSize.y, texFormat);
				mFrameInput->setTopDown(true);
				mFrameBuffer = gl::Fbo::create(inputSize.x, inputSize.y, gl::Fbo::Format().colorTexture(texFormat));

				symbols.resize(16, 0.0f);
				values.resize(7, 0);
			}

			const gl::Texture2dRef getProcessedTexture(){ return mFrameBuffer->getColorTexture(); }
			
			bool process(const std::vector<float>& input, std::vector<float>& output)
			{
				if (mFrameBuffer)
				{
					makeInput(input);
					makeDecode(output);
					return true;
				}
				return false;
			}
		};
	}
}
