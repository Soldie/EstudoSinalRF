#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;


#include "cinder/audio/Buffer.h"
#include "cinder/audio/dsp/RingBuffer.h"


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

		float				mLevelHi;
		float				mLevelLo;
		float				mLevelMean;

		bool				mModelCompare = false;
		bool				mModelMatching = false;
		size_t				mModelOffset = 0;
		audio::Buffer		mModelSignal;

		int					mSwitchState = 0;
		array<size_t, 2>	mLevelCount;
		array<float, 2>		mLevelDelay;
		
		bool				mTrigger;
		bool				mBeginFill;

	public:

		//HSyncDetector(VideoDecoderNode* vd) : Detector(vd)

		HSyncDetector()
			: mTrigger(false)
			, mSwitchState(0)
			, mModelOffset(0)
			, mModelCompare(false)
			, mModelMatching(false)
			, mBeginFill(true)
		{}

		void initialize()
		{	
			mLevelHi	= 1.0f;
			mLevelLo	= 0.0f;
			mLevelMean	= (mLevelHi - mLevelLo) / 2.0f;

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
				float& currentSample = ch0[i];

				if (mBeginFill){
					ld0 = ld1 = currentSample;
					mBeginFill = false;
				}
				else{
					// Codigo de transição: 
					// -1 = transicao 1.0 -> 0.0
					// +1 = transicao 0.0 -> 1.0
					//  0 = nenhuma
					ld0 = currentSample;
					mTrigger = ld0 != ld1;
					mSwitchState = !mTrigger ? 0 : (ld0 < mLevelMean ? -1 : 1); // relativo a idéia (abaixo de levelMean, acima de mLevelMean)	
					ld1 = ld0;
				}

				// Rotina:
				// - Rotina de comparação é ativada ao detectar uma transicao alta-baixa
				// - Compara sinal da entrada com sinal modelo/chave
				// - Caso sinal comparado desde a transicao forem identicos ao modelo, 
				//   a amostra do proximo laço tera atribuição de valor 1.0.
				// - Encerra-se a comparação.

				if (!mModelCompare && mSwitchState == -1){
					mModelOffset = 0;
					mModelCompare = true;
					buffer->getChannel(1)[i] = 1.0f;
				}

				if (mModelCompare){
					
					if (mModelMatching){
						buffer->getChannel(3)[i] = 1.0f;
						//TODO: deixar preencher mais amostras para dar garantia
						//de que as outras partes do sistema detectem o sinal.
						mModelCompare = mModelMatching = false;
					}
					//FIXME: aplicar margem de erro na comparação devido as imperfeições no domínio analógico.
					else if (currentSample == mModelSignal.getChannel(0)[mModelOffset]){
						if (++mModelOffset >= mModelSignal.getNumFrames()){
							mModelMatching = true;
						}
					}
					else{
						mModelCompare = false;
						buffer->getChannel(2)[i] = 1.0f;
					}
				}				
			}
		}
	};

	
}


class EstudoSinalRFApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
	RF::HSyncDetector mHSyncDetector;
};


void EstudoSinalRFApp::setup()
{
	audio::Buffer testBuffer = audio::Buffer(64u, 1u);
	{
		vector<float> keySignalSamples = RF::Teste::Padroes::GerarModelo2();
		float* ptr = testBuffer.getChannel(0);
		std::fill(ptr, ptr + testBuffer.getSize(), 1.0f);
		std::copy(keySignalSamples.begin(), keySignalSamples.end(), ptr + 14);
	}

	RF::Teste::ImprimirBufferAudio(testBuffer);

	audio::Buffer hSyncBuffer(testBuffer.getNumFrames(), 4u);
	{
		hSyncBuffer.copyChannel(0u, testBuffer.getChannel(0));
		mHSyncDetector.initialize();
		mHSyncDetector.process(&hSyncBuffer);
	}

	RF::Teste::ImprimirBufferAudio(hSyncBuffer);
}

void EstudoSinalRFApp::mouseDown( MouseEvent event )
{
}

void EstudoSinalRFApp::update()
{
}

void EstudoSinalRFApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP(EstudoSinalRFApp, RendererGl(RendererGl::Options().msaa(4)), [&](App::Settings *settings)
{
	settings->setConsoleWindowEnabled();
	//settings->setFrameRate(15.0f);
	//settings->disableFrameRate();
	settings->setWindowSize(ivec2(1024, 512));
});
