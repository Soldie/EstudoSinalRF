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
	RF::HSyncDetector mHSyncDetector;
	AudioBufferGraph mAudioBufferGraph;
};

void EstudoSinalRFApp::setup()
{
	//Casos de teste:
	//- exatidão do algoritmos
	//- entrada com imperfeições (deverá não afetar a detecção)
	//- entrada com padrão levemente modificado (deverá dar erro)

	audio::Buffer testBuffer = audio::Buffer(128u, 1u);
	{
		float* ptr = testBuffer.getChannel(0);
		std::fill(ptr, ptr + testBuffer.getSize(), 1.0f);
		{
			vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo2();
			ptr += 8;
			std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr);
		}
		{
			vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo1();
			ptr += 32 + 10;
			std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr);
		}
		{
			vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo3();
			ptr += 32 + 10;
			std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr);
		}
		

		float* beg = testBuffer.getChannel(0);
		float* end = testBuffer.getChannel(0) + testBuffer.getSize();
		for_each(beg, end, [&](float &v){
			static default_random_engine ro;
			static uniform_real_distribution<float> rd(-1.0f, 1.0f);
			v *= 0.8f;
			v += 0.1f;
			v += rd(ro) * 0.1f;
		});
	}

	//RF::Teste::ImprimirBufferAudio(testBuffer);

	audio::Buffer hSyncBuffer(testBuffer.getNumFrames(), 8u);
	{
		hSyncBuffer.copyChannel(0u, testBuffer.getChannel(0));
		mHSyncDetector.initialize();
		mHSyncDetector.process(&hSyncBuffer);
	}

	//RF::Teste::ImprimirBufferAudio(hSyncBuffer);

	//"Sinal entrada",
	//"Sinal modelo",
	//"Sinal limiar"

	mAudioBufferGraph = AudioBufferGraph(hSyncBuffer, vector<string>
	{
		// ATENÇÃO: Não usar caracteres unicode ou o programa quebrará
		"Sinal entrada",
		"Sinal modelo",
		"Comparador, progresso",
		"Comparador, integridade",
		"Comparador, analizar",
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
	mAudioBufferGraph.draw(Rectf(getWindowBounds()));
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options()), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
