#include "gtest/gtest.h"
#include <chrono>
#include "RHI/VulkanRHI.h"
//#include "Scene/Scene.h"

namespace {
	constexpr int WIDTH = 640;
	constexpr int HEIGHT = 480;

	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
}

TEST(TEST_METAINIT_RENDER, TEST_SIMPLE_EXAMPLE)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	auto* window = glfwCreateWindow(WIDTH, HEIGHT, "TEST_SIMPLE_EXAMPLE", nullptr, nullptr);
	glfwSetKeyCallback(window, key_callback);
	
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		//todo add render logic here
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}