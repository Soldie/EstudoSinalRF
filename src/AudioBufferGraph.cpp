#include "AudioBufferGraph.hpp"
#include "EstudoSinalRFApp.hpp"

void AudioBufferGraph::initTexture()
{
	gl::Texture2d::Format texFormat;
	texFormat.internalFormat(GL_R32F);
	texFormat.dataType(GL_FLOAT);
	texFormat.minFilter(GL_NEAREST);
	texFormat.magFilter(GL_NEAREST);
	texFormat.mipmap();

	mTex.front() = gl::Texture2d::create(
		nullptr,
		GL_RED,
		mAudioBuffer.getNumFrames(),
		mAudioBuffer.getNumChannels(),
		texFormat);
}

void AudioBufferGraph::initShader()
{
	try{

		EstudoSinalRFApp* app = static_cast<EstudoSinalRFApp*>(App::get());
		DeviceRect* rect = app->getDeviceRect();
		mBuf.at(0) = rect->getVertices();
		mBuf.at(1) = rect->getIndices();
		mVao.at(0) = gl::Vao::create();
		{

			gl::ScopedVao scopedVao(mVao.at(0));
			mBuf.at(0)->bind();
			mBuf.at(1)->bind();
			gl::enableVertexAttribArray(0);
			gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		}

	}
	catch (gl::Exception e){
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

		const float PI = atan(1.0)*4.0;

		float remap(float value, float smin, float smax, float dmin, float dmax)
		{
			return (value - smin) * (dmax - dmin) / (smax - smin) + dmin;
		}

		float coserp(float a, float b, float x)
		{
			return mix(a, b, 0.5 + 0.5*cos(PI + PI*x));
		}

		////////////////////////////////////////////////////////////////////////////////////////

		float calcLine(vec2 p, vec2 a, vec2 b, float r)
		{
			vec2 pa = p - a;
			vec2 ba = b - a;
			float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
			return length(pa - ba * h) - r;
		}

		float drawLine(vec2 res, vec2 uv, vec2 p1, vec2 p2, float r)
		{
			vec2 d = 1.0 / res;

			//uv -= 0.5;
			//uv.x *= res.y / res.x;
			//uv += 0.5;

			//p1.x += 0.5;
			//p2.x -= 0.5;

			return smoothstep(d.x*4.0, d.x*0.0, calcLine(uv, p1, p2, d.x*r));
		}

		////////////////////////////////////////////////////////////////////////////////////////

		float calcGridLine(float res_fbo, float res_chn, float map_value, vec2 map_range, bool wrap)
		{
			map_value = remap(map_value, 0.0, 1.0, map_range[0], map_range[1]);

			float line;

			if (wrap)
				line = 1.0 - abs(-1.0 + 2.0 * fract(map_value));
			else
				line = abs(-1.0 + 2.0 * map_value);

			float dist = (res_chn / res_fbo) * 2.0 * (abs(map_range[0]) + abs(map_range[1]));

			return smoothstep(dist + dist, 0.0, line);
		}

		float drawGridLine(float res_fbo, float res_chn, float map_value, vec2 map_range)
		{
			return calcGridLine(res_fbo, res_chn, map_value, map_range, true);
		}

		float drawWaveLine(float res_fbo, float res_chn, float map_value, vec2 map_range)
		{
			return calcGridLine(res_fbo, res_chn, map_value, map_range, false);
		}

		void viewTemplate(vec2 res_fbo, vec2 res_chn, vec2 view_id, vec2 view_uv, out vec3 color)
		{
			color = vec3(0.20);

			//camada, padrão1
			//color = mix(vec3(0.20), vec3(0.25), mod(view_id.x, 2.0));

			//camada, degradê
			color = mix(color*0.50, color*1.00, abs(-1.0 + 2.0 * view_uv.y));
			//linhas, separador, bloco
			color = mix(color, vec3(0.20), drawGridLine(res_fbo.x, res_chn.x, view_uv.x, vec2(0.0, 1.0)));

			if (view_id.y == 0.0){
				//linhas, referencia, limiar
				color += vec3(0.75, 0, 0) * drawWaveLine(res_fbo.y, res_chn.y, view_uv.y, vec2(-0.5, 1.5));
				//linhas, referencia, amplitude
				color += vec3(0, 0.75, 0) * drawGridLine(res_fbo.y, res_chn.y, view_uv.y, vec2(-0.5, 1.5));
			}
			else{
				//linhas, referencia, amplitude
				color = mix(color, vec3(0.30), drawGridLine(res_fbo.y, res_chn.y, view_uv.y, vec2(-0.5, 1.5)));
			}

			//linhas, separador, faixa
			color = mix(color, vec3(1.00), drawGridLine(res_fbo.y, res_chn.y, view_uv.y, vec2(0.0, 1.0)));
		}

		void viewWaveform(vec2 fbo_res, vec2 view_res, vec2 view_id, vec2 view_uv, out vec3 color)
		{
			vec2 uv = view_id / view_res;
			vec2 step = fbo_res / view_res;

			vec2 wave;

			wave.x = texture2D(iChnSrc, uv).r;
			wave.x = remap(wave.x, 0.0, 1.0, -0.25, 0.25);

			wave.y = texture2D(iChnSrc, uv + vec2(1.0 / view_res.x, 0.0)).r;
			wave.y = remap(wave.y, 0.0, 1.0, -0.25, 0.25);

			//float k = wave.x;
			//float k = mix(wave.x, wave.y, view_uv.x);

			float k = coserp(wave.x, wave.y, view_uv.x);
			color += vec3(0.1, 1.0, 0.2) * drawWaveLine(fbo_res.y, view_res.y, view_uv.y + k, vec2(0.0, 1.0));

			//color += vec3(0.1, 1.0, 0.2) * drawLine(view_res, view_uv, vec2(0.0, 0.5 + wave.x), vec2(1.0, 0.5 + wave.y), 1.0);
		}

		////////////////////////////////////////////////////////////////////////////////////////

		void main()
		{
			//vec2 _iChnRes = vec2(iChnRes.x, 1.0);
			vec2 _iChnRes = iChnRes;
			vec2 _iFboRes = iFboRes;

			vec2 uv = gl_FragCoord.xy / _iFboRes;
			uv.y = 1.0 - uv.y;

			vec2 fboPixelStep = 1.0 / _iFboRes;
			vec2 chnPixelStep = 1.0 / _iChnRes;

			vec2 view_id = floor(uv*_iChnRes);
			vec2 view_uv = fract(uv*_iChnRes);

			vec3 sDst;

			//linhas (amplitude, e blocos (trilhas x amostras))
			//sDst.r = calcLineBase(_iFboRes.x, _iChnRes.x, view_uv.x, vec2( 0.0, 1.0));
			//sDst.g = calcLineBase(_iFboRes.y, _iChnRes.y, view_uv.y, vec2( 0.0, 1.0));
			//sDst.b = calcLineBase(_iFboRes.y, _iChnRes.y, view_uv.y, vec2(-0.5, 1.5));

			//padrão zebra 
			//sDst.r = mod(view_id.x, 2.0);
			//sDst.g = mod(view_id.y, 2.0);

			//padrão xadrez 
			//sDst.r = mod(view_id.x + mod(view_id.y, 2.0), 2.0);
			//sDst.g = mod(view_id.y, 2.0);

			//mapa para espaço de amostra e faixa
			//sDst.r = view_uv.x;
			//sDst.g = view_uv.y;

			//sDst = mix(vec3(0.20), vec3(0.25), mod(view_id.x, 2.0));
			//sDst = mix(sDst*0.9, sDst, mod(view_id.y, 2.0));
			//sDst.r += calcLineBase(_iFboRes.x, _iChnRes.x, view_uv.x, vec2(0.0, 1.0));
			//sDst.g += calcLineBase(_iFboRes.y, _iChnRes.y, view_uv.y, vec2(0.0, 1.0));
			//sDst.b += calcLineBase(_iFboRes.y, _iChnRes.y, view_uv.y, vec2(-0.5, 1.5));

			viewTemplate(_iFboRes, _iChnRes, view_id, view_uv, sDst);
			viewWaveform(_iFboRes, _iChnRes, view_id, view_uv, sDst);

			fragOut = vec4(sDst, 1.0);
		}
		);

		gl::GlslProg::Format prgFormat;
		prgFormat.vertex(vSrc);
		prgFormat.fragment(fSrc);
		mPrg.at(0) = gl::GlslProg::create(prgFormat);

	}
	catch (gl::GlslProgExc e){
		cout << e.what() << endl;
	}
}

