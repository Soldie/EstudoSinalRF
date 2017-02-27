#pragma once
#include "AudioBufferGraph.hpp"
#include <random>

using namespace ci;
using namespace ci::app;
using namespace std;

namespace RF
{
	/*
	class VideoDecoderNode
	{
	private:
	EdgeDetector mEdgeDetector;
	HSyncDetector mHSyncDetector;
	VSyncDetector mVSyncDetector;
	public:
	VideoDecoderNode()
	: mEdgeDetector(this)
	, mHSyncDetector(this)
	, mVSyncDetector(this)
	{}
	EdgeDetector& GetEdgeDetector(){ return mEdgeDetector; }
	HSyncDetector& GetHSyncDetector(){ return mHSyncDetector; }
	VSyncDetector& GetVSyncDetector(){ return mVSyncDetector; }
	};

	class Detector
	{
	private:
	VideoDecoderNode* mVideoDecoderNode;
	protected:
	Detector(VideoDecoderNode* vd) : mVideoDecoderNode(vd){}
	public:
	void initialize();
	void process(audio::Buffer* buffer);
	VideoDecoderNode* GetVideoDecoderNode(){ return mVideoDecoderNode; }
	};

	class EdgeDetector : public Detector
	{
	private:

	float mHighLevel, mLowLevel, mMeanLevel;
	unsigned mLowLevelCounter, mHighLevelCounter, mState;

	public:

	EdgeDetector(VideoDecoderNode* vd) : Detector(vd)
	{}

	void initialize()
	{
	mHighLevel = 0.0f;
	mLowLevel = -1.0f;
	mMeanLevel = (mHighLevel - mLowLevel) / 2.0f;
	mLowLevelCounter = mHighLevelCounter = mState = 0;
	}
	void process(audio::Buffer* buffer)
	{
	VideoDecoderNode* decoder = GetVideoDecoderNode();

	float* ch0 = buffer->getChannel(0);

	for (int i = 0; i < buffer->getNumFrames(); i++)
	{
	float &level = *ch0++;

	if (level < mMeanLevel){
	level = 0.0f;
	}
	else{
	level = 1.0f;
	}
	}
	}
	unsigned getHighLevelCount()
	{
	return mHighLevelCounter;
	}
	unsigned getLowLevelCount()
	{
	return mLowLevelCounter;
	}
	};


	class VSyncDetector : public Detector
	{
	public:
	VSyncDetector(VideoDecoderNode* vd) : Detector(vd)
	{}
	};

	*/

	namespace Teste
	{
		void ImprimirBufferAudio(audio::Buffer& b){
			for (unsigned ch = 0u; ch < b.getNumChannels(); ch++){
				float* off = b.getChannel(ch);
				float* end = off + b.getNumFrames();
				console() << "Channel" << ch << " [";
				while (off != end) console() << *off++;
				console() << "]" << endl;
			}
			console() << endl;
		};

		void AplicarRuido(vector<float>& vec)
		{
			static random_device ro;
			static default_random_engine re;
			static uniform_real_distribution<float> rd(-1.0f, 1.0f);
			for_each(vec.begin(), vec.end(), [&](float& v){v = (float)((int)(v + rd(re) * 0.1f)); });
		}

		namespace Padroes{
			vector<float> GerarModelo1(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				fill(off, off + 8, 0.0f); off += 8;
				fill(off, off + 8, 1.0f); off += 8;
				fill(off, off + 4, 0.0f); off += 4;
				fill(off, off + 4, 1.0f); off += 4;
				fill(off, off + 8, 0.0f);
				return keySignalSamples;
			}
			vector<float> GerarModelo2(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				float levelH = 1.0f;
				float levelL = 0.0f;
				fill(off, off + 8, levelL); off += 8;
				fill(off, off + 8, levelH); off += 8;
				fill(off, off + 2, levelL); off += 2;
				fill(off, off + 2, levelH); off += 2;
				fill(off, off + 2, levelL); off += 2;
				fill(off, off + 2, levelH); off += 2;
				fill(off, off + 8, levelL);
				return keySignalSamples;
			}
			vector<float> GerarModelo3(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				for (unsigned i = 0u; i < keySignalSamples.size() / 2u; i++){
					*off++ = static_cast<float>(i % 2u);
					*off++ = static_cast<float>(i % 2u);
				}
				return keySignalSamples;
			}
		}
	}


	//https://www.slideshare.net/yayavaram/digital-signal-processing-7604400
	//TODO: Separador de sinal para ADC de 1bit para recosntrução do sinal
	//TODO: AGC

	class LogicTrigger{ // aka edge detector

	};
	class LogicModel{

	};
	class LogicMatcher{
		void process(audio::Buffer* buffer);
	};

	class HSyncDetector // : public Detector
	{
	private:

		bool				mModelCompare;
		bool				mModelMatching;
		float				mModelMatchFactor;
		size_t				mModelOffset;

		//audio::dsp::RingBuffer	mModelInput;
		audio::Buffer			mModelSignal;
		

		vec2				mLevelSrcRange;
		float				mLevelSrcMean;
		vec2				mLevelDstRange;
		float				mLevelDstMean;
		array<float, 2>		mSampleDelay;

		enum EdgeState{ EDGE_NONE = 0, EDGE_HIGH_LOW = -1, EDGE_LOW_HIGH = 1 };
		int	mEdgeState;
		bool mFirstLoop;

	private:

		float sampleClamp(float x, float a, float b){
			return min(max(x, a), b);
		}

		float sampleClamp(float x, vec2& r){
			return sampleClamp(x, r.x, r.y);
		}

		float sampleLerp(float x, float a, float b){
			return a + (b - a) * x;
		}

		float sampleLerp(float x, vec2& r){
			return sampleLerp(x, r.x, r.y);
		}

