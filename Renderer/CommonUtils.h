#pragma once

#ifdef WIN32
#define WINAPI __declspec(dllexport)
#else 
#define WINAPI
#endif 

#include "glm/glm.hpp"
