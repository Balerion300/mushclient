#include "WorkspaceInputEncoding.h"
#include <iostream>
#include <cstdlib>
namespace {
void Check(bool ok,const char* description){if(!ok){std::cerr<<"FAIL "<<description<<'\n';std::exit(1);}}
}
int main()
{
  std::string encoded;
  const std::string unicode="page Test=" "\xce\xbb\xf0\x9f\x90\x89";
  Check(workspace_input::EncodeForWorld(true,1252,unicode,encoded)&&encoded==unicode,"UTF-8 world preserves Greek and emoji exactly");
  Check(workspace_input::EncodeForWorld(false,65001,unicode,encoded)&&encoded==unicode,"UTF-8 Windows system locale works for legacy-configured world");
  Check(workspace_input::EncodeForWorld(false,1252,"say \xc2\xa9\xe2\x82\xac",encoded)&&encoded==std::string("say \xa9\x80"),"legacy Windows-1252 exact conversion");
  encoded="previous";
  Check(!workspace_input::EncodeForWorld(false,1252,unicode,encoded)&&encoded.empty(),"unrepresentable command rejected instead of substituting characters");
  Check(!workspace_input::EncodeForWorld(true,1252,"\xc0\xaf",encoded)&&encoded.empty(),"overlong UTF-8 rejected");
  Check(!workspace_input::EncodeForWorld(true,1252,"\xed\xa0\x80",encoded)&&encoded.empty(),"UTF-8 encoded surrogate rejected");
  Check(!workspace_input::EncodeForWorld(true,1252,"\xf0\x9f",encoded)&&encoded.empty(),"truncated UTF-8 rejected");
  Check(!workspace_input::EncodeForWorld(false,99999,"say test",encoded)&&encoded.empty(),"invalid legacy codepage rejected");
  std::cout<<"PASS human/editor command encoding: exact UTF-8, UTF-8 system locale, Windows-1252 and no lossy substitutions\n";
}
