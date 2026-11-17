/* Struct offsets that the assembler VM of LuaJIT uses.
** buildvm reads them with the host compiler. The lib uses the Vita compiler.
** tools/check_layout.sh builds this file with both and compares the values.
*/
#include "lj_obj.h"
#include "lj_jit.h"
#include "lj_dispatch.h"
#include <stddef.h>
#include <stdio.h>

#define LS(x) { "L." #x, (int)offsetof(lua_State, x) }
#define GS(x) { "G." #x, (int)offsetof(global_State, x) }
#define JS(x) { "J." #x, (int)offsetof(jit_State, x) }
#define TR(x) { "T." #x, (int)offsetof(GCtrace, x) }

typedef struct Entry { const char *name; int value; } Entry;

const Entry layout[] = {
  { "sizeof lua_State", (int)sizeof(lua_State) },
  { "sizeof global_State", (int)sizeof(global_State) },
  { "sizeof jit_State", (int)sizeof(jit_State) },
  { "sizeof GCtrace", (int)sizeof(GCtrace) },
  { "sizeof GCproto", (int)sizeof(GCproto) },
  { "sizeof GCfunc", (int)sizeof(GCfunc) },
  { "sizeof GCtab", (int)sizeof(GCtab) },
  { "sizeof FoldState", (int)sizeof(FoldState) },
  { "sizeof GG_State", (int)sizeof(GG_State) },
  { "GG.g", (int)offsetof(GG_State, g) },
  { "GG.J", (int)offsetof(GG_State, J) },
  { "GG.hotcount", (int)offsetof(GG_State, hotcount) },
  { "GG.dispatch", (int)offsetof(GG_State, dispatch) },
  { "DISPATCH_J(trace)", (int)(GG_DISP2J + offsetof(jit_State, trace)) },
  LS(base), LS(top), LS(maxstack), LS(stack), LS(cframe),
  GS(gc), GS(tmpbuf), GS(hookmask), GS(dispatchmode), GS(vmstate), GS(cur_L), GS(jit_base),
  GS(tmptv), GS(nilnode), GS(strempty),
  TR(nins), TR(nk), TR(snap), TR(szmcode), TR(mcode),
  JS(cur), JS(curfinal), JS(L), JS(pc), JS(flags), JS(fold), JS(bc_min), JS(state), JS(retdepth),
  JS(ksimd), JS(irbuf), JS(irtoplim), JS(sizesnap), JS(snapbuf), JS(postproc), JS(retryrec),
  JS(trace), JS(freetrace), JS(chain), JS(slot), JS(param), JS(exitstubgroup), JS(penalty),
  JS(bpropcache), JS(scev), JS(startpc), JS(parent), JS(exitno), JS(exitcode), JS(patchpc),
  JS(mcprot), JS(mcarea), JS(errinfo),
};

#ifdef LAYOUT_MAIN
int main(void)
{
  for (unsigned i = 0; i < sizeof layout / sizeof layout[0]; i++)
    printf("%s %d\n", layout[i].name, layout[i].value);
  return 0;
}
#endif
