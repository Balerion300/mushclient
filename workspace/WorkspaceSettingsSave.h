#pragma once
#include "../sqlite3/sqlite3.h"

// A failed row or commit leaves the previous complete settings snapshot intact.
namespace workspace_settings {
class Save {
  sqlite3* db_;bool active_;
public:
  explicit Save(sqlite3* db):db_(db),active_(db && sqlite3_exec(db,"SAVEPOINT workspace_settings_save",NULL,NULL,NULL)==SQLITE_OK){}
  ~Save(){Rollback();}
  void Rollback(){if(active_){sqlite3_exec(db_,"ROLLBACK TO workspace_settings_save; RELEASE workspace_settings_save",NULL,NULL,NULL);active_=false;}}
  bool Ready()const{return active_;}
  bool Commit(){if(!active_ || sqlite3_exec(db_,"RELEASE workspace_settings_save",NULL,NULL,NULL)!=SQLITE_OK)return false;active_=false;return true;}
  Save(const Save&)=delete;Save& operator=(const Save&)=delete;
};
}
