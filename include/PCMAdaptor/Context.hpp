#pragma once
#include <cinder/CinderGlm.h>

namespace PCMAdaptor
{
	class Context
	{
	private:
		size_t bitDepth;
		size_t bitPeriod;
		size_t linePeriod;
		size_t channels;
		size_t blocksPerLine;
		size_t samplesPerLine;
		glm::ivec2 iSize, oSize;

	public:

		Context()
		{
			bitDepth = 16u;
			bitPeriod = 5u;
			linePeriod = 2u;
			channels = 2u;
			blocksPerLine = 3u;
			samplesPerLine = blocksPerLine * channels;
			iSize = glm::ivec2(128, 245);
			oSize = iSize * glm::ivec2(bitPeriod, linePeriod);
		}

		size_t getBitDepth(){ return bitDepth; }

		size_t getBitPeriod(){ return bitPeriod; }

		size_t getLinePeriod(){ return linePeriod; }

		size_t getChannels(){ return channels; }

		size_t getBlocksPerLine(){ return blocksPerLine; }

		size_t getSamplesPerLine() { return samplesPerLine; }

		//size_t getSamplesCountPerField() { return getSamplesCountPerLine() * iSize.y; }

		glm::ivec2	getBitSize(){ return glm::ivec2(bitPeriod, linePeriod); }

		glm::ivec2	getInputSize(){ return iSize; }

		size_t		getInputLength(){return iSize.x*iSize.y;}

		glm::ivec2	getOutputSize(){ return oSize; }

		size_t		getOutputLength(){ return oSize.x*oSize.y; }

	};
}