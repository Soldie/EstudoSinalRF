#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;


#include "cinder/audio/Buffer.h"
#include "cinder/audio/dsp/RingBuffer.h"
#include <random>
#include <stdexcept>
#include "cinder/Unicode.h"

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
			for_each(vec.begin(), vec.end(), [&](float& v){v = (float)((int)(v + rd(re) * 0.1f));});
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
		bool				mBeginFill;

	private:

		float mLevelHi, mLevelLo, mLevelMean;

		void setEdgeLevels(float hi, float lo)
		{
			mLevelHi	= hi;
			mLevelLo	= lo;
			mLevelMean	= hi + (lo - hi) / 2.0f;
		}

	public:

		//HSyncDetector(VideoDecoderNode* vd) : Detector(vd)

		HSyncDetector()
			: mEdgeFound(false)
			, mEdgeState(0)
			, mModelOffset(0)
			, mModelCompare(false)
			, mModelMatching(false)
			, mBeginFill(true)
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

			for (int i = 0; i < buffer->getNumFrames(); i++)
			{
				float& inputSample = ch0[i];

				if (mBeginFill){
					ld0 = ld1 = inputSample;
					mBeginFill = false;
				}
				else{
					// Codigo de transição: 
					// -1 = transicao 1.0 -> 0.0
					// +1 = transicao 0.0 -> 1.0
					//  0 = nenhuma
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
					mModelOffset	= 0;
					mModelCompare	= true;
					buffer->getChannel(2)[i] = 1.0f;
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
					
					float modelSample = mModelSignal.getChannel(0)[mModelOffset];

					if (mModelMatching){
						buffer->getChannel(3)[i] = 1.0f;
						//TODO: deixar preencher mais amostras para deixar claro as 
						//outras partes do sistema no momento de detectar o sinal.
						mModelCompare = mModelMatching = false;
					}
					else if (inputSample == modelSample){
						//FIXME: aplicar margem de erro no momento da comparação
						//para evitar menos problemas devido a imperfeições misturadas
						//no sinal.
						buffer->getChannel(1)[i] = modelSample == 0.0f ? 0.25f : modelSample;
						if (++mModelOffset >= mModelSignal.getNumFrames()){
							mModelMatching = true;
						}
					}
					else{
						mModelCompare = false;
						buffer->getChannel(4)[i] = 1.0f;
					}
				}				
			}
		}
	};

	
}









class AudioBufferGraph
{
private:

	audio::Buffer		mAudioBuffer;
	gl::Texture2dRef	mGraphTexture;
	vector<string>	mLabels;

	void initialize()
	{
		gl::Texture2d::Format texFormat;
		texFormat.internalFormat(GL_R32F);
		texFormat.dataType(GL_FLOAT);
		texFormat.minFilter(GL_NEAREST);
		texFormat.magFilter(GL_NEAREST);
		texFormat.mipmap();

		mGraphTexture = gl::Texture2d::create(
			nullptr,
			GL_RED,
			mAudioBuffer.getNumFrames(),
			mAudioBuffer.getNumChannels(),
			texFormat);
	}

public:

	AudioBufferGraph() = default;

	AudioBufferGraph(const AudioBufferGraph& copy)
	{
		mAudioBuffer = copy.mAudioBuffer;
		mGraphTexture = copy.mGraphTexture;
		mLabels = copy.mLabels;
	}

	AudioBufferGraph(audio::Buffer& buffer, vector<string>& labels)
		: mAudioBuffer(buffer)
		, mLabels(buffer.getNumChannels(), "Channel")
	{
		initialize();
		update(buffer);
		size_t numFrames = min(mLabels.size(), labels.size());
		copy(labels.begin(), labels.begin() + numFrames, mLabels.begin());
	}

	void update(audio::Buffer& other)
	{
		if (mAudioBuffer.isEmpty() || other.isEmpty()) return;

		mAudioBuffer.copy(other);

		mGraphTexture->update(
			mAudioBuffer.getData(),
			GL_RED,
			GL_FLOAT,
			0,
			mAudioBuffer.getNumFrames(),
			mAudioBuffer.getNumChannels());
	}

	void draw(Rectf& windowBounds)
	{
		AppBase& app = *App::get();

		gl::Texture2dRef& tex = mGraphTexture;

		if (mGraphTexture){
			
			gl::pushMatrices();
			gl::setMatricesWindow(app.getWindowSize(), false);
			gl::draw(tex, windowBounds);
			gl::popMatrices();

			gl::color(Color::white());

			try
			{
				//TODO: Usar shader de grade
				vec2 stepPer = vec2(windowBounds.getSize()) / vec2(tex->getSize());
				{
					gl::ScopedColor scpColor(Color::black());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (int i = 0; i < mAudioBuffer.getNumFrames(); i++){
						gl::drawLine(vec2(nextPos.x, windowBounds.getY1()), vec2(nextPos.x, windowBounds.getY2()));
						nextPos.x += stepPer.x;
					}
				}
				{
					gl::ScopedColor scpColor(Color::white());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (auto str = mLabels.begin(); str != mLabels.end(); str++){
						gl::drawLine(vec2(windowBounds.getX1(), nextPos.y), vec2(windowBounds.getX2(), nextPos.y));
						gl::drawString(*str, nextPos);
						nextPos.y += stepPer.y;
					}
				}
			}
			catch (...)
			{
				std::exception_ptr eptr = std::current_exception(); // capture
				
				try {
					if (eptr) {
						std::rethrow_exception(eptr);
					}
				}
				catch (const std::exception& e) {
					app.console() << __FUNCTION__ << ": " << e.what() << endl;
				}
			}
		}

		gl::drawStrokedRect(windowBounds);
	}
};
#include <codecvt>
#include <locale>

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

	// sinal
	// comparação inicio
	// comparação erro
	// comparação fim
	// proximidade da linear

	audio::Buffer testBuffer = audio::Buffer(64u, 1u);
	{
		vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo2();
		//RF::Teste::AplicarRuido(keySignalSamples);
		float* ptr = testBuffer.getChannel(0);
		std::fill(ptr, ptr + testBuffer.getSize(), 1.0f);
		std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr + 14);
	}

	//RF::Teste::ImprimirBufferAudio(testBuffer);

	audio::Buffer hSyncBuffer(testBuffer.getNumFrames(), 8u);
	{
		hSyncBuffer.copyChannel(0u, testBuffer.getChannel(0));
		mHSyncDetector.initialize();
		mHSyncDetector.process(&hSyncBuffer);
	}

	//RF::Teste::ImprimirBufferAudio(hSyncBuffer);


	mAudioBufferGraph = AudioBufferGraph(hSyncBuffer, vector<string>
	{
		// ATENÇÃO: Não usar caracteres unicode ou o programa quebrará
		"Entrada",
		"Modelo",
		"Comparacao, inicio",
		"Comparacao, identico",
		"Comparacao, diferente",
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
