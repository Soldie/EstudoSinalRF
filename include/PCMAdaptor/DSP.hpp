#pragma once
#include <cmath>
#include <vector>
#include <array>
#include <iostream>

namespace PCMAdaptor
{
	namespace DSP
	{
		class Processor
		{
		public:
			virtual ~Processor() = default;
			virtual void process(std::vector<float>* buffer) = 0;
		};

		class SchmittTrigger : public Processor
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
				, state(0)
			{}

			SchmittTrigger(Level signal, Level threshold)
				: signal(signal)
				, threshold(threshold)
				, state(0)
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

		class ClockParser : public Processor
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
					<< "locked=" << locked << ", "
					<< "transited=" << transited << ", "
					<< "periodAccum=" << periodAccum << ", "
					<< "periodCurrent=" << periodCurrent << ", "
					<< "periodLocked=" << periodLocked << ", "
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

			bool hasTransited(){ return transited; }

			bool isLearning(){ return learning; }

			bool isLocked(){ return locked; }

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

						//debugPrint();

						valueIter++;
					}
				}
			}
		};

		class ClockOscillator : public Processor
		{
		private:

			float mFrequency;
			float mPhase;
			float mTime;
			//std::array<float,2> mSamples;
			//bool fillFirstSample;

		public:

			ClockOscillator()
				: mFrequency(0.0f)
				, mPhase(0.0f)
			{
				reset();
			}

			ClockOscillator(float frequency, float phase = 0.0f)
				: mFrequency(frequency)
				, mPhase(phase)
			{
				reset();
				mTime += phase - floor(phase);
			}

			void reset()
			{
				//mTime = mSample[0] = mSample[1] = 0.0f;
				//fillFirstSample = true;
				mTime = 0.0f;
			}

			float getFrequency()
			{
				return mFrequency;
			}

			float getPhase()
			{
				return mPhase;
			}

			void process(std::vector<float>* buffer)
			{
				if (buffer)
				{
					if (mFrequency <= 1.0f)
					{
						std::fill(buffer->begin(), buffer->end(), floor(mFrequency) == 0.0f ? 0.0f : 1.0f);
					}
					else
					{
						/*
						static auto integralPart = [](float value)
						{
						float intPart(0.0f);
						modf(value, &intPart);
						return intPart;
						};
						*/

						auto valueIter = buffer->begin();
						float nextValue;
						while (valueIter != buffer->end())
						{
							float period = 1.0f / mFrequency;

							nextValue = floor(mTime*2.0);

							/*
							if (fillFirstSample){
							mSample[1] = mSample[0] = nextValue;
							fillFirstSample = false;
							}
							else{
							mSample[1] = mSample[0];
							mSample[0] = nextValue;
							}

							if (triggerEnabled){
							nextValue = mSample[0] == mSample[1] ? 0.0f : 1.0f;
							}
							*/

							*valueIter++ = nextValue;

							mTime += period;
							mTime -= floor(mTime);
						}
					}
				}
			}

		};

		class EdgeTrigger
		{
		private:

			std::array<float, 2> mSample;
			bool fillFirstSample;

		public:

			EdgeTrigger()
			{
				reset();
			}

			void reset()
			{
				mSample[0] = mSample[1] = 0.0f;
				fillFirstSample = true;
			}

			void process(std::vector<float>* buffer)
			{
				if (buffer)
				{
					auto valueIter = buffer->begin();
					float nextValue;
					while (valueIter != buffer->end())
					{
						nextValue = *valueIter;

						if (fillFirstSample){
							mSample[1] = mSample[0] = nextValue;
							fillFirstSample = false;
						}
						else{
							mSample[1] = mSample[0];
							mSample[0] = nextValue;
						}

						*valueIter++ = mSample[0] == mSample[1] ? 0.0f : 1.0f;
					}
				}
			}
		};
	}
}