#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Unicode.h"
#include "RF.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class EstudoSinalRFApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	audio::MonitorNodeRef mMonitorNode;
	audio::InputDeviceNodeRef mInputDeviceNode;

	vector<audio::dsp::RingBuffer>	mHSyncRingBuffer;


	audio::Buffer		mHSyncBuffer;
	RF::HSyncDetector	mHSyncDetector;
	AudioBufferGraph	mAudioBufferGraph;
	
	void gerarSinalTeste(float* data, size_t length);
	void gerarSinalTeste(audio::Buffer& buffer);
};

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
	{
		vector<float> model = RF::Teste::Padroes::GerarModelo2();
		std::copy(model.begin(), model.end(), data);
	}
	data += 32;
	data += 8;
	{
		vector<float> model = RF::Teste::Padroes::GerarModelo1();
		std::copy(model.begin(), model.end(), data);
	}
	data += 32;
	data += 8;
	{
		vector<float> model = RF::Teste::Padroes::GerarModelo3();
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

void EstudoSinalRFApp::setup()
{
	try
	{
		audio::Context* ctx = audio::Context::master();
		mInputDeviceNode = ctx->createInputDeviceNode(audio::Device::getDefaultInput(), audio::Node::Format().autoEnable());
		mMonitorNode = ctx->makeNode<audio::MonitorNode>(audio::MonitorNode::Format().channels(1).autoEnable());
		mInputDeviceNode >> mMonitorNode >> ctx->getOutput();
		ctx->enable();
	}
	catch (exception e)
	{
		console() << e.what() << endl;
	}

	//Casos de teste:
	//- exatidão do algoritmos
	//- entrada com imperfeições (deverá não afetar a detecção)
	//- entrada com padrão levemente modificado (deverá dar erro)

	vector<float> model = RF::Teste::Padroes::GerarModelo1();

	//audio::dsp::RingBuffer rb(128u);
	//console() << rb.getSize() << "\t" << rb.getReadIndex() << "\t" << rb.getWriteIndex() << endl;
	//rb.write(model.data(), model.size());
	//console() << rb.getSize() << "\t" << rb.getReadIndex() << "\t" << rb.getWriteIndex() << endl;
	//rb.read(model.data(), model.size());
	//console() << rb.getSize() << "\t" << rb.getReadIndex() << "\t" << rb.getWriteIndex() << endl;

	mHSyncBuffer = audio::Buffer(128u, 8u);

	size_t ringBufferPaddingFactor = 10u;
	for (int i = 0; i < mHSyncBuffer.getNumChannels(); i++){
		mHSyncRingBuffer.emplace_back(audio::dsp::RingBuffer(mHSyncBuffer.getNumFrames() * ringBufferPaddingFactor));
	}

	mHSyncDetector.initialize();

	//////////////////////////////////////////////////////////////////////

	// ATENÇÃO: Não usar caracteres unicode ou o programa quebrará
	mAudioBufferGraph = AudioBufferGraph(mHSyncBuffer, vector<string>{
		"Sinal entrada",
		"Sinal modelo",
		"Comparador, qualidade",
		"Comparador, progresso",
		"Comparador, ativacao",
		"Comparador, compativel",
		"Comparador, incompativel",
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

	gerarSinalTeste(mHSyncBuffer);

	const audio::Buffer& buf = mMonitorNode->getBuffer();
	const float* src = buf.getChannel(0);
	float* dst = mHSyncBuffer.getChannel(0);
	audio::dsp::add(src, dst, dst, mHSyncBuffer.getNumFrames());
	
	mHSyncDetector.process(&mHSyncBuffer);
	mAudioBufferGraph.update(mHSyncBuffer);
	mAudioBufferGraph.draw(Rectf(getWindowBounds()));
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(1)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