		float sampleRemap(float x, float smin, float smax, float dmin, float dmax){
			return (x - smin) * (dmax - dmin) / (smax - smin) + dmin;
		}

		float sampleRemap(float x, vec2& src, vec2& dst){
			return sampleRemap(x, src.x, src.y, dst.x, dst.y);
		}

		float sampleNormalize(float x){
			return sampleRemap(x, mLevelSrcRange.x, mLevelSrcRange.y, mLevelDstRange.x, mLevelDstRange.y);
		}

		float getModelCompareProgress(){
			return sampleClamp(mModelOffset / static_cast<float>(mModelSignal.getNumFrames()), 0.0f, 1.0f);
		}

		float testModelSampleQuality(float input){
			//Nota: amostra mais perto da limiar deve ter qualidade mais próxima a 0%
			//TODO: aplicar uma distribuição normal relativa a posicao do input
			return abs(sampleNormalize(input));
		}

		bool testModelSampleMatch(float input){
			if (!mModelCompare) return false;
			float model = mModelSignal.getChannel(0)[mModelOffset];
			input = sampleNormalize(input);
			model = sampleNormalize(model);
			return abs(input) > mModelMatchFactor && signbit(input) == signbit(model);
		}

		int testEdgeSamplesMatch(float s1, float s0){

			s0 = sampleRemap(s0, mLevelSrcRange, mLevelDstRange);
			s1 = sampleRemap(s1, mLevelSrcRange, mLevelDstRange);

			// true = -, false = +
			bool b0 = signbit(s0);
			bool b1 = signbit(s1);

			return b0 == b1 ? EdgeState::EDGE_NONE : (b1 ? EdgeState::EDGE_LOW_HIGH : EdgeState::EDGE_HIGH_LOW);
		}

		void setLevelSrcRange(vec2& range){
			mLevelSrcRange	= range;
			mLevelSrcMean	= sampleLerp(0.5f, range);
		}

		void setLevelDstRange(vec2& range){
			mLevelDstRange	= range;
			mLevelDstMean	= sampleLerp(0.5f, range);
		}

	public:

		HSyncDetector()
			: mModelCompare(false)
			, mModelMatching(false)
			, mModelMatchFactor(0.0f)
			, mModelOffset(0)
			, mEdgeState(0)
			, mFirstLoop(true)
		{}

		void initialize()
		{
			mModelSignal = audio::Buffer(32u);
			//mModelInput = audio::dsp::RingBuffer(mModelSignal.getNumFrames());

			vector<float> model = Teste::Padroes::GerarModelo2();
			copy(model.begin(), model.end(), mModelSignal.getChannel(0));
			setLevelSrcRange(vec2(1.0f,  0.0f));
			setLevelDstRange(vec2(1.0f, -1.0f));
		}

		void process(audio::Buffer* buffer)
		{
			float &ld0 = mSampleDelay.at(0);
			float &ld1 = mSampleDelay.at(1);

			for (unsigned c = 1u; c < buffer->getNumChannels(); c++){
				buffer->zeroChannel(c);
			}

			for (unsigned i = 0u; i < buffer->getNumFrames(); i++)
			{
				float& inputSample = buffer->getChannel(0)[i];

				if (mFirstLoop){
					//TODO: intervalo de aquecimento por algumas amostras.
					ld0 = ld1 = inputSample;
					mFirstLoop = false;
				}
				else{
					ld0 = inputSample;
					mEdgeState = testEdgeSamplesMatch(ld1, ld0);
					ld1 = ld0;
				}

				// Codigo de transição: 
				// -1 = transicao 1.0 -> 0.0
				// +1 = transicao 0.0 -> 1.0
				//  0 = nenhuma
				//
				//////////////////////////////////////////////////////////////////
				//
				// Periodo de sinal retangular 
				//
				// n-x     -1   0      n+x
				// |________|___|________|
				// |        |\  |        |
				// |--------|-\-|--------| <- limiar média entre
				// |________|__\|________|    sinal alto e sinal baixo.
				//
				//  Nota: transição de um sinal retangular no
				//  domínio analógico não é uma transição perfeita!
				//
				//////////////////////////////////////////////////////////////////

				// Rotina:
				// - Rotina de comparação é ativada ao detectar uma transicao alta-baixa
				// - Compara sinal da entrada com sinal modelo/chave
				// - Caso sinal comparado desde a transicao forem identicos ao modelo, 
				//   a amostra do proximo laço tera atribuição de valor 1.0.
				// - Encerra-se a comparação.

				if (!mModelCompare && mEdgeState == EdgeState::EDGE_HIGH_LOW){
					mModelCompare = true;
				}

				buffer->getChannel(2)[i] = testModelSampleQuality(inputSample);

				if (mModelCompare){
					if (mModelMatching){
						// TODO: ter opção de preencher mais amostras para deixar claro as 
						// outras partes do sistema no momento de detectar o sinal.
						buffer->getChannel(5)[i] = 1.0f;
						mModelOffset = 0;
						mModelCompare = false;
						mModelMatching = false;
					}
					else if (testModelSampleMatch(inputSample)){
						buffer->getChannel(1)[i] = mModelSignal.getChannel(0)[mModelOffset];
						//buffer->getChannel(2)[i] = testModelSampleQuality(inputSample);
						buffer->getChannel(3)[i] = getModelCompareProgress();
						buffer->getChannel(4)[i] = 1.0f;
						if (++mModelOffset >= mModelSignal.getNumFrames()){
							mModelMatching = true;
						}
					}
					else{
						buffer->getChannel(6)[i] = 1.0f;
						mModelOffset = 0;
						mModelCompare = false;
						mModelMatching = false;
					}
				}
			}
		}
	};
}




