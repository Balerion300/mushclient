#pragma once
#include "WorkspaceSpell.h"
class CMUSHclientDoc;
namespace workspace_spell {
Context ForDocument(CMUSHclientDoc* doc);
Context ForConnection(const CString& connection);
}
