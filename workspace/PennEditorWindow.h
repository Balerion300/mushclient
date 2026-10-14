#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <windows.h>
// The editor owns local unsent drafts. It has no network or game-send
// capability.
HWND OpenPennEditor(HWND owner, const std::string &endpointIdentity,
                    const std::string &initialUtf8 = "",
                    const std::string &destinationNameUtf8 = "");
// Optional before opening windows; default is exe-relative editor-state.
bool PennEditorConfigureStorage(const std::wstring &folder);
bool PennEditorSnapshot(HWND window, std::string &utf8, uint64_t &revision,
                        std::string &endpoint);
bool PennEditorApplyProposal(HWND window, uint64_t revision, size_t fromByte,
                             size_t toByte, const std::string &replacement);
struct PennEditorInfo {
  HWND window = 0;
  uint64_t documentID = 0, revision = 0;
  std::string endpoint, source;
};
bool PennEditorSnapshotInfo(HWND window, PennEditorInfo &info);
std::vector<PennEditorInfo> PennEditorListSnapshots();
bool PennEditorApplyDocumentProposal(uint64_t documentID, uint64_t revision,
                                     const std::string &replacement);
// Human UI only: its full scrollable diff and explicit Accept button supply
// review; validator runs immediately before the local edit. No game dispatch.
bool PennEditorApplyReviewedDocumentProposal(
    uint64_t documentID, uint64_t revision, const std::string &replacement,
    const std::function<bool()> &validator);
void PennEditorSetTransport(
    std::function<bool(const std::string &, const std::string &)> transport);
void PennEditorSetSendPrepared(
    std::function<bool(const std::string &, uint64_t documentID,
                       uint64_t revision,
                       const std::vector<std::string> &commands)>
        sender);
// Consume correlated private protocol replies before shared output publication,
// including stale/replayed/malformed replies that cannot change a draft.
bool PennEditorHandleOutput(const std::string &endpoint,
                            const std::string &utf8LogicalLine);
void PennEditorInvalidateEndpoint(const std::string &endpoint);
bool PennEditorSaveRecoveryFile(const std::wstring &path);
bool PennEditorRestoreRecoveryFile(HWND window, const std::wstring &path);
