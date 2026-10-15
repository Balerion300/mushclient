#pragma once
#include <windows.h>
#include <string>

// Display metadata only. Neither value is used as a connection/send identity.
bool ShowCharacterLabelsDialog(HWND owner,std::wstring& game,std::wstring& character);
