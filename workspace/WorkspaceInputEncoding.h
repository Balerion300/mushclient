#pragma once
#include <string>
namespace workspace_input {
// Explicit legacy codepage makes Windows UTF-8 system locales testable too.
// Never substitute or best-fit characters in a command destined for the game.
bool EncodeForWorld(bool utf8World,unsigned legacyCodepage,
                    const std::string& utf8,std::string& encoded);
}
