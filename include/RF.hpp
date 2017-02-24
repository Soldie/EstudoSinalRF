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

		bool				mModelCompare = false;
		bool				mModelMatching = false;
		size_t				mModelOffset = 0;
		audio::Buffer		mModelSignal;

		int					mEdgeState = 0;
		array<size_t, 2>	mLevelCount;
		array<float, 2>		mLevelDelay;

		bool				mEdgeFound;
		bool				mFirstLoop;

	private:

		float mLevelHi, mLevelLo, mLevelMean;

		float clamp(float x, float a, float b){
			return min(max(x, a), b);
		}

		float mapValue(float x, float a1, float a2, float b1, float b2){
			return clamp((x - a1) * (b2 - b1) / (a2 - a1) + b1, b1, b2);
		}

		float calcEdgeIntegrity(float x){
			return abs(mapValue(x, mLevelHi, mLevelLo, -1.0, 1.0));
		}

		float normalizeLevel(float x){
			return mapValue(x, mLevelHi, mLevelLo, -1.0f, 1.0f);
		}

		void setEdgeLevels(float hi, float lo){
			mLevelHi = hi;
			mLevelLo = lo;
			mLevelMean = hi + (lo - hi) / 2.0f;
		}

		bool checkModelMatch(float input){
			float model = mModelSignal.getChannel(0)[mModelOffset];
			// Nota: aceitar uma margem minima de 50% de integridade
			// devido a imperfeições misturadas no sinal analógico.
			input = normalizeLevel(input);
			model = normalizeLevel(model) * 0.5f;
			return abs(input) > abs(model) && signbit(input) == signbit(model);
		}

	public:

		//HSyncDetector(VideoDecoderNode* vd) : Detector(vd)

		HSyncDetector()
			: mEdgeFound(false)
			, mEdgeState(0)
			, mModelOffset(0)
			, mModelCompare(false)
			, mModelMatching(false)
			, mFirstLoop(true)
		{}

		void initialize()
		{
			setEdgeLevels(1.0f, 0.0f);

			mModelSignal = audio::Buffer(32u);
			vector<float> keySignalSamples = Teste::Padroes::GerarModelo2();
			copy(keySignalSamples.begin(), keySignalSamples.end(), mModelSignal.getChannel(0));

			mLevelDelay.at(0) = 0.0f;
			mLevelDelay.at(1) = 0.0f;
		}

		void process(audio::Buffer* buffer)
		{
			float* ch0 = buffer->getChannel(0);

			float &ld0 = mLevelDelay.at(0);
			float &ld1 = mLevelDelay.at(1);

			for (unsigned i = 0u; i < buffer->getNumFrames(); i++)
			{
				float& inputSample = ch0[i];

				// Codigo de transição: 
				// -1 = transicao 1.0 -> 0.0
				// +1 = transicao 0.0 -> 1.0
				//  0 = nenhuma

				if (mFirstLoop){
					ld0 = ld1 = inputSample;
					mFirstLoop = false;
				}
				else{
					ld0 = inputSample;
					mEdgeFound = ld0 != ld1; //= min(max(ld0, mLevelLo), mLevelHi) != min(max(ld1, mLevelLo), mLevelHi);
					mEdgeState = mEdgeFound ? (ld0 < mLevelMean ? -1 : 1) : 0; // relativo a idéia (abaixo de levelMean, acima de mLevelMean)	
					ld1 = ld0;
				}

				// Rotina:
				// - Rotina de comparação é ativada ao detectar uma transicao alta-baixa
				// - Compara sinal da entrada com sinal modelo/chave
				// - Caso sinal comparado desde a transicao forem identicos ao modelo, 
				//   a amostra do proximo laço tera atribuição de valor 1.0.
				// - Encerra-se a comparação.

				if (!mModelCompare && mEdgeState == -1){
					mModelOffset = 0;
					mModelCompare = true;
					buffer->getChannel(4)[i] = 1.0f;
				}

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

				//buffer->getChannel(1)[i] = 0.1f;

				if (mModelCompare){
					if (mModelMatching){
						// TODO: ter opção de preencher mais amostras para deixar claro as 
						// outras partes do sistema no momento de detectar o sinal.
						buffer->getChannel(5)[i] = 1.0f;
						mModelCompare = mModelMatching = false;
					}
					else if (checkModelMatch(inputSample)){
						buffer->getChannel(1)[i] = mModelSignal.getChannel(0)[mModelOffset];// == 0.0f ? 0.25f : modelSample;
						buffer->getChannel(2)[i] = mModelOffset / static_cast<float>(mModelSignal.getNumFrames());
						buffer->getChannel(3)[i] = calcEdgeIntegrity(inputSample);
						if (++mModelOffset >= mModelSignal.getNumFrames()){
							mModelMatching = true;
						}
					}
					else{
						mModelCompare = false;
						buffer->getChannel(6)[i] = 1.0f;
					}
				}
			}
		}
	};
}




