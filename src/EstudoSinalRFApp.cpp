#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

#include <thrust\random.h>
#include <thrust\device_vector.h>
#include <thrust\host_vector.h>

#include <boost\crc.hpp>

/*
namespace DataImage
{
	class Base
	{
	protected:
		Base() = default;
	public:
		virtual string toString(){return "";}
	};


	class BitstreamFormat : public Base
	{
	protected:
		BitstreamFormat() = default;
	public:
		virtual size_t getBitCount() = 0;
	};

	class BitstreamFormatM1 : public BitstreamFormat
	{
	private:
		size_t mClockBits = 16;
		size_t mSampleCount = 6;
		size_t mSampleBits = 16;
		size_t mChecksumBits = 16;
	public:
		BitstreamFormatM1(int clockBits, int sampleBits, int sampleCount, int checksumBits)
			: mClockBits(clockBits)
			, mSampleBits(sampleBits)
			, mSampleCount(sampleCount)
			, mChecksumBits(checksumBits)
		{}
		size_t getBitCount()
		{ 
			return mClockBits + (mSampleCount*mSampleBits) + mChecksumBits; 
		}
		string toString() override
		{
			stringstream ss;
			ss << "Clock bits = " << mClockBits << endl;
			ss << "Sample count = " << mSampleCount << endl;
			ss << "Sample bits = " << mSampleBits << endl;
			ss << "Checksum bits = " << mChecksumBits << endl;
			return ss.str();
		}
	};


	class MediumFormat : public Base
	{
	protected:
		MediumFormat() = default;
	public:
	};

	class FrameMediumFormat : public MediumFormat
	{
	private:

		pair<size_t, size_t> mDimension;

	protected:

		FrameMediumFormat(const pair<size_t, size_t>& dimension) 
			: mDimension(dimension)
		{}

		string toString() override
		{
			stringstream ss;
			ss << "Image width = " << mDimension.first << endl;
			ss << "Image height = " << mDimension.second << endl;
			return ss.str();
		}

		void calculate(BitstreamFormat& bfmt)
		{
			//mBitsSample = floor(mDimension.first / bfmt.getBitCount());
			//mSamplesFrame = bfmt.getSampleCount() * mDimension.second;
			//mSampleBitsLine = mSamplesFrame * bfmt.getSampleBits();
			
			//Ex: 2880 * floor(44100/2880) = 43200 hz
			//Taxa de amostragem sem quadro incompleto = 43200hz
			//Taxa de amostragem com quadro incompleto = 44100hz
		}
	};

	class Calculator
	{
	private:

		int mPixelsBit;
		int mSamplesSecondCompatible;
		int mSamplesSecondTarget;
		int mSamplesFrame;

	public:

		Calculator(size_t samplesSecondTarget)
			: mSamplesSecondTarget(samplesSecondTarget)
			, mSamplesSecondCompatible(0)
		{}
		
		void calculate(const BitstreamFormat& bfmt, const MediumFormat& mfmt)
		{
			
		}

	};
}
*/

EstudoSinalRFApp::EstudoSinalRFApp()
	: mFft(512)
	, mBufferSpectral(mFft.getSize())
{

}

void EstudoSinalRFApp::gerarSinalTeste(float* data, size_t length)
{
	function<void(float*, float*)> ruidar = [](float* beg, float* end){
		for_each(beg, end, [&](float &v){
			static default_random_engine ro;
			static uniform_real_distribution<float> rd(-1.0f, 1.0f);
			v *= 0.90f;
			v += 0.05f;
			v += rd(ro) * 0.1f;
		});
	};

	function<void(float*, float*)> escalar = [](float* beg, float* end){
		AppBase* app = App::get();
		vec2 mousePos = vec2(app->getMousePos()) / vec2(app->getWindowSize());
		for_each(beg, end, [&](float &v){
			v = (v * mousePos.x) + mousePos.y;
		});
	};

	function<void(float*, float*)> cortar = [](float* beg, float* end){
		for_each(beg, end, [&](float &v){
			v = min(max(v, 0.0f), 1.0f);
		});
	};

	float* lastPtr = data;
	std::fill(data, data + length, 1.0f);
	data += 8;
	if (false)
	{
		vector<float> model = RF::Testes::Padroes::GerarModelo1();
		std::copy(model.begin(), model.end(), data);
	}
	data += 32;
	data += 8;
	if (true)
	{
		vector<float> model = RF::Testes::Padroes::GerarModelo2();
		std::copy(model.begin(), model.end(), data);
	}
	data += 32;
	data += 8;
	if (false)
	{
		vector<float> model = RF::Testes::Padroes::GerarModelo3();
		std::copy(model.begin(), model.end(), data);
	}
	data += 32;
	data = lastPtr;

	//ruidar(data, data + length);
	escalar(data, data + length);
	cortar(data, data + length);
}

