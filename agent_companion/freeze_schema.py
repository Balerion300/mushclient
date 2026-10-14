import json
from pathlib import Path
from companion import TOOLS
root=Path(__file__).resolve().parents[1]
root.joinpath('AgentToolSchemas.h').write_text('// Independently frozen schemas; generated from companion.py\n#pragma once\nstatic const char kAgentToolSchemas[] = R"schemas('+json.dumps(TOOLS,separators=(',',':'))+')schemas";\n',encoding='utf-8')
root.joinpath('agent_companion/tools.json').write_text(json.dumps(TOOLS,indent=2)+'\n',encoding='utf-8')
