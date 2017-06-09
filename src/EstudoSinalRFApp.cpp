#include "EstudoSinalRFApp.hpp"
#include "DeviceRectImpl.hpp"
#include "Testes.hpp"

//#define GLM_FORCE_SSE2
//#include <glm/gtx/simd_vec4.hpp>
#include <glm/glm.hpp>

//Nota 1: Thrust requer placa NVIDIA e compilador NVCC.
//#include <thrust\random.h>
//#include <thrust\device_vector.h>
//#include <thrust\host_vector.h>

//Nota 2: Por alguma razão, OpenCL interfere no uso de shaders OpenGL.
//#define USAR_OPENCL

/*
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
*/


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


#if defined USAR_OPENCL
#include <CL/cl.hpp>
#define CL_KERNEL_CODE(text) #text

void PCMVideoEncoder::setupCL()
{
	vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	if (!platforms.empty())
	{
		cout << "Platforms available:" << endl;

		for (auto platform : platforms)
			cout << "\t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

		cl::Platform selectedPlatform = cl::Platform::getDefault();
		cout << endl << "Platform selected: " << selectedPlatform.getInfo<CL_PLATFORM_NAME>() << endl << endl;

		vector<cl::Device> devices;
		selectedPlatform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

		if (!devices.empty())
		{
			cout << "Devices available:" << endl;
			for (auto device : devices){
				cout << "\t" << device.getInfo<CL_DEVICE_NAME>() << endl;
			}

			cl::Device selectedDevice = cl::Device::getDefault();
			cout << endl << "Device selected: " << selectedDevice.getInfo<CL_DEVICE_NAME>() << endl << endl;
			
			cl::Context context({ selectedDevice });

			//////////////////////////////////////////////////////////////////////////////
			
			string kernelCode = CL_KERNEL_CODE(
				void kernel simple_add(global const int* A, global const int* B, global int* C)
				{
					int id = get_global_id(0);
					C[id] = A[id] + B[id];
				}
			);

			//////////////////////////////////////////////////////////////////////////////
			
			cl::Program::Sources sources;
			sources.push_back({kernelCode.c_str(), kernelCode.length()});
			
			cl::Program program(context, sources);
			if (program.build({ selectedDevice }) == CL_SUCCESS)
			{
				vector<int> numbers(100);
				for (size_t i = 0; i < numbers.size(); i++)
				{
					int n = i + 1;
					numbers.at(i) = n*n;
				}
				size_t dataSize = sizeof(int) * numbers.size();

				///////////////////////////////////////////////////////////////////////////////

				cl::Buffer bufferA(context, CL_MEM_READ_WRITE, dataSize);
				cl::Buffer bufferB(context, CL_MEM_READ_WRITE, dataSize);
				cl::Buffer bufferC(context, CL_MEM_READ_WRITE, dataSize);

				///////////////////////////////////////////////////////////////////////////////

				cl::Kernel kernel_add = cl::Kernel(program, "simple_add");
				kernel_add.setArg(0, bufferA);
				kernel_add.setArg(1, bufferB);
				kernel_add.setArg(2, bufferC);

				///////////////////////////////////////////////////////////////////////////////
				
				cl::CommandQueue queue(context, selectedDevice);
				queue.enqueueWriteBuffer(bufferA, CL_TRUE, 0, dataSize, numbers.data());
				queue.enqueueWriteBuffer(bufferB, CL_TRUE, 0, dataSize, numbers.data());
				queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(numbers.size()), cl::NullRange);
				queue.finish();
				queue.enqueueReadBuffer(bufferC, CL_TRUE, 0, dataSize, numbers.data());

				///////////////////////////////////////////////////////////////////////////////

				cout << "Result: " << endl;
				for (size_t i = 0; i < numbers.size(); i++)
				{
					cout << numbers.at(i) << "\t";
				}
				cout << endl;
			}
			else
			{
				cout << "Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(selectedDevice) << endl;
			}
		}
		else
		{
			cout << "No devices found." << endl;
		}
	}
	else
	{
		cout << "No platforms available." << endl;
	}
}
#endif //INCLUIR_OPENCL

void PCMVideoEncoder::writeBitmap(vector<float>::iterator& cur, vector<float>::iterator& end, uint16_t value)
{
	size_t bitShift = std::distance(cur, end);

	while (cur != end)
		*cur++ = static_cast<float>((value >> --bitShift) & 0x1);

	cur = end;
};

