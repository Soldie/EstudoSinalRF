#pragma once
#include "cinder\gl\gl.h"

class DeviceRect : public ci::Noncopyable
{
protected:

	DeviceRect() = default;

public:

	virtual ~DeviceRect() = default;

	virtual ci::gl::VboRef getVertices() = 0;

	virtual ci::gl::VboRef getIndices() = 0;

};
