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



	class HSyncDetector // : public Detector
	{
	private:

		float mLevelHi;
		float mLevelLo;
		float mLevelMean;

		bool mKeyCompare = false;
		bool mKeyMatching = false;
		size_t mKeyOffset = 0;
		audio::Buffer mKeySignal;

		int	mSwitchState = 0;
		array<size_t, 2> mLevelCount;
		array<float, 2> mLevelDelay;
		
		bool mTrigger;
		bool mBeginFill;

	public:

		//HSyncDetector(VideoDecoderNode* vd) : Detector(vd)

		HSyncDetector()
			: mTrigger(false)
			, mSwitchState(0)
			, mKeyOffset(0)
			, mKeyCompare(false)
			, mKeyMatching(false)
			, mBeginFill(true)
		{}

		void initialize()
		{	
			mLevelHi	= 1.0f;
			mLevelLo	= 0.0f;
			mLevelMean	= (mLevelHi - mLevelLo) / 2.0f;

			mKeySignal = audio::Buffer(10u);
			vector<float> keySignalSamples(mKeySignal.getNumFrames(), 0.0f);
			copy(keySignalSamples.begin(), keySignalSamples.end(), mKeySignal.getChannel(0));

			mLevelDelay.at(0) = 0.0f;
			mLevelDelay.at(1) = 0.0f;
		}

		void process(audio::Buffer* buffer)
		{
			float* ch0 = buffer->getChannel(0);

			float &ld0 = mLevelDelay.at(0);
			float &ld1 = mLevelDelay.at(1);

			//FIXME: tempo de aquecimento (caso de encontra nivel alto no buffer ao começo)

			for (int i = 0; i < buffer->getNumFrames(); i++)
			{
				float& currentSample = ch0[i];

				if (mBeginFill){
					ld0 = ld1 = currentSample;
					mBeginFill = false;
					currentSample = 0.0f;
					continue;
				}

				// Codigo de transição: 
				// -1 = transicao 1.0 -> 0.0
				// +1 = transicao 0.0 -> 1.0
				//  0 = nenhuma

				//FIXME: resetar caso anteriormente for detectado

				ld0 = currentSample;
				mTrigger = ld0 != ld1;
				mSwitchState = !mTrigger ? 0 : (ld0 < mLevelMean ? -1 : 1); // relativo a idéia (abaixo de levelMean, acima de mLevelMean)	
				ld1 = ld0;

				////////////////////////////////////////////////////////////

				// Rotina:
				// - Ao identificar transição alto-baixo, a rotina de comparação é ativada.
				// - Compara sinal da entrada com sinal modelo
				// - Caso sinal comparado desde a transicao forem identicos ao modelo, 
				//   a amostra do proximo laço tera atribuição de valor 1.0.
				// - Encerra-se a comparação.

				//FIXME: desativar após anteriormente detectado
				if (mSwitchState == -1){
					mKeyCompare = true;
					mKeyOffset = 0;
				}

				if (mKeyCompare){
					if (mKeyMatching){
						currentSample = 1.0f;
						mKeyMatching = mKeyCompare = false;
					}
					else if (currentSample != mKeySignal.getChannel(0)[mKeyOffset++]){
						mKeyMatching = false;
					}
					else if (mKeyOffset >= mKeySignal.getNumFrames()){
						mKeyMatching = true;
					}
					/*
					if (mKeyMatching){
						currentSample = 1.0f;
						mKeyOffset = 0;
						mKeyMatching = mKeyCompare = false;
					}
					*/
				}
				else{
					currentSample = 0.0f;
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
	function<void(audio::Buffer&)> printBuffer = [&](audio::Buffer& b){
		for (int ch = 0; ch < b.getNumChannels(); ch++){
			float* off = b.getChannel(ch);
			float* end = off + b.getNumFrames();
			console() << "Channel" << ch << " [";
			while (off != end) console() << *off++;
			console() << "]" << endl;
		}
		console() << endl;
	};

	audio::Buffer testBuffer = audio::Buffer(64u,1u);
	vector<float> keySignalSamples(testBuffer.getNumFrames(), 0.0f);
	fill(keySignalSamples.begin(), keySignalSamples.begin() + 5, 1.0f); 
	fill(keySignalSamples.begin() + 15, keySignalSamples.end(), 1.0f);
	copy(keySignalSamples.begin(), keySignalSamples.end(), testBuffer.getChannel(0));
	printBuffer(testBuffer);

	audio::Buffer hSyncBuffer(testBuffer);
	mHSyncDetector.initialize();
	mHSyncDetector.process(&hSyncBuffer);
	printBuffer(hSyncBuffer);
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
