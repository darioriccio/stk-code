#!/usr/bin/env python3
"""Supplemental source contracts for isolated native mouse-debug startup.

These checks do not compile or run STK and do not prove filesystem behaviour.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
main = (ROOT / "src/main.cpp").read_text()
mouse = (ROOT / "src/modes/immersive_relay/mouse_debug.cpp").read_text()
files = (ROOT / "src/io/file_manager.cpp").read_text()

assert main.index("TholusMouseDebug::readEarlyCommandLine();") < main.index("initUserConfig();")
early = mouse.split("void readEarlyCommandLine()", 1)[1].split("void applyWindowSettings()", 1)[0]
assert early.index("if (!enabled)") < early.index('std::getenv("THOLUS_MOUSE_DEBUG_DATA_ROOT")')
for validation in ["realpath(root, canonical)", "std::string(root) != canonical", "lstat(root, &attributes)",
                   "attributes.st_uid != getuid()", "(attributes.st_mode & 077) != 0"]:
    assert validation in early, validation
assert "throw std::runtime_error" in early

directories = {
    "Config": "m_user_config_dir", "Addons": "m_addons_dir", "Screenshot": "m_screenshot_dir",
    "Replay": "m_replay_dir", "CachedTextures": "m_cached_textures_dir", "GP": "m_gp_dir",
}
for kind, variable in directories.items():
    implementation = files.split(f"void FileManager::checkAndCreate{kind}Dir()", 1)[1]
    opening = implementation.index("{")
    first_statement = implementation[opening + 1:].lstrip()
    assert first_statement.startswith(f"if (useMouseDebugDirectory({variable},"), kind

helper = files.split("bool FileManager::useMouseDebugDirectory(", 1)[1].split("void FileManager::checkAndCreateConfigDir()", 1)[0]
for validation in ["TholusMouseDebug::dataRoot()", "path != canonical", "!S_ISDIR(attributes.st_mode)",
                   "throw std::runtime_error"]:
    assert validation in helper, validation
config = files.split("void FileManager::checkAndCreateConfigDir()", 1)[1]
assert config.index('m_stdout_filename = "stdout.log"') < config.index('getenv("SUPERTUXKART_SAVEDIR")')
print("Mouse debug source isolation contracts passed (source evidence only).")
