#include "Utf8ProcessArguments.hpp"
#include "Utils.hpp"
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

Utf8ProcessArguments::Utf8ProcessArguments(int argc, char** argv)
{
#ifdef _WIN32
    int wide_argc = 0;
    wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (!wide_argv) throw std::runtime_error("Failed to read the Windows command line");
    const std::unique_ptr<wchar_t*, decltype(&LocalFree)> owned(wide_argv, LocalFree);
    arguments_.reserve(wide_argc);
    for (int index = 0; index < wide_argc; ++index) {
        arguments_.push_back(Utils::path_to_utf8(std::filesystem::path(wide_argv[index])));
    }
#else
    arguments_.reserve(argc);
    for (int index = 0; index < argc; ++index) arguments_.emplace_back(argv[index]);
#endif
    pointers_.reserve(arguments_.size() + 1);
    for (auto& argument : arguments_) pointers_.push_back(argument.data());
    pointers_.push_back(nullptr);
}

int Utf8ProcessArguments::count() const noexcept { return static_cast<int>(arguments_.size()); }
char** Utf8ProcessArguments::data() noexcept { return pointers_.data(); }
