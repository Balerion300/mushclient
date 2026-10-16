#pragma once
#include <string>
#include <cstdint>

namespace workspace_spawn {
struct Appearance {
  std::wstring face=L"Consolas";
  int pointSize=10, weight=400, charset=0;
  uint32_t foreground=0x00e1e1e1, background=0x001c1919;
  bool operator==(const Appearance& other) const {
    return face==other.face && pointSize==other.pointSize && weight==other.weight && charset==other.charset && foreground==other.foreground && background==other.background;
  }
  bool operator!=(const Appearance& other) const {return !(*this==other);}
};
inline bool ValidAppearance(const Appearance& value) {
  if(value.face.empty() || value.face.size()>31 || value.pointSize<1 || value.pointSize>1000 || value.weight<0 || value.weight>1000 || value.charset<0 || value.charset>255 || value.foreground>0xffffff || value.background>0xffffff)return false;
  for(auto ch:value.face)if(ch<32 || ch==127 || ch==L';' || (ch>=0xd800 && ch<=0xdfff))return false;
  return true;
}
}
