#include "RF.hpp"
#include "Testes.hpp"

namespace RF{
	namespace Math{
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
	}

	void HSyncDetector::initialize()
	{
		mLogicModelBuffer = audio::Buffer(32u);
		//mLogicInputBuffer = audio::dsp::RingBuffer(mModelSignal.getNumFrames());

		vector<float> model = Testes::Padroes::GerarModelo2();
		copy(model.begin(), model.end(), mLogicModelBuffer.getChannel(0));
		setLevelSrcRange(vec2(1.0f, 0.0f));
		setLevelDstRange(vec2(1.0f, -1.0f));

		mTimer.emplace_back(Timer(44100));
	}

	void HSyncDetector::process(audio::Buffer* buffer)
	{
		float &ld0 = mSampleDelay.at(0);
		float &ld1 = mSampleDelay.at(1);

		for (unsigned c = 1u; c < buffer->getNumChannels(); c++){
			buffer->zeroChannel(c);
		}

		for (unsigned i = 0u; i < buffer->getNumFrames(); i++)
		{
			for_each(mTimer.begin(), mTimer.end(), [&](Timer& t){t.tick(); });

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

			if (!mLogicCompare && mEdgeState == EdgeState::EDGE_HIGH_LOW){
				mLogicCompare = true;
			}

			buffer->getChannel(2)[i] = testModelSampleQuality(inputSample);

			if (mLogicCompare){
				if (mLogicMatching){
					// TODO: ter opção de preencher mais amostras para deixar claro as 
					// outras partes do sistema no momento de detectar o sinal.
					buffer->getChannel(5)[i] = 1.0f;
					mLogicModelIndex = 0;
					mLogicCompare = false;
					mLogicMatching = false;
				}
				else if (testModelSampleMatch(inputSample)){
					buffer->getChannel(1)[i] = mLogicModelBuffer.getChannel(0)[mLogicModelIndex];
					//buffer->getChannel(2)[i] = testModelSampleQuality(inputSample);
					buffer->getChannel(3)[i] = getModelCompareProgress();
					buffer->getChannel(4)[i] = 1.0f;
					if (++mLogicModelIndex >= mLogicModelBuffer.getNumFrames()){
						mLogicMatching = true;
					}
				}
				else{
					buffer->getChannel(6)[i] = 1.0f;
					mLogicModelIndex = 0;
					mLogicCompare = false;
					mLogicMatching = false;
				}
			}
		}
	}
}
