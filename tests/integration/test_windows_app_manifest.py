"""Check DPI, code page, and privilege declarations in compiled Windows executables."""

import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET


RT_MANIFEST = 24
CREATEPROCESS_MANIFEST_RESOURCE_ID = 1
LOAD_LIBRARY_AS_DATAFILE = 0x00000002
LOAD_LIBRARY_AS_IMAGE_RESOURCE = 0x00000020
NAMESPACES = {
    "asm": "urn:schemas-microsoft-com:asm.v3",
    "dpi2005": "http://schemas.microsoft.com/SMI/2005/WindowsSettings",
    "dpi2016": "http://schemas.microsoft.com/SMI/2016/WindowsSettings",
    "codepage2019": "http://schemas.microsoft.com/SMI/2019/WindowsSettings",
}


def embedded_manifest(path):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FindResourceW.argtypes = [wintypes.HMODULE, wintypes.LPCWSTR, wintypes.LPCWSTR]
    kernel.FindResourceW.restype = wintypes.HANDLE
    kernel.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.LoadResource.restype = wintypes.HANDLE
    kernel.LockResource.argtypes = [wintypes.HANDLE]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    kernel.FreeLibrary.restype = wintypes.BOOL
    # Map resources only: do not execute the application or load its dependencies.
    module = kernel.LoadLibraryExW(str(path), None, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        name = ctypes.cast(ctypes.c_void_p(CREATEPROCESS_MANIFEST_RESOURCE_ID), wintypes.LPCWSTR)
        kind = ctypes.cast(ctypes.c_void_p(RT_MANIFEST), wintypes.LPCWSTR)
        resource = kernel.FindResourceW(module, name, kind)
        if not resource:
            raise ctypes.WinError(ctypes.get_last_error())
        size = kernel.SizeofResource(module, resource)
        loaded = kernel.LoadResource(module, resource)
        address = kernel.LockResource(loaded)
        if not size or not address:
            raise ctypes.WinError(ctypes.get_last_error())
        return ET.fromstring(ctypes.string_at(address, size))
    finally:
        kernel.FreeLibrary(module)


@unittest.skipUnless(sys.platform == "win32", "Windows executable resources")
class WindowsAppManifestTests(unittest.TestCase):
    app_path = None
    launcher_path = None

    def check_manifest(self, path):
        manifest = embedded_manifest(path)
        settings = manifest.find("asm:application/asm:windowsSettings", NAMESPACES)
        self.assertIsNotNone(settings, "Missing embedded Windows settings")
        self.assertEqual(settings.findtext("dpi2016:dpiAwareness", namespaces=NAMESPACES), "PerMonitorV2")
        self.assertEqual(settings.findtext("dpi2005:dpiAware", namespaces=NAMESPACES), "true/PM")
        self.assertEqual(settings.findtext("codepage2019:activeCodePage", namespaces=NAMESPACES), "UTF-8")
        privilege = manifest.find("asm:trustInfo/asm:security/asm:requestedPrivileges/asm:requestedExecutionLevel",
                                  NAMESPACES)
        self.assertIsNotNone(privilege, "Missing explicit standard-user privilege declaration")
        self.assertEqual(privilege.get("level"), "asInvoker")
        self.assertEqual(privilege.get("uiAccess"), "false")

    def test_application_embeds_per_monitor_v2_without_elevation(self):
        if self.app_path is None:
            self.skipTest("Pass --app to inspect a compiled application.")
        self.check_manifest(self.app_path)

    def test_launcher_embeds_per_monitor_v2_without_elevation(self):
        if self.launcher_path is None:
            self.skipTest("No launcher in this build variant.")
        self.check_manifest(self.launcher_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--launcher", type=Path)
    arguments, test_arguments = parser.parse_known_args()
    WindowsAppManifestTests.app_path = arguments.app.resolve()
    WindowsAppManifestTests.launcher_path = arguments.launcher.resolve() if arguments.launcher else None
    unittest.main(argv=[sys.argv[0], *test_arguments])
