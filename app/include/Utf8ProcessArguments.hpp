#pragma once
#include <string>
#include <vector>

/** @brief Own UTF-8 process arguments, reading the wide Windows command line before CRT data is lost. */
class Utf8ProcessArguments {
public:
    /** @brief Capture arguments for the process lifetime. @param argc CRT argument count. @param argv CRT arguments on POSIX. */
    Utf8ProcessArguments(int argc, char** argv);
    Utf8ProcessArguments(const Utf8ProcessArguments&) = delete;
    Utf8ProcessArguments& operator=(const Utf8ProcessArguments&) = delete;
    /** @brief Return the argument count, excluding the null sentinel. */
    int count() const noexcept;
    /** @brief Return mutable argv pointers for Qt; strings stay owned by this object. */
    char** data() noexcept;
private:
    std::vector<std::string> arguments_;
    std::vector<char*> pointers_;
};
