#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include "nlohmann/json.hpp"

namespace mmd {
struct RigPresetHistory {
  std::string file;
  bool requiresPmx = false;
};
inline RigPresetHistory ReadRigPresetHistory(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) return {};
  if (std::filesystem::file_size(path) > 128 * 1024)
    throw std::runtime_error(u8"适配预设记录过大");
  std::ifstream stream(path, std::ios::binary);
  nlohmann::json j; stream >> j;
  if (j.at("version").get<int>() != 1)
    throw std::runtime_error(u8"不支持的适配预设记录");
  RigPresetHistory record{j.at("file").get<std::string>(),j.value("requires_pmx",false)};
  if (record.file.empty() || record.file.find('\0') != record.file.npos ||
      !std::filesystem::u8path(record.file).is_absolute())
    throw std::runtime_error(u8"适配预设路径无效");
  return record;
}
inline void WriteRigPresetHistory(const std::filesystem::path &path,
                                  const std::filesystem::path &preset, bool requiresPmx) {
  const auto file=std::filesystem::absolute(preset).lexically_normal();
  const auto destination=std::filesystem::absolute(path).lexically_normal();
  if(_wcsicmp(file.c_str(),destination.c_str())==0)
    throw std::runtime_error(u8"请将预设另存为其他文件名，不能覆盖内部适配记录");
  std::filesystem::create_directories(path.parent_path());
  auto temporary=path;temporary+=L".tmp";
  {
    std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
    stream << nlohmann::json{{"version",1},{"file",file.u8string()},{"requires_pmx",requiresPmx}}.dump(2);
    stream.flush();
    if(!stream)throw std::runtime_error(u8"无法记录上次使用的适配预设");
  }
  if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error(u8"无法替换适配预设记录");
}
} // namespace mmd
