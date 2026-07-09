# Summary

These commits substantially upgraded Linux packaging and runtime deployment. The app gained RPM packaging, backend-aware runtime bundling, stronger Linux llama build detection, Fedora-aware Qt tool discovery, version parsing from the app header, CPU fallback that can reuse staged Vulkan payloads, and a safer path for Linux CUDA builds that rely on `libcuda` stubs.

# Motivation

Linux support was growing beyond "build it locally and hope the machine is already prepared." Packaging and runtime staging needed to understand which precompiled payloads were present, how different distros expose Qt tools, and how CUDA-capable builds should behave on systems where only the stub libraries are available during packaging.

# Implementation

The biggest change in this chapter was the introduction of `create_rpm.sh` alongside more capable Debian packaging logic and launcher/runtime scripts. The Linux build helpers also became more defensive about:

- finding Qt 6 tools on Fedora-style layouts
- choosing a CUDA-capable host compiler
- staging backend-specific runtime payloads
- reusing Vulkan payloads when a dedicated CPU payload is absent
- deriving the app version from the authoritative header instead of hard-coded script values

# Validation

Validation was a mix of packaging-script smoke checks and updated documentation:

- `app/scripts/create_rpm.sh`
- updated Debian packaging flow
- `build_llama_linux.sh` compatibility changes
- README updates for Linux build and packaging instructions

Because this work targeted multiple Linux environments, the validation was necessarily distribution-aware rather than a single universal smoke test.

# User-visible impact

Linux users gained more reliable packaged builds and fewer runtime surprises when backend payloads differ between CPU, CUDA, and Vulkan deployments. Release builders also got a clearer path for producing backend-aware packages instead of manually stitching runtime pieces together.

# Remaining caveats

Linux packaging is still distro-sensitive. These changes reduced the number of implicit assumptions, but they did not eliminate all differences in compiler, loader, and package-manager environments.