AudioBufferGraph::AudioBufferGraph(audio::Buffer& buffer, vector<string>& labels)
	: mAudioBuffer(buffer)
	, mLabels(labels.begin(), labels.begin() + min(mLabels.size(), buffer.getNumChannels()))
	, mFont("Courier New", 18.0f)
{
	mBuf.resize(2);
	mVao.resize(1);
	mPrg.resize(1);
	mTex.resize(1);

	initTexture();
	initShader();
	setGraph(buffer);
}

void AudioBufferGraph::setGraph(audio::Buffer& other)
{
	if (mAudioBuffer.isEmpty() || other.isEmpty()) return;

	mAudioBuffer.copy(other);

	mTex.front()->update(
		mAudioBuffer.getData(),
		GL_RED,
		GL_FLOAT,
		0,
		mAudioBuffer.getNumFrames(),
		mAudioBuffer.getNumChannels());
}

void AudioBufferGraph::draw(Rectf& windowBounds)
{
	AppBase& app = *App::get();

	gl::Texture2dRef& tex = mTex.front();

	if (tex){

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

			// TODO: desenhar shader por faixa ao invés de tudo junto
			// para poder ter mais controle sobre alcance de niveis e etc...
			if (prg)
			{
				gl::ScopedVao scopedVao(mVao.front());
				gl::ScopedGlslProg scopedProg(prg);
				gl::ScopedTextureBind scopedTex0(tex, 0);

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
					gl::drawString((*str).empty() ? ("Channel " + to_string(std::distance(mLabels.begin(), str))) : *str, ivec2(nextPos), Color::white(), mFont);
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
