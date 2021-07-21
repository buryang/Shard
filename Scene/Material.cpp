#include "Primitive.h"

namespace MetaInit
{
	Image::Image(Image&& image)
	{
		std::swap(data_, image.data_);
		size_ = image.size_;
	}

}