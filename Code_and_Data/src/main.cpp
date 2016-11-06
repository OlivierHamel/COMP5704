// comp5704_project.cpp : Defines the entry point for the console application.
//


namespace {

#ifdef _WIN32
// http://stackoverflow.com/questions/5248704/how-to-redirect-stdout-to-output-window-from-visual-studio
/// \brief This class is a derivate of basic_stringbuf which will output all the written data using the OutputDebugString function
template<typename TChar>
class OutputDebugStringBuf : public std::basic_stringbuf<TChar, std::char_traits<TChar>> {
public:
    using TTraits = std::char_traits<TChar>;
    static_assert(std::is_same<TChar,char>::value || std::is_same<TChar,wchar_t>::value
                 ,"OutputDebugStringBuf only supports char and wchar_t types");

    explicit OutputDebugStringBuf() : _buffer(256) {
        setg(nullptr, nullptr, nullptr);
        setp(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
    }

    int sync() {
        try {
            MessageOutputer<TChar,TTraits>()(pbase(), pptr());
            setp(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
            return 0;
        } catch(...) {
            return -1;
        }
    }

    int overflow(int c = TTraits::eof()) {
        auto syncRet = sync();
        if (c != TTraits::eof()) {
            _buffer[0] = char(c);
            setp(_buffer.data(), _buffer.data() + 1, _buffer.data() + _buffer.size());
        }
        return syncRet == -1 ? TTraits::eof() : 0;
    }


private:
    std::vector<TChar>      _buffer;

    template<typename TChar, typename TTraits>
    struct MessageOutputer;

    template<>
    struct MessageOutputer<char,std::char_traits<char>> {
        template<typename TIterator>
        void operator()(TIterator begin, TIterator end) const {
            std::string s(begin, end);
            OutputDebugStringA(s.c_str());
        }
    };

    template<>
    struct MessageOutputer<wchar_t,std::char_traits<wchar_t>> {
        template<typename TIterator>
        void operator()(TIterator begin, TIterator end) const {
            std::wstring s(begin, end);
            OutputDebugStringW(s.c_str());
        }
    };
};

static OutputDebugStringBuf<char    > charDebugOutput;
static OutputDebugStringBuf<wchar_t > wcharDebugOutput;
#endif


auto const kDocOptsUsage = R"[[(
comp5704project
    olivier hamel - comp5704 project - eikonal gpu solver

Usage:
    comp5704project [options]
    comp5704project info [options]
    comp5704project (-h | --help)
    comp5704project --version

info                Prints info regarding the default OpenCL platform on this system.

Options:
    -h --help               Show this screen.
    --version               Show version.
    -d --devicetype <type>  Specify which device type to use. Options: gpu, cpu, any [default: gpu]
)[[";



auto const k_kernel_src =
#include "eikonal.cl"
;

}

template<typename F>
auto cl_try(char const* const taskName, F&& task) -> std::result_of_t<F()> {
    try { return task(); }
    catch (const cl::Error& e) {
        std::cout << "OpenCL Error.\n\tTask: " << taskName << "\n\tError: " << e.what() << "\n";
        exit(-1);
    }
}


std::vector<std::string> split_string_by_spaces_dump_empty(char const* const s) {
    if (!(s && *s)) return {};

    std::vector<std::string> exts;
    auto const addExt = [&](char const* const head, char const* const end) {
        auto const len = end - head;
        if (0 < len) exts.push_back(std::string(head, len));
    };

    auto const  s_end = s + strlen(s);
    char const* head  = s;
    while (char const* end = strchr(head, ' ')) {
        addExt(head, end);
        head = end + 1;
    }

    // last one isn't picked up by the main loop
    addExt(head, s + strlen(s));
    return exts;
}

std::vector<std::string> cl_splice_extensions(char const* const s)
{ return split_string_by_spaces_dump_empty(s); }

std::string display_format_mem(cl_ulong const bytes) {
    char const* k_suffixes[] = {"bytes", "KiB", "MiB", "GiB"};
    auto const  k_num_suffix = sizeof(k_suffixes) / sizeof(k_suffixes[0]);

    size_t scale  = 0;
    double f      = (double)bytes;
    for (; (scale < k_num_suffix) && (1024 < f); ++scale)
        f /= 1024;

    char buffer[512];
    sprintf_s(buffer, "%f %s", f, k_suffixes[scale]);
    return buffer;
}


std::string cl_device_type_string(cl_device_type const type) {
    if (type == 0) return "<none?>";

    std::string s;
    auto const add = [&](cl_device_type const flag, char const* name) {
        if (!(type & flag)  ) return;
        if (!s.empty()      ) s += " ";
        s += name;
    };
    add(CL_DEVICE_TYPE_CPU          , "CPU");
    add(CL_DEVICE_TYPE_GPU          , "GPU");
    add(CL_DEVICE_TYPE_ACCELERATOR  , "Accelerator");
    add(CL_DEVICE_TYPE_CUSTOM       , "Custom");
    return s.empty() ? "<unknown>" : s;
}

optional<cl_device_type> cl_device_type_parse(char const* const s) {
    auto const parts = split_string_by_spaces_dump_empty(s);
    if (parts.empty()) return {};

    cl_device_type  device_type = 0;
    for (auto&& p : parts) {
        auto const matches = [&](char const* const k) { return _strcmpi(p.c_str(), k) == 0; };
             if (matches("all"          )) return CL_DEVICE_TYPE_ALL;
        else if (matches("cpu"          )) device_type |= CL_DEVICE_TYPE_CPU;
        else if (matches("gpu"          )) device_type |= CL_DEVICE_TYPE_GPU;
        else if (matches("accelerator"  )) device_type |= CL_DEVICE_TYPE_ACCELERATOR;
        else if (matches("custom"       )) device_type |= CL_DEVICE_TYPE_CUSTOM;
        else    return {};
    }

    return device_type;
}


int main(int const argc, char const* const argv[]) {
#ifdef _WIN32
    if (IsDebuggerPresent()) {
        std::cerr.rdbuf(&charDebugOutput);
        std::clog.rdbuf(&charDebugOutput);
        std::cout.rdbuf(&charDebugOutput);

        std::wcerr.rdbuf(&wcharDebugOutput);
        std::wclog.rdbuf(&wcharDebugOutput);
        std::wcout.rdbuf(&wcharDebugOutput);
    }
#endif

    auto const cmd_args     = docopt::docopt(kDocOptsUsage, { argv + 1, argv + argc }
                                            ,true                   /* show help if requested */
                                            ,"comp5704 project 0.1" /* version string */);
    auto const device_type  = cl_device_type_parse(cmd_args.at("--devicetype").asString().c_str());
    if (!device_type) {
        std::cout << "Invalid --devicetype value: " << cmd_args.at("--devicetype").asString() << "\n"
                  << "    See --help for valid device types.\n";
        return -1;
    }

    cl::Platform platform = cl_try("getting default platform object"
                                  ,[] { return cl::Platform::getDefault(); });
    cl::vector<cl::Device> devices;
    cl_try("fetching device list", [&] { platform.getDevices(*device_type, &devices); });

    struct Device_Info {
        cl::Device  device;
        std::string name, vendor, version, profile, type;
        std::vector<std::string> extensions;
        cl_uint  hzMax;
        cl_ulong memLocal, memGlobal;
        cl_bool  available;
    };

    std::vector<Device_Info> devices_info(devices.size());
    cl_try("fetching device info", [&] {
        std::transform(devices.begin(), devices.end(), devices_info.begin(), [&](cl::Device& d) {
            Device_Info info {};
            info.device     = d;
            info.name       = d.getInfo<CL_DEVICE_NAME>();
            info.vendor     = d.getInfo<CL_DEVICE_VENDOR>();
            info.version    = d.getInfo<CL_DEVICE_VERSION>();
            info.profile    = d.getInfo<CL_DEVICE_PROFILE>();
            info.type       = cl_device_type_string(d.getInfo<CL_DEVICE_TYPE>());
            info.hzMax      = d.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
            info.memLocal   = d.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
            info.memGlobal  = d.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
            info.available  = d.getInfo<CL_DEVICE_AVAILABLE>();
            info.extensions = cl_splice_extensions(d.getInfo<CL_DEVICE_EXTENSIONS>().c_str());
            return info;
        });
    });
    std::sort(devices_info.begin(), devices_info.end()
             ,[](Device_Info const& a, Device_Info const& b) {
        return std::tie(a.type, a.available, a.hzMax, a.memGlobal, a.memLocal) <
               std::tie(b.type, b.available, b.hzMax, b.memGlobal, b.memLocal);
    });

    if (cmd_args.at("info").asBool()) {
        cl_try("retrieving platform info", [&] {
            auto extensions = cl_splice_extensions(platform.getInfo<CL_PLATFORM_EXTENSIONS>().c_str());
            std::sort(extensions.begin(), extensions.end());

            std::cout << "Platform: "       << platform.getInfo<CL_PLATFORM_NAME>()
                      << "\n  Vendor    : " << platform.getInfo<CL_PLATFORM_VENDOR>()
                      << "\n  Version   : " << platform.getInfo<CL_PLATFORM_VERSION>()
                      << "\n  Profile   : " << platform.getInfo<CL_PLATFORM_PROFILE>()
                      << "\n  Extensions:\n";
            for (auto&& s : extensions)
                std::cout << "    " << s << "\n";
        });

        

        std::cout << "Devices reported (" << devices_info.size() << "):\n";
        for (auto&& d : devices_info) {
            std::cout << "Device: "       << d.name
                      << "\n  Available : " << d.available
                      << "\n  Type      : " << d.type
                      << "\n  Vendor    : " << d.vendor
                      << "\n  Version   : " << d.version
                      << "\n  Profile   : " << d.profile
                      << "\n  Clk Hz Max: " << d.hzMax
                      << "\n  Mem Local : " << display_format_mem(d.memLocal)
                      << "\n  Mem Global: " << display_format_mem(d.memGlobal)
                      << "\n  Extensions:\n";
            for (auto&& s : d.extensions)
                std::cout << "    " << s << "\n";
        }

        return 0;
    }

    auto const deviceSelected = cl_try("selecting device", [&] {
        for (auto&& d : devices_info) {
            if (!d.available                            ) continue;
            if (*device_type == CL_DEVICE_TYPE_ALL      ) return d;

            auto const type = d.device.getInfo<CL_DEVICE_TYPE>();
            if ((type & *device_type) == *device_type   ) return d;
        }

        std::cout << "No device satisfying requirements found. Requirements:\n"
                  << "  Device Type: " << cl_device_type_string(*device_type) << "\n";
        exit(-1);
        return Device_Info {}; // UNREACHABLE
    });
    std::cout << "Selected Device: " << deviceSelected.name;

    auto const kernel = cl_try("building kernel...", [&] {
        cl::Program prg(k_kernel_src, false);
        try { prg.build({ deviceSelected.device }); }
        catch (cl::Error& e) {
            std::cerr << e.what() << "\n"
                      << "Build Status : "  << prg.getBuildInfo<CL_PROGRAM_BUILD_STATUS >(deviceSelected.device) << "\n"
                      << "Build Options: "  << prg.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(deviceSelected.device) << "\n"
                      << "Build Log    :\n" << prg.getBuildInfo<CL_PROGRAM_BUILD_LOG    >(deviceSelected.device) << "\n";
            throw e;
        }

        return std::move(prg);
    });
    std::cout << "Compiled kernel.\n";

#if 0

    cl::Program vectorAddProgram(std::string(sourceStr), false);
    try
    {
        vectorAddProgram.build("");
    }
    catch (cl::Error e) {
        std::cout << e.what() << "\n";
        std::cout << "Build Status: " << vectorAddProgram.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(cl::Device::getDefault()) << "\n";
        std::cout << "Build Options:\t" << vectorAddProgram.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(cl::Device::getDefault()) << "\n";
        std::cout << "Build Log:\t " << vectorAddProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault()) << "\n";

        return FAILURE;
    }
    typedef cl::make_kernel<
        cl::Buffer&,
        cl::Buffer&,
        cl::Buffer&
    > KernelType;

    // create kernel as a functor
    KernelType vectorAddKernel(vectorAddProgram, "vectorAdd");
#endif
    return 0;
}

