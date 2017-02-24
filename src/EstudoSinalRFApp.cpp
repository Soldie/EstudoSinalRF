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

	audio::Buffer		mHSyncBuffer;
	RF::HSyncDetector	mHSyncDetector;
	AudioBufferGraph	mAudioBufferGraph;
	
	void gerarSinalTeste(audio::Buffer& buffer);
};

void EstudoSinalRFApp::gerarSinalTeste(audio::Buffer& buffer)
{
	function<void(float*,float*)> ruidoFino = [](float* beg, float* end){
		for_each(beg, end, [&](float &v){
			static default_random_engine ro;
			static uniform_real_distribution<float> rd(-1.0f, 1.0f);
			v *= 0.9f;
			v += 0.05f;
			v += rd(ro) * 0.1f;
		});
	};

	//Teste simulado de sinal analógico com padrões e imperfeições.

	float* ptr = buffer.getChannel(0);
	std::fill(ptr, ptr + buffer.getSize(), 1.0f);
	{
		
		vector<float> model = RF::Teste::Padroes::GerarModelo2();
		ptr += 8;
		std::copy(model.begin(), model.end(), ptr);
	}
	{
		
		vector<float> model = RF::Teste::Padroes::GerarModelo1();
		ptr += (32 * 2) + 10;
		std::copy(model.begin(), model.end(), ptr);
	}
	{
		//ptr += 32 + 10;
		//vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo3();
		//std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr);
	}
	ruidoFino(buffer.getChannel(0), buffer.getChannel(0) + buffer.getNumFrames());
}

void EstudoSinalRFApp::setup()
{
	//Casos de teste:
	//- exatidão do algoritmos
	//- entrada com imperfeições (deverá não afetar a detecção)
	//- entrada com padrão levemente modificado (deverá dar erro)

	mHSyncBuffer = audio::Buffer(128u, 8u);
	//mHSyncBuffer.zero();
	//gerarSinalTeste(mHSyncBuffer);
	mHSyncDetector.initialize();
	//mHSyncDetector.process(&mHSyncBuffer);

	//////////////////////////////////////////////////////////////////////

	// ATENÇÃO: Não usar caracteres unicode ou o programa quebrará
	mAudioBufferGraph = AudioBufferGraph(mHSyncBuffer, vector<string>{
		"Sinal entrada",
		"Sinal modelo",
		"Comparador, progresso",
		"Comparador, qualidade",
		"Comparador, ativo",
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
