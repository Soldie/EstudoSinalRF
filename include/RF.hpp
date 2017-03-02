#pragma once
#include "cinder\audio\Buffer.h"
#include "cinder\app\App.h"
#include <random>
#include <array>

using namespace ci;
using namespace ci::app;
using namespace std;

namespace RF{
	namespace Math{

		float sampleClamp(float x, float a, float b);

		float sampleClamp(float x, vec2& r);

		float sampleLerp(float x, float a, float b);

		float sampleLerp(float x, vec2& r);

		float sampleRemap(float x, float smin, float smax, float dmin, float dmax);

		float sampleRemap(float x, vec2& src, vec2& dst);
	}

	//https://www.slideshare.net/yayavaram/digital-signal-processing-7604400
	//TODO: Separador de sinal para ADC de 1bit para recosntrução do sinal
	//TODO: AGC
	//class LogicTrigger
	//class LogicModel
	//class LogicMatcher

	class HSyncDetector // : public Detector
	{
	private:

		struct Timer{
		private:
			size_t	mInterval;
			size_t	mTime;
			bool	mRunning;
		public:
			Timer() 
				: mTime(0u)
				, mInterval(0u)
				, mRunning(false){}
			Timer(size_t interval, bool run = false) 
				: mTime(0u)
				, mInterval(interval)
				, mRunning(false)
			{}
			void start(){ zero(); mRunning = true; }
			void tick(size_t samples = 1u){ if (mRunning) mTime += samples; }
			bool over(){ return mTime >= mInterval; }
			void zero(){ mTime = 0u; }
		};

		bool				mLogicCompare;
		bool				mLogicMatching;
		float				mLogicMinLevel;
		audio::Buffer		mLogicInputBuffer;
		size_t				mLogicInputIndex;
		audio::Buffer		mLogicModelBuffer;
		size_t				mLogicModelIndex;

		vector<Timer>		mTimer;

		vec2				mLevelSrcRange;
		float				mLevelSrcMean;
		vec2				mLevelDstRange;
		float				mLevelDstMean;
		array<float, 2>		mSampleDelay;

		enum EdgeState{ EDGE_NONE = 0, EDGE_HIGH_LOW = -1, EDGE_LOW_HIGH = 1 };
		int	mEdgeState;
		bool mFirstLoop;

	private:

		float normalizeSample(float x){
			return Math::sampleRemap(x, mLevelSrcRange.x, mLevelSrcRange.y, mLevelDstRange.x, mLevelDstRange.y);
		}

		float getModelCompareProgress(){
			return Math::sampleClamp(mLogicModelIndex / static_cast<float>(mLogicModelBuffer.getNumFrames()), 0.0f, 1.0f);
		}

		float testModelSampleQuality(float input){
			//Nota: amostra mais perto da limiar deve ter qualidade mais próxima a 0%
			//TODO: aplicar uma distribuição normal relativa a posicao do input
			return abs(normalizeSample(input));
		}

		bool testModelSampleMatch(float input){
			if (!mLogicCompare) return false;
			float model = mLogicModelBuffer.getChannel(0)[mLogicModelIndex];
			input = normalizeSample(input);
			model = normalizeSample(model);
			return abs(input) > mLogicMinLevel && signbit(input) == signbit(model);
		}

		int testEdgeSamplesMatch(float s1, float s0){

			s0 = Math::sampleRemap(s0, mLevelSrcRange, mLevelDstRange);
			s1 = Math::sampleRemap(s1, mLevelSrcRange, mLevelDstRange);

			// true = -, false = +
			bool b0 = signbit(s0);
			bool b1 = signbit(s1);

			return b0 == b1 ? EdgeState::EDGE_NONE : (b1 ? EdgeState::EDGE_LOW_HIGH : EdgeState::EDGE_HIGH_LOW);
		}

		void setLevelSrcRange(vec2& range){
			mLevelSrcRange	= range;
			mLevelSrcMean	= Math::sampleLerp(0.5f, range);
		}

		void setLevelDstRange(vec2& range){
			mLevelDstRange	= range;
			mLevelDstMean	= Math::sampleLerp(0.5f, range);
		}

	public:

		HSyncDetector()
			: mLogicCompare(false)
			, mLogicMatching(false)
			, mLogicMinLevel(0.0f)
			, mLogicModelIndex(0)
			, mEdgeState(0)
			, mFirstLoop(true)
		{}

		void initialize();

		void process(audio::Buffer* buffer);
	};
}