void PCMVideoEncoder::updateStream(
	size_t samplesPerBlock, 
	size_t blocksPerFrame, 
	vector<uint16_t>& values)
{
	const double sampleRate = 44100.0;// tex->getHeight();
	const double samplePeriod = 1.0 / sampleRate;
	const double sampleValue = static_cast<double>(numeric_limits<int16_t>::max());
	const double frequency = 1.0;
	static double phase = 0.0;
	{
		auto value = values.begin();
		unsigned method = 0;
		double func = 0.0;

		// Nota: Tentativa de aceleração com SIMD através do GLM.
		// Há um preprocessador declarado em "Preprocessor definitions"
		// para tentar forçar o GLM a utilizar instruções SSE2.
		// Caminho: Properties > C++ > Preprocessor > Preprocessor definitions.
		dvec4 simd_value;
		dvec4 simd_steps = dvec4(samplePeriod) * dvec4(0, 1, 2, 3);

		while (value != values.end() || std::distance(value, values.end()) > 4){

			simd_value = dvec4(sampleValue) * glm::sin(dvec4(2.0 * M_PI * phase) + simd_steps);

			phase = fract(phase + frequency * (samplePeriod * 4));

			*value++ = static_cast<uint16_t>(simd_value.x);
			*value++ = static_cast<uint16_t>(simd_value.y);
			*value++ = static_cast<uint16_t>(simd_value.z);
			*value++ = static_cast<uint16_t>(simd_value.w);
		}
		
		/*
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
				double step = 1.0 / blocksPerFrame;
				fill(value, value + samplesPerBlock, sampleValue * func);
				func += step;
				value += samplesPerBlock;
			}
		}
		*/

		// simula error pointer
		// std::fill(values.end() - samplesPerBlock), values.end(), 0);
		std::fill(values.begin(), values.begin() + samplesPerBlock, 0);
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

}

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

void PCMVideoEncoder::updateFrame(gl::Texture2dRef& tex)
{
	auto	bitRow = bitmap.begin();
	auto	bitCol = bitRow;
	auto	bitEnd = bitmap.end();
	int		interleaveBlocks = 16 * 3;
	ivec3	interleaveOffset;
	ivec2	sampleOffset;

	//TODO: sinal entrelaçado

	while (bitRow != bitEnd)
	{
		// Clock
		//0x6AAA = 0110 10101010
		//0x5556 = 01010101 0110
		writeBitmap(bitCol, bitCol + 16, 0x5556); // 101010
		
		// Amostras
		for (auto sample = block.begin(); sample != block.end(); sample++)
		{
			sampleOffset.x = std::distance(block.begin(), sample);

			interleaveOffset.x = sampleOffset.x;
			interleaveOffset.y = (sampleOffset.y - sampleOffset.x * interleaveBlocks)  % tex->getHeight();
			interleaveOffset.z = interleaveOffset.x + interleaveOffset.y * samplesPerBlock;

			if (interleaveOffset.z < 0)
				interleaveOffset.z = stream.size() - abs(interleaveOffset.z);

			*sample = stream.at(interleaveOffset.z);

			writeBitmap(bitCol, bitCol + bitsPerSample, *sample);
		}
		sampleOffset.y++;

		// CRC
		crc.process_bytes(block.data(), block.size() * sizeof(uint16_t));
		writeBitmap(bitCol, bitCol + bitsPerSample, crc.checksum());

		// Clock
		//writeBitmap(bitCol, bitCol + 4, 0x6); // 0101

		bitCol = bitRow += tex->getWidth();
	}


	{
		/*
		gl::disableDepthRead();
		gl::disableDepthWrite();
		gl::ScopedTextureBind scpTex(mFrame);
		{
			gl::ScopedBuffer scpPbo(mPbo);
			mPbo->bufferData(bitmap.size() * sizeof(float), nullptr, GL_STREAM_DRAW);

			void* mem = mPbo->mapWriteOnly();

			if (mem)
			{
				console() << "udfhusfhd" << endl;
				std::memcpy(mem, bitmap.data(), bitmap.size() * sizeof(float));
				mPbo->unbind();
			}
			console() << "lalalalala" << endl;
		}
		*/
		tex->update(bitmap.data(), GL_RED, GL_FLOAT, 0, tex->getWidth(), tex->getHeight());
	}

}

void PCMVideoEncoder::setup()
{
#if defined USAR_OPENCL
	setupCL();
#endif // USAR_OPENCL 

	mSignalDraw = App::get()->getWindow()->getSignalPostDraw().connect(bind(&PCMVideoEncoder::draw, this));

	bitsPerSample	= 16u;
	samplesPerBlock = 6u;
	periodPerBit	= 5u;

	gl::Texture2d::Format texFormat;
	texFormat.dataType(GL_FLOAT);
	texFormat.minFilter(GL_NEAREST);
	texFormat.magFilter(GL_NEAREST);

	mFrame	= gl::Texture2d::create(640 / periodPerBit, 480, texFormat);
	bitmap	= vector<float>(mFrame->getWidth() * mFrame->getHeight());
	stream	= vector<uint16_t>(samplesPerBlock * mFrame->getHeight());
	block	= vector<uint16_t>(samplesPerBlock);
	mPbo	= gl::Pbo::create(GL_PIXEL_UNPACK_BUFFER, bitmap.size() * sizeof(float));
}

void PCMVideoEncoder::draw()
{
	updateStream(samplesPerBlock, mFrame->getHeight(), stream);
	updateFrame(mFrame);
	Area src = mFrame->getBounds();
	Rectf dst = Rectf(vec2(0, src.getHeight()), vec2(src.getWidth() * periodPerBit, 0));
	gl::draw(mFrame, src, dst);
	gl::drawString(to_string(App::get()->getAverageFps()), vec2());
}

PCMVideoEncoderRef PCMVideoEncoder::create()
{
	PCMVideoEncoderRef newInstance = make_shared<PCMVideoEncoder>();
	newInstance->setup();
	return newInstance;
}






void EstudoSinalRFApp::setup()
{
	mDeviceRect.reset(new DeviceRectImpl());

	mPCMVideoEncoder = PCMVideoEncoder::create();

	//////////////////////////////////////////////////////////////////////


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
	gl::color(Color::white());

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
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(1)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
