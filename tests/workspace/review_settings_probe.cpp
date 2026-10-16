#include "stdafx.h"
#include "WorkspacePane.h"
#include "workspace/WorkspaceSettingsSave.h"
#include <fstream>
#undef App
#undef Frame
struct FixtureApplication {sqlite3* db=NULL;} App;
static int feedback=0;
int FixtureMessageBox(LPCTSTR,UINT=MB_OK,UINT=0){++feedback;return IDOK;}
#define AfxMessageBox FixtureMessageBox
CString WorkspacePersistentWorld(const CString& id){return id;}
std::string WorkspaceLayoutUtf8(const std::wstring& text){int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);std::string out(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&out[0],n,NULL,NULL);return out;}
bool WorkspaceRoute::Compile(CString& error){return !connectionId.IsEmpty()&&!transcriptName.IsEmpty()&&!pattern.empty();} // Parser is covered by route probes; this isolates production persistence.
struct FixturePane {
 std::map<std::pair<CString,CString>,std::wstring> prefixes;std::vector<WorkspacePaneLayout> layouts;
 bool SetInputPrefix(const CString& id,const CString& name,const std::wstring& prefix){prefixes[{id,name}]=prefix;return true;}
 void GetLayouts(std::vector<WorkspacePaneLayout>& target,bool){target.insert(target.end(),layouts.begin(),layouts.end());}
};
struct FixtureSession {std::string forest="original-forest";std::string Save(int){return forest;}};
class CMainFrame {
public:
 FixturePane m_workspacePane;std::vector<FixturePane*> m_workspaceSplits,m_workspaceTools;std::vector<WorkspaceRoute> m_workspaceRoutes;
 FixtureSession session;FixtureSession* m_workspaceDockSession=&session;
 void LoadWorkspaceDockState(){}int WorkspaceDockEntries(){return 0;}void SetStatusMessage(const char*){}void ClearWorkspaceImageAssociation(const CString&){}
 bool OpenWorkspaceTranscript(const CString&,const CString&){return true;}
 bool SaveWorkspaceRoute(const WorkspaceRoute*,const WorkspaceRoute&,const std::wstring&,CString&);
 bool ToggleWorkspaceChannelMain(const CString&,const CString&,CString&);
 void SaveWorkspaceLayouts();
};
#include "review-settings-production.inc"
static std::string Read(const char* sql){sqlite3_stmt* q=NULL;sqlite3_prepare_v2(App.db,sql,-1,&q,NULL);std::string out;if(sqlite3_step(q)==SQLITE_ROW){const char* value=(const char*)sqlite3_column_text(q,0);if(value)out=value;}sqlite3_finalize(q);return out;}
int main(){std::ofstream out("review-settings-results.txt");out.setf(std::ios::unitbuf);bool good=true;auto check=[&](const char* name,bool pass){out<<(pass?"PASS ":"FAIL ")<<name<<'\n';good=pass&&good;};
 check("fresh in-memory preferences open",sqlite3_open(":memory:",&App.db)==SQLITE_OK && WorkspaceLayoutSchema() && WorkspaceSpawnSchema());
 CMainFrame frame;WorkspaceRoute original;original.connectionId="fictional";original.transcriptName="Chat";original.pattern=L"before";original.followingImages=true;CString error;
 check("initial rule prefix and image option save together",frame.SaveWorkspaceRoute(NULL,original,L"+before ",error)&&Read("SELECT prefix FROM workspace_input_prefix_v1")=="+before ");
 WorkspaceRoute changed=original;changed.pattern=L"after";changed.omitFromMain=true;
 sqlite3_exec(App.db,"CREATE TRIGGER reject_prefix BEFORE INSERT ON workspace_input_prefix_v1 BEGIN SELECT RAISE(ABORT,'fixture prefix failure'); END",NULL,NULL,NULL);
 check("prefix failure rejects the complete rule edit",!frame.SaveWorkspaceRoute(&original,changed,L"+after ",error));
 check("failed save keeps persisted rule prefix image and live route unchanged",Read("SELECT pattern FROM workspace_route_v1")=="before" && Read("SELECT prefix FROM workspace_input_prefix_v1")=="+before " && frame.m_workspaceRoutes.size()==1 && frame.m_workspaceRoutes[0].pattern==L"before" && frame.m_workspacePane.prefixes[{CString("fictional"),CString("Chat")}]==L"+before ");
 sqlite3_exec(App.db,"DROP TRIGGER reject_prefix;CREATE TRIGGER reject_delete BEFORE DELETE ON workspace_route_images_v1 BEGIN SELECT RAISE(ABORT,'fixture delete failure'); END",NULL,NULL,NULL);
 check("late old-rule deletion failure rolls back replacement and prefix",!frame.SaveWorkspaceRoute(&original,changed,L"+after ",error)&&Read("SELECT pattern FROM workspace_route_v1")=="before"&&Read("SELECT prefix FROM workspace_input_prefix_v1")=="+before ");
 sqlite3_exec(App.db,"DROP TRIGGER reject_delete",NULL,NULL,NULL);
 check("successful replacement atomically switches persisted and live settings",frame.SaveWorkspaceRoute(&original,changed,L"+after ",error)&&Read("SELECT pattern FROM workspace_route_v1")=="after"&&Read("SELECT prefix FROM workspace_input_prefix_v1")=="+after "&&frame.m_workspaceRoutes.size()==1&&frame.m_workspaceRoutes[0].pattern==L"after"&&frame.m_workspacePane.prefixes[{CString("fictional"),CString("Chat")}]==L"+after ");
 WorkspaceRoute second=changed;second.pattern=L"second";second.omitFromMain=false;
 check("second channel rule creates mixed routing",frame.SaveWorkspaceRoute(NULL,second,L"+after ",error));
 check("mixed channel toggle mirrors every matching rule",frame.ToggleWorkspaceChannelMain("fictional","Chat",error)&&Read("SELECT count(*) FROM workspace_route_v1 WHERE omit_main<>0")=="0" && std::all_of(frame.m_workspaceRoutes.begin(),frame.m_workspaceRoutes.end(),[](const WorkspaceRoute& r){return !r.omitFromMain;}));
 sqlite3_exec(App.db,"CREATE TRIGGER reject_toggle BEFORE UPDATE ON workspace_route_v1 BEGIN SELECT RAISE(ABORT,'fixture toggle failure'); END",NULL,NULL,NULL);
 check("toggle failure keeps saved and live routing unchanged",!frame.ToggleWorkspaceChannelMain("fictional","Chat",error)&&Read("SELECT count(*) FROM workspace_route_v1 WHERE omit_main<>0")=="0" && !frame.m_workspaceRoutes[0].omitFromMain);
 sqlite3_exec(App.db,"DROP TRIGGER reject_toggle",NULL,NULL,NULL);
 check("mirrored channel toggle separates all rules",frame.ToggleWorkspaceChannelMain("fictional","Chat",error)&&Read("SELECT count(*) FROM workspace_route_v1 WHERE omit_main=1")=="2" && frame.m_workspaceRoutes[0].omitFromMain && frame.m_workspaceRoutes[1].omitFromMain);
 check("missing channel cannot toggle unrelated routes",!frame.ToggleWorkspaceChannelMain("fictional","Missing",error)&&Read("SELECT count(*) FROM workspace_route_v1 WHERE omit_main=1")=="2");
 WorkspacePaneLayout layout;layout.connectionId="fictional";layout.name="Chat";layout.width=400;layout.height=300;frame.m_workspacePane.layouts.push_back(layout);frame.SaveWorkspaceLayouts();
 check("complete initial layout saves without error feedback",feedback==0&&Read("SELECT data FROM workspace_dock_forest_v3")=="original-forest"&&Read("SELECT width FROM workspace_pane_layout_v1")=="400");
 frame.session.forest="changed-forest";frame.m_workspacePane.layouts[0].width=900;
 sqlite3_exec(App.db,"CREATE TRIGGER reject_split BEFORE INSERT ON workspace_split_v2 BEGIN SELECT RAISE(ABORT,'fixture late layout failure'); END",NULL,NULL,NULL);frame.SaveWorkspaceLayouts();
 check("late layout failure keeps the complete previous forest and geometry",feedback==1&&Read("SELECT data FROM workspace_dock_forest_v3")=="original-forest"&&Read("SELECT width FROM workspace_pane_layout_v1")=="400");
 check("failed layout transaction releases its database savepoint",sqlite3_get_autocommit(App.db)!=0);
 sqlite3_exec(App.db,"DROP TRIGGER reject_split",NULL,NULL,NULL);frame.SaveWorkspaceLayouts();
 check("layout can be retried successfully after a rejected save",feedback==1&&Read("SELECT data FROM workspace_dock_forest_v3")=="changed-forest"&&Read("SELECT width FROM workspace_pane_layout_v1")=="900");
 sqlite3_close(App.db);out<<(good?"PASS":"FAIL")<<" extracted production atomic route and layout persistence\n";return good?0:1;}
