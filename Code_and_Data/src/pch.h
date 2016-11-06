
#pragma once

// C++ headers
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>
#include <vector>
#include <utility>

// C headers
#include <cinttypes>
#include <cstdio>

// OS headers
#include <SDKDDKVer.h>
#define NOMINMAX
#include <windows.h>


// Libraries

// docopt
#include <docopt.cpp/docopt.h>

// OpenCL headers
#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <clhpp/cl2.hpp>

// optional
#include <optional/optional.hpp>
using std::experimental::optional;