void EstudoSinalRFApp::gerarSinalTeste(audio::Buffer& buffer)
{
	gerarSinalTeste(buffer.getChannel(0), buffer.getNumFrames());
}


struct RandGen
{
	__device__
	float operator () (int idx)
	{
		thrust::default_random_engine randEng;
		thrust::uniform_real_distribution<float> uniDist;
		randEng.discard(idx);
		return uniDist(randEng);
	}
};


void EstudoSinalRFApp::gerarQuadroVideoPCM(gl::Texture2dRef& tex)
{
	// Calculos usados:
	// - Dimensão 640 x 480 (com fator por bit 5:1, fica 128 x 480)
	// - Formato de dados por linha: Valores 16 bits (clock + 6 amostras + checksum = 128 bits)
	// - Fator aspecto por bit: 5 (640/128)

	// Base 640 pixels/linha:
	// - 640 bits, fator 1:1 = 40 valores de 16 bits
	// - 320 bits, fator 2:1 = 20 valores de 16 bits
	// - 160 bits, fator 4:1 = 10 valores de 16 bits
	// - 128 bits, fator 5:1 =  8 valores de 16 bits
	// -  80 bits, fator 8:1 =  5 valores de 16 bits 

	// Base 720 pixels/linha:
	// - 720 bits, fator 1:1 = 45 valores de 16 bits
	// - 120 bits, fator 6:1 =  7 valores de 16 bits + clock 8 bits


	size_t bitDepth = 16u;
	size_t samplesPerBlock = 6u;

	/////////////////////////////////////////////////////////////////

	/*
	std::default_random_engine re(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<uint16_t> rd;
	std::vector<uint16_t> values(samplesPerBlock * tex->getHeight());
	std::generate(values.begin(), values.end(), bind(re,rd));
	*/

	/*
	// Baseado em:
	// https://codeyarns.com/2013/10/31/how-to-generate-random-numbers-in-thrust/
	// FIXME: compilar com nvcc -> http://stackoverflow.com/questions/28965173/cuda-thrustdevice-vector-of-class-error
	thrust::host_vector<uint16_t> values(samplesPerBlock * tex->getHeight());
	thrust::device_vector<uint16_t> dvalues = values;
	const int randNum = dvalues.size();
	thrust::transform(thrust::make_counting_iterator(0),thrust::make_counting_iterator(randNum),dvalues.begin(),RandGen());
	thrust::copy(dvalues.begin(), dvalues.end(), values.begin());
	*/

	std::vector<uint16_t> values(samplesPerBlock * tex->getHeight());

	const double sampleRate = 44100.0;// tex->getHeight();
	const double samplePeriod = 1.0 / sampleRate;
	const double sampleValue = static_cast<double>(numeric_limits<int16_t>::max());
	const double frequency = 1.0;
	static double phase = 0.0;
	{
		auto value = values.begin();
		unsigned method = 0;
		double func = 0.0;

		while (value != values.end()){
			func = sin(2.0 * M_PI * phase);
			switch (method){
			default:
				*value = sampleValue * func;
				value++;
				phase = fract(phase + frequency * samplePeriod);
				break;
			case 1:
				fill(value, value + samplesPerBlock, sampleValue * func);
				value += samplesPerBlock;
				phase = fract(phase + frequency * samplePeriod);
				break;
			case 2:
				double step = 1.0 / tex->getHeight();
				fill(value, value + samplesPerBlock, sampleValue * func);
				func += step;
				value += samplesPerBlock;
			}
		}

		// simula error pointer
		// std::fill(values.end() - (samplesPerBlock * 1), values.end(), 0);
		std::fill(values.begin(), values.begin() + samplesPerBlock * 2, 0);
	}
	/*
	{
		auto it = values.begin();
		for (int i = 0; it != values.end(); i++){
			//fill(it, it + samplesPerBlock, i);
			//it += samplesPerBlock;
			*it++ = i;
		}
	}
	*/

	auto value = values.begin();

	/////////////////////////////////////////////////////////////////

	//typedef thrust::host_vector<float> FVec;
	typedef std::vector<float> FVec;
	typedef FVec::iterator FVecIter;

	static function<void(FVecIter, FVecIter, uint16_t)> encodeWord = [&](FVecIter beg, FVecIter end, uint16_t value){
		size_t bitShift = std::distance(beg, end);
		while (beg != end){
			*beg++ = static_cast<float>((value >> --bitShift) & 0x1);
		}
	};
	
	boost::crc_16_type	crc;
	vector<uint16_t>	samples(samplesPerBlock);
	vector<float>		bitBuf(tex->getWidth()*tex->getHeight());
	auto				bitRow = bitBuf.begin();
	auto				bitCol = bitRow;
	auto				bitEnd = bitRow;
	ivec2				sampleOffset;

	// Amostras intercaladas:
	//
	// Patente US4459696. Coluna 3, linha 20:
	// - In this case, the number of blocks of the interleave is 16, 
	// which is equivalent to a word-interleave of 3D = 48 words 
	// since there are twochannels and three words from each channel 
	// in a data block.
	//
	// Conclusões:
	// - um bloco é amostras por linha.
	// - cada bloco possui 6 amostras ou 3 pares de amostras, sem contar com 'dados de correção' e 'crc'.
	// - atraso ocorre em 16 blocos, multiplicado pelo fator do indice de coluna.
	// - sinal é entrelaçado, então os ponteiros de erro aparentam ser maior na saída final.

	// ponteiro de erro na 6a coluna deve estar no meio do quadro.
	int	interleaveBlocks = tex->getHeight() / ((samplesPerBlock - 1) * 2); // 16
	ivec3 interleaveOffset;

	while (bitRow != bitBuf.end())
	{
		// Clock
		//bitEnd = bitCol + 4;
		//encodeWord(bitCol, bitEnd, 0xA); // 1010
		//bitCol = bitEnd;

		bitEnd = bitCol + 8;
		encodeWord(bitCol, bitEnd, 0xAA); // 1010
		bitCol = bitEnd;
		
		// Amostras
		for (auto s = samples.begin(); s != samples.end(); s++)
		{
			sampleOffset.x = std::distance(samples.begin(), s);

			interleaveOffset.x = sampleOffset.x;
			interleaveOffset.y = (sampleOffset.y - sampleOffset.x * interleaveBlocks)  % tex->getHeight();
			interleaveOffset.z = interleaveOffset.x + interleaveOffset.y * samplesPerBlock;

			if (interleaveOffset.z < 0)
				interleaveOffset.z = values.size() - abs(interleaveOffset.z);

			*s = values.at(interleaveOffset.z);

			bitEnd = bitCol + bitDepth;
			encodeWord(bitCol, bitEnd, *s);
			//fill(bitCol, bitCol + bitDepth, abs(-1.0 + 2.0*(*s / sampleValue)));
			bitCol = bitEnd;
		}
		sampleOffset.y++;

		// Checksum (com boost::crc_16_type)
		crc.process_bytes(samples.data(), samples.size() * 2);
		bitEnd = bitCol + bitDepth;
		encodeWord(bitCol, bitEnd, crc.checksum());
		bitCol = bitEnd;

		// Clock
		//bitEnd = bitCol + 10;
		//encodeWord(bitCol, bitEnd, 0x1E1); // 0111100000
		//bitCol = bitEnd;
		
		bitEnd = bitCol + 8;
		encodeWord(bitCol, bitEnd, 0x55); // 1010
		bitCol = bitEnd;

		bitCol = bitRow += tex->getWidth();
	}

	tex->update(bitBuf.data(), GL_RED, GL_FLOAT, 0, tex->getWidth(), tex->getHeight());
}

