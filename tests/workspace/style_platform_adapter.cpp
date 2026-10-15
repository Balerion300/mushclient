// Test-only platform responses. No OS settings are changed. The production
// implementation is included unchanged so pressed high-contrast rendering and
// borrowed-font lifetime can be exercised deterministically in a hidden fixture.
#include "stdafx.h"
bool StyleFixtureHighContrast=false,StyleFixtureLargeFont=false;
BOOL WINAPI StyleFixtureParametersA(UINT action,UINT parameter,PVOID value,UINT flags) {
  const BOOL result=SystemParametersInfoA(action,parameter,value,flags);
  if(result && action==SPI_GETHIGHCONTRAST && StyleFixtureHighContrast)
    static_cast<HIGHCONTRASTA*>(value)->dwFlags|=HCF_HIGHCONTRASTON;
  return result;
}
BOOL WINAPI StyleFixtureParametersW(UINT action,UINT parameter,PVOID value,UINT flags) {
  const BOOL result=SystemParametersInfoW(action,parameter,value,flags);
  if(result && action==SPI_GETNONCLIENTMETRICS && StyleFixtureLargeFont)
    static_cast<NONCLIENTMETRICSW*>(value)->lfMessageFont.lfHeight-=4;
  return result;
}
FARPROC WINAPI StyleFixtureProc(HMODULE module,LPCSTR name) {
  if(!strcmp(name,"SystemParametersInfoForDpi"))return NULL;
  return GetProcAddress(module,name);
}
#undef SystemParametersInfo
#define SystemParametersInfo StyleFixtureParametersA
#define SystemParametersInfoW StyleFixtureParametersW
#define GetProcAddress StyleFixtureProc
#include "style-production.inc"
