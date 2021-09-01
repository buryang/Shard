#include "Primitive.h"

namespace MetaInit
{
	Image::Image(Image&& image)
	{
		std::swap(raw_, image.raw_);
		size_ = image.size_;
	}

}