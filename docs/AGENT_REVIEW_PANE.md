# Native human review pane

`workspace/AgentReviewPane.h` exports `HWND OpenAgentReviewPane(HWND parent)`.
Compile the new `AgentReviewPane.cpp` without precompiled headers as C++17 and
link the existing agent core and native editor modules. It references the existing
`WorkspaceAgentStore()` function without importing MFC GUI headers. The owning
UI thread must also own the Store and editor calls; bridge workers must marshal
requests to that thread. Root integration supplies its launcher/docking host.

The pane is a singleton native window with responsive sizing. The human chooses
an agent name and a category: individual editor tabs, explicitly granted completed
output, editor proposals, tasks, debug captures or unsent drafts. It never starts
agents or creates output grants; use the separate Agent access interface first.
Editor listing is human-visible inventory only. Source enters Store only after
pressing Share selected editor, requires its original destination among that
agent's output grants, and is bounded to 32 KiB. Unshare revokes that agent's
document grant. The 500 ms poll updates actual source/revision for registered
editors and expires closed/disconnected/oversize documents. Closing this review
pane removes its registered editor references and their pending proposals so
sharing cannot silently continue with a stopped synchronizer.

Proposal selection shows complete current/proposed source in two independent,
scrollable, read-only native views. Accept is the human review action; it does not
open a second confirmation. It rechecks actual document identity/revision and
current proposal/session/grant immediately before applying local text through
`PennEditorApplyReviewedDocumentProposal`. That internal API is for human UI;
agent tools cannot call it. Store acceptance follows synchronously with no UI
message pumping, then receives the actual new native revision. Unexpected Store
failure triggers a guarded restoration only if the actual source still matches
the proposal. Stale/revoked/closed proposals cannot apply, and rejection preserves
source. No review-pane path sends game commands.

For selected-text context, choose an explicitly shared output window or capture,
preview its bounded completed records, highlight the exact text in the left
read-only view, then press Share selected text. The original preview session must
still match. UTF-16 selection boundaries cannot split a surrogate pair. Only that
selected text is attached as immutable context with provenance; neither the game
command-input control nor other agents' private context is read.

Start capture acts only on the chosen shared output and records future completed
records; the model enforces eight captures, 10,000 records and 1 MiB per capture.
Captured previews expose source indices and dropped/gap counts. Stop preserves
existing bounded records for reading, while revocation/disconnect expire capture
access. Task lists show current status, cooperative cancellation request and a
stale-heartbeat label after 30 seconds. Cancellation requests do not prove the
external runner stopped. Agent drafts open in destination-bound unsent native
editor documents after current grant checks; this never transmits them.

`workspace/AgentReviewPaneSmoke.cpp` links against a standalone fixture Store and
real Scintilla window. It covers explicit document sharing, full-diff acceptance,
stale rejection after native typing, explicit rejection, completed-output-only
selected context, capture start/read/stop, stale-heartbeat task cancellation,
unsharing and disconnect expiration. A mocked prepared-send callback stays at
zero through all operations. No game account, network endpoint or model is used.
Integrated docking, screenshot/layout, keyboard and bridge-worker acceptance
remain root validation responsibilities.
