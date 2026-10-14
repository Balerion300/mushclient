#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
extern int luaopen_rex(lua_State *L);
int main(void)
{
  lua_State *L=luaL_newstate();int result;
  if(!L)return 2;
  luaL_openlibs(L);luaopen_rex(L);lua_pop(L,1);
  result=luaL_dofile(L,"tests/scripted_logging/helper_tests.lua");
  if(result)fprintf(stderr,"%s\n",lua_tostring(L,-1));
  lua_close(L);return result?1:0;
}
