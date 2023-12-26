#include "gtest/gtest.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	//::testing::GTEST_FLAG(filter) = "TEST_JOB_SYSTEM";
	return RUN_ALL_TESTS();
}