void EstudoSinalRFApp::setup()
{
	mDeviceRect.reset(new DeviceRectImpl());

	//////////////////////////////////////////////////////////////////////

	gl::Texture2d::Format texFormat;
	texFormat.dataType(GL_FLOAT);
	texFormat.minFilter(GL_NEAREST);
	texFormat.magFilter(GL_NEAREST);

	size_t pixelsBit = 5u;
	mDataImageFrame = gl::Texture2d::create(640 / pixelsBit, 480, texFormat);

	// Casos de teste:
	// - exatidão do algoritmos
	// - entrada com imperfeições (deverá não afetar a detecção)
	// - entrada com padrão levemente modificado (deverá dar erro)

	// TODO: comparar amostras com modelo de n amostras
	// TODO: detecção bidimensional (amostra x modelo de n amostras)
	// TODO: nodo emissor de sinal

	mHSyncBuffer	= audio::Buffer(128u, 8u);
	mHSyncDetector	= RF::HSyncDetector::create();

	//////////////////////////////////////////////////////////////////////

	try
	{
		audio::Context* ctx = audio::Context::master();
		audio::Node::Format nodeFormat;
		nodeFormat.autoEnable();

		//Setup
		mInputDeviceNode	= ctx->createInputDeviceNode(audio::Device::getDefaultInput(), nodeFormat);
		mMonitorNode		= ctx->makeNode<audio::MonitorNode>(audio::MonitorNode::Format().autoEnable());
		mGenSineNode		= ctx->makeNode<audio::GenSineNode>(440.0f, nodeFormat);
		mGainNode			= ctx->makeNode<audio::GainNode>(0.1f, nodeFormat);
		
		//Input nodes
		mInputDeviceNode >> mMonitorNode;

		//Output nodes;
		mGenSineNode >> mGainNode >> ctx->getOutput();

		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	//////////////////////////////////////////////////////////////////////

	// ATENÇÃO: Não usar caracteres unicode ou o programa quebrará
	mAudioBufferGraph = AudioBufferGraph(mHSyncBuffer, vector<string>{
	//mAudioBufferGraph = AudioBufferGraph(mBufferSpectral, vector<string>{
		"Sinal entrada",
		"Sinal modelo",
		"Comparador, qualidade",
		"Comparador, progresso",
		"Comparador, ativacao",
		"Comparador, compativel",
		"Comparador, incompativel",
		"Sincronismo",
	});
}

void EstudoSinalRFApp::mouseDown( MouseEvent event )
{
}

void EstudoSinalRFApp::update()
{
}

void EstudoSinalRFApp::draw()
{
	gl::clear();

	//gerarSinalTeste(mHSyncBuffer);
	//const audio::Buffer& buf = mMonitorNode->getBuffer();
	//const float* src = buf.getChannel(0);
	//float* dst = mHSyncBuffer.getChannel(0);
	//audio::dsp::add(src, dst, dst, mHSyncBuffer.getNumFrames());
	//mHSyncDetector.process(&mHSyncBuffer);

	const audio::Buffer& buf = mMonitorNode->getBuffer();
	mFft.forward(&buf, &mBufferSpectral);

	mHSyncBuffer.copyChannel(0, mBufferSpectral.getReal());
	mHSyncBuffer.copyChannel(1, mBufferSpectral.getImag());

	{
		function<void(const float*, const float*, float*)> processSample = [&](const float* src1, const float* src2, float* dst){
			*dst = (*src1 + *src2) / 2.0f;
		};

		const float* src1 = mHSyncBuffer.getChannel(0);
		const float* src2 = mHSyncBuffer.getChannel(1);
		float* dst = mHSyncBuffer.getChannel(2);

		for (unsigned i = 0u; i < mHSyncBuffer.getNumFrames(); i++)
		{
			processSample(src1++, src2++, dst++);
		}
	}

	mAudioBufferGraph.setGraph(mHSyncBuffer);
	mAudioBufferGraph.draw(Rectf(getWindowBounds()));

	gl::color(Color::white());

	//gl::ScopedMatrices scpMatrices;
	//gl::setMatricesWindow(getWindowSize(),false);
	gerarQuadroVideoPCM(mDataImageFrame);
	Area src = mDataImageFrame->getBounds();
	Rectf dst = Rectf(vec2(0, src.getHeight()), vec2(src.getWidth()*5, 0));
	//Rectf dst = Rectf(vec2(0, src.getHeight()), vec2(src.getWidth(), 0));
	//Rectf dst = Rectf(vec2(0, getWindowHeight()), vec2(getWindowWidth(), 0));
	gl::draw(mDataImageFrame, src, dst);
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(1)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
