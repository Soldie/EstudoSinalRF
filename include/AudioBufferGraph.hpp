#pragma once
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class AudioBufferGraph
{
private:

	audio::Buffer			mAudioBuffer;
	vector<string>			mLabels;
	vector<gl::VaoRef>		mVao;
	vector<gl::VboRef>		mBuf;
	vector<gl::GlslProgRef> mPrg;
	gl::Texture2dRef		mGraphTexture;

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

	void loadShader()
	{
		mVao.resize(1);
		mBuf.resize(2);
		mPrg.resize(1);

		try{

			static const vector<float> vtx{ 0, 0, 1, 0, 1, 1, 0, 1 };
			static const vector<unsigned> idx{ 0, 1, 3, 3, 1, 2 };

			mVao.at(0) = gl::Vao::create();
			mBuf.at(0) = gl::Vbo::create(GL_ARRAY_BUFFER, vtx);
			mBuf.at(1) = gl::Vbo::create(GL_ELEMENT_ARRAY_BUFFER, idx);
			{
				gl::ScopedVao scopedVao(mVao.at(0));
				mBuf.at(0)->bind();
				mBuf.at(1)->bind();
				gl::enableVertexAttribArray(0);
				gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
			}

		}catch (gl::Exception e){
			cout << e.what() << endl;
		}

		try{

			static const string vSrc = CI_GLSL(440,
			in vec2 coord;
			uniform mat4 mModelViewProjMatrix;
			void main()
			{
				gl_Position = mModelViewProjMatrix * vec4(coord, 0, 1);
			}
			);

			static const string fSrc = CI_GLSL(440,
			uniform sampler2D iChnSrc;
			uniform vec2 iChnRes;
			uniform vec2 iFboRes;
			out vec4 fragOut;
			void main()
			{

				
				vec2 uv = gl_FragCoord.xy / iFboRes;

				vec2 fboPixelStep = 1.0 / iFboRes;
				vec2 chnPixelStep = 1.0 / iChnRes;

				/////////////////////////////////////

				// Identificador por:
				// x = amostra
				// y = faixa
				vec2 mask = floor(uv*iChnRes);

				// Referencia para:
				// x = alinhamento da amostra por posicao
				// y = limite de amplitude por faixa
				vec2 line = fract(uv * vec2(1.0, iChnRes.y));
				line.y = -0.2 + 1.4 * line.y;
				//line.y = (-1.0 + 2.0 * line.y) * 1.5; // amplitude entre -1.5 a +1.5
				//line.y = line.y * 1.5; // amplitude entre 0 a 1.5

				

				/////////////////////////////////////

				//FIXME: aspecto adaptavel relativo a resolução e fator de ampliação.
				vec2 dist = fboPixelStep*(iChnRes*2.0);
				vec2 grid = smoothstep(dist * 0.0, dist * 2.0, 1.0 - abs(-1.0 + 2.0 * fract(uv * iChnRes)));

				/////////////////////////////////////

				vec3 sSrc;
				vec3 sDst;
				
				vec2 sampleUV = vec2(line.x, 1.0 - uv.y);
				vec2 sampleOff = vec2(0, sampleUV.y);

				sampleOff.x = sampleUV.x - fboPixelStep.x;
				float s0 = texture2D(iChnSrc, sampleOff).r; // pixel n-1

				sampleOff.x = sampleUV.x;
				float s1 = texture2D(iChnSrc, sampleOff).r;

				sampleOff.x = sampleUV.x + fboPixelStep.x;
				float s2 = texture2D(iChnSrc, sampleOff).r;

				vec2 k1 = line.y + vec2(-dist.y, +dist.y) * 0.0;
				vec2 k2 = line.y + vec2(-dist.y, +dist.y) * 2.0;

				float s3 = smoothstep(k2.x, k1.x, s1) - smoothstep(k1.y, k2.y, s0);
				float s4 = smoothstep(k2.x, k1.x, s1) - smoothstep(k1.y, k2.y, s1);
				float s5 = smoothstep(k2.x, k1.x, s1) - smoothstep(k1.y, k2.y, s2);
				
				//sDst = vec3(max(max(s3,s4),s5));

				vec3 color1 = vec3(0.1, 1.0, 0.4);
				sDst = color1;
				sDst = mix(color1 * 0.10, sDst, s4);
				//sDst = mix(color1 * 0.08, sDst, grid.x);
				sDst = mix(color1 * 0.50, sDst, grid.y);

				/////////////////////////////////////

				//uv.y = 1.0 - uv.y;
				//sSrc = mix(vec3(0, 0, 0), vec3(1, 1, 1), texture2D(iChnSrc, uv).r);
				//sDst = sSrc;
				//sDst = mix(vec3(0), sDst, grid.x);
				//sDst = mix(vec3(1), sDst, grid.y);

				/////////////////////////////////////
				
				fragOut = vec4(sDst, 1.0);
			}
			);

			gl::GlslProg::Format prgFormat;
			prgFormat.vertex(vSrc);
			prgFormat.fragment(fSrc);
			mPrg.at(0) = gl::GlslProg::create(prgFormat);

		}catch (gl::GlslProgExc e){
			cout << e.what() << endl;
		}
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
		, mLabels(buffer.getNumChannels())
	{
		initialize();
		loadShader();
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

			//gl::pushMatrices();
			//gl::setMatricesWindow(app.getWindowSize(), false);
			//gl::draw(tex, windowBounds);
			//gl::popMatrices();

			{
				//gl::FboRef& fbo = mFbo.at(0);
				gl::GlslProgRef& prg = mPrg.at(0);
				//gl::ScopedFramebuffer scopedFbo(fbo);
				//gl::ScopedViewport scopedViewport(fbo->getSize());
				gl::ScopedMatrices scopedMatrices;
				gl::ScopedColor scopedColor;
				//gl::clear();

				if (prg)
				{
					gl::ScopedVao scopedVao(mVao.front());
					gl::ScopedGlslProg scopedProg(prg);
					gl::Texture2dRef& tex = mGraphTexture;
					gl::ScopedTextureBind scopedTex0(tex, 0);

					//prg->uniform("mModelViewProjMatrix", glm::ortho(0, 1, 1, 0));
					prg->uniform("mModelViewProjMatrix", glm::ortho(0, 1, 1, 0));
					prg->uniform("iChnSrc", 0);
					prg->uniform("iChnRes", vec2(tex->getSize()));
					prg->uniform("iFboRes", vec2(app.getWindowSize()));

					gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				}
			}
			//gl::popMatrices();
			gl::color(Color::white());

			try
			{
				//TODO: Usar shader de grade
				vec2 stepPer = vec2(windowBounds.getSize()) / vec2(tex->getSize());
				/*
				{
					gl::ScopedColor scpColor(Color::black());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (unsigned i = 0u; i < mAudioBuffer.getNumFrames(); i++){
						gl::drawLine(vec2(nextPos.x, windowBounds.getY1()), vec2(nextPos.x, windowBounds.getY2()));
						nextPos.x += stepPer.x;
					}
				}
				*/
				{
					gl::ScopedColor scpColor(Color::white());
					vec2 nextPos = windowBounds.getUpperLeft();
					for (auto str = mLabels.begin(); str != mLabels.end(); str++){
						//gl::drawLine(vec2(windowBounds.getX1(), nextPos.y), vec2(windowBounds.getX2(), nextPos.y));
						gl::drawString((*str).empty() ? ("Channel " + to_string(std::distance(mLabels.begin(), str))) : *str, nextPos);
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