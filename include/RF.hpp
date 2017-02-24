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
				fill(off, off + 8, 0.0f); off += 8;
				fill(off, off + 8, 1.0f); off += 8;
				fill(off, off + 2, 0.0f); off += 2;
				fill(off, off + 2, 1.0f); off += 2;
				fill(off, off + 2, 0.0f); off += 2;
				fill(off, off + 2, 1.0f); off += 2;
				fill(off, off + 8, 0.0f);
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

	class HSyncDetector // : public Detector
	{
	private:

		bool				mModelCompare;
		bool				mModelMatching;
		float				mModelMatchFactor;
		size_t				mModelOffset;
		audio::Buffer		mModelSignal;

		float				mLevelHi;
		float				mLevelLo;
		float				mLevelMean;
		//array<size_t, 2>	mLevelCount;
		array<float, 2>		mLevelDelay;

		enum EdgeState{EDGE_NONE = 0, EDGE_HIGH_LOW = -1, EDGE_LOW_HIGH = 1};
		int	mEdgeState;
		bool mEdgeFound;
		bool mFirstLoop;

	private:

		float sampleClamp(float x, float a, float b){
			return min(max(x, a), b);
		}

		float sampleRemap(float x, float a1, float a2, float b1, float b2){
			return sampleClamp((x - a1) * (b2 - b1) / (a2 - a1) + b1, b1, b2);
		}

		float sampleNormalize(float x){
			return sampleRemap(x, mLevelHi, mLevelLo, -1.0f, 1.0f);
		}

		float sampleCompareProgress(){
			return sampleClamp(mModelOffset / static_cast<float>(mModelSignal.getNumFrames()), 0.0f, 1.0f);
		}

		float sampleQuality(float input){
			return abs(sampleNormalize(input));
		}

		bool sampleModelMatch(float input){
			// Nota: aceitar uma margem minima de 50% de aceitação
			// devido a imperfeições misturadas no sinal analógico.
			float model = mModelSignal.getChannel(0)[mModelOffset];
			input = sampleNormalize(input);
			model = sampleNormalize(model) * mModelMatchFactor;
			return abs(input) > abs(model) && signbit(input) == signbit(model);
		}

		void setEdgeLevels(float hi, float lo){
			mLevelHi = hi;
			mLevelLo = lo;
			mLevelMean = hi + (lo - hi) / 2.0f;
		}

	public:

		//HSyncDetector(VideoDecoderNode* vd) : Detector(vd)

		HSyncDetector()
			: mModelCompare(false)
			, mModelMatching(false)
			, mModelMatchFactor(0.5f)
			, mModelOffset(0)
			, mEdgeFound(false)
			, mEdgeState(0)
			, mFirstLoop(true)
		{}

		void initialize()
		{
			mModelSignal = audio::Buffer(32u);
			vector<float> model = Teste::Padroes::GerarModelo2();
			copy(model.begin(), model.end(), mModelSignal.getChannel(0));

			setEdgeLevels(1.0f, 0.0f);
		}

		void process(audio::Buffer* buffer)
		{
			float &ld0 = mLevelDelay.at(0);
			float &ld1 = mLevelDelay.at(1);

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
					mEdgeFound = ld0 != ld1; //= min(max(ld0, mLevelLo), mLevelHi) != min(max(ld1, mLevelLo), mLevelHi);

					if (!mEdgeFound){
						mEdgeState = EdgeState::EDGE_NONE;
					}
					else if (ld0 < mLevelMean){
						mEdgeState = EdgeState::EDGE_HIGH_LOW;
					}
					else{
						mEdgeState = EdgeState::EDGE_LOW_HIGH;
					}

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
					mModelCompare	= true;
				}

				if (mModelCompare){
					if (mModelMatching){
						// TODO: ter opção de preencher mais amostras para deixar claro as 
						// outras partes do sistema no momento de detectar o sinal.
						buffer->getChannel(5)[i] = 1.0f;
						mModelOffset	= 0;
						mModelCompare	= false;
						mModelMatching	= false;
					}
					else if (sampleModelMatch(inputSample)){
						buffer->getChannel(1)[i] = mModelSignal.getChannel(0)[mModelOffset];
						buffer->getChannel(2)[i] = sampleCompareProgress();
						buffer->getChannel(3)[i] = sampleQuality(inputSample);
						buffer->getChannel(4)[i] = 1.0f;
						if (++mModelOffset >= mModelSignal.getNumFrames()){
							mModelMatching = true;
						}
					}
					else{
						buffer->getChannel(6)[i] = 1.0f;
						mModelOffset	= 0;
						mModelCompare	= false;
						mModelMatching	= false;
					}
				}
			}
		}
	};
}




