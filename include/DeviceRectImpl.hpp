#pragma once
#include "cinder/app/App.h"
#include "DeviceRect.hpp"

using namespace ci::app;
using namespace std;

class DeviceRectImpl : public DeviceRect
{
private:

	vector<ci::gl::VboRef> mBuf;

public:

	DeviceRectImpl(Rectf r = Rectf(vec2(0.0f),vec2(1.0f)))
	{
		try{
			mBuf.push_back(ci::gl::Vbo::create(GL_ARRAY_BUFFER, vector<float>{r.x1, r.y1, r.x2, r.y1, r.x2, r.y2, r.x1, r.y2}));
			mBuf.push_back(ci::gl::Vbo::create(GL_ELEMENT_ARRAY_BUFFER, vector<unsigned>{0, 1, 3, 3, 1, 2}));
		}
		catch (ci::gl::Exception e){
			console() << __FUNCTION__ << ": " << e.what() << endl;
		}
	}

public:

	ci::gl::VboRef getVertices(){ return mBuf.front(); }

	ci::gl::VboRef getIndices(){ return mBuf.back(); }

};
