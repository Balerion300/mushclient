#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <limits>
#include "WorkspaceInputEncoding.h"
namespace workspace_input {
bool EncodeForWorld(bool utf8World,unsigned legacyCodepage,
                    const std::string& source,std::string& encoded)
{
  encoded.clear();
  if(source.empty() || source.size()>static_cast<size_t>((std::numeric_limits<int>::max)()))return false;
  int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),(int)source.size(),NULL,0);
  if(!length)return false;
  if(utf8World || legacyCodepage==CP_UTF8){encoded=source;return true;}
  std::wstring text(length,L'\0');
  if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),(int)source.size(),&text[0],length))return false;
  BOOL substituted=FALSE;
  int bytes=WideCharToMultiByte(legacyCodepage,WC_NO_BEST_FIT_CHARS,text.data(),length,NULL,0,NULL,&substituted);
  if(!bytes || substituted)return false;
  std::string result(bytes,'\0');
  if(!WideCharToMultiByte(legacyCodepage,WC_NO_BEST_FIT_CHARS,text.data(),length,&result[0],bytes,NULL,&substituted) || substituted)return false;
  encoded.swap(result);return true;
}
}
