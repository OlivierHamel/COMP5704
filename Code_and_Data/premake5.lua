

local bIsWindows    = os.get() == "windows"
local strOclIncDir  = bIsWindows and "$(AMDAPPSDKROOT)/include"     or ""
local strOclLibDir  = bIsWindows and "$(AMDAPPSDKROOT)/lib/x86_64"  or ""

workspace "gpu_hcm"
   configurations { "debug", "release" }



project "gpu_hcm"
  architecture  "x86_64"
  kind          "ConsoleApp"
  language      "C++"
  targetdir     ".build/%{cfg.buildcfg}"
  objdir        ".build/%{cfg.buildcfg}/obj"

  includedirs { "lib", "src", strOclIncDir }
  libdirs     { strOclLibDir  }
  links       { "OpenCL"      } -- either a project's name, or the name of a sys lib w/o ext

  pchheader "pch.h"
  pchsource "src/pch.cpp"

  forceincludes "pch.h"

  files       { "src/**.h", "src/**.cpp", "lib/docopt.cpp/docopt.cpp" }
  flags       { }

  filter { "system:linux" }
    buildoptions { "-std=c++0x" }

  filter "configurations:debug"
    defines   { "DEBUG"   }
    symbols   "On"

  filter "configurations:release"
    defines   { "NDEBUG"  }
    symbols   "On"
    optimize  "On"

