#pragma once
#include "cinder/app/App.h"
#include "DeviceRect.hpp"

using namespace ci::app;
using namespace std;

class DeviceRectImpl : public DeviceRect
{
private:

	vector<ci::gl::VboRef> mBuf;
	gl::VaoRef mVao;

public:

	DeviceRectImpl(Rectf r = Rectf(vec2(0.0f),vec2(1.0f)))
	{
		try{
			mBuf.push_back(ci::gl::Vbo::create(GL_ARRAY_BUFFER, vector<float>
			{
				r.x1, r.y1, 0.0f, 1.0f,
				r.x2, r.y1, 1.0f, 1.0f,
				r.x2, r.y2, 1.0f, 0.0f,
				r.x1, r.y2, 0.0f, 0.0f,
			}));
			mBuf.push_back(ci::gl::Vbo::create(GL_ELEMENT_ARRAY_BUFFER, vector<unsigned>
			{
				0, 1, 3, 3, 1, 2
			}));
			mVao = gl::Vao::create();
			{
				gl::ScopedVao scpVao(mVao);
				mBuf.at(0)->bind();
				mBuf.at(1)->bind();
				gl::enableVertexAttribArray(0);
				gl::enableVertexAttribArray(1);
				size_t stride = sizeof(float) * 4;
				gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
				gl::vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const GLvoid*>(sizeof(float)*2));
			}
		}
		catch (ci::gl::Exception e){
			console() << __FUNCTION__ << ": " << e.what() << endl;
		}
	}

public:

	ci::gl::VboRef getVertices(){ return mBuf.at(0); }

	ci::gl::VboRef getIndices(){ return mBuf.at(1); }

	ci::gl::VaoRef getVao(){ return mVao; }

};
