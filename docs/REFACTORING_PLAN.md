# Refatoração Modular do Canary's Map Editor (RME)  
  
## Contexto  
  
- **Repositório:** vllsystems/remeres-map-editor (branch: main)  
- **Linguagem:** C++23, CMake 3.22+, vcpkg  
- **Dependências:** wxWidgets, OpenGL 3.3, GLAD, spdlog, protobuf, asio, sol2/lua, cpr, pugixml, liblzma, nlohmann_json, fmt, FastNoiseLite, stb_truetype, ZLIB  
- **Build:** Unity build habilitado por padrão (`SPEED_UP_BUILD_UNITY` em `source/CMakeLists.txt` linha 39). `gl_renderer.cpp` é excluído do unity build (linha 120). Quando unity build está desligado, `main.h` é usado como PCH (linha 124).  
- **PCH / Mega-header:** `source/main.h` inclui wx, STL, pugixml, spdlog, fmt, asio, nlohmann_json, e headers internos (`con_vector.h`, `common.h`, `threads.h`, `const.h`, `rme_forward_declarations.h`). Quase todos os `.cpp` fazem `#include "main.h"` como primeira linha.  
- **Include dir:** `${CMAKE_SOURCE_DIR}/source` (CMakeLists.txt linha 265). Includes são relativos a `source/`.  
- **target_sources:** Lista flat de ~130 `.cpp` em `source/CMakeLists.txt` linhas 128-261. Subdiretório `lua/` já usa paths relativos (`lua/lua_engine.cpp`, etc.).  
- **Subdiretórios existentes:** `source/lua/` (17 arquivos) e `source/protobuf/` (com próprio CMakeLists) — NÃO mexer neles.  
  
### Arquivos problemáticos (god objects)  
  
| Arquivo | Linhas header | Linhas impl | Problema |  
|---|---|---|---|  
| `gui.h` / `gui.cpp` | ~560 | ~2055 | GL context + hotkeys + loading bar + brush state + palettes + file ops + minimap + clipboard + dialogs |  
| `tile.h` / `tile.cpp` | ~369 | ~810 | Data struct + queries + mutations + selection + borders + walls + tables + house + zones |  
| `editor.h` / `editor.cpp` | ~197 | ~2169 | Undo/redo + clipboard + map ops + selection + live server/client + draw + backup |  
| `map_drawer.h` / `map_drawer.cpp` | ~267 | ~2411 | Map rendering + grid + luz + seleção + overlays + tooltips + performance stats + creatures |  
| `main_menubar.h` / `main_menubar.cpp` | ~361 | ~2677 | ~170 ActionIDs + ~80 event handlers + menu construction + recent files + search logic |  
| `iomap_otbm.h` / `iomap_otbm.cpp` | ~150 | ~2020 | Header + tiles + items + towns + waypoints + spawns + houses + zones serialization |  
  
### Nota sobre `iomap_otmm.cpp/h`  
Estes arquivos existem no diretório `source/` mas **NÃO estão listados** no `target_sources` do CMakeLists.txt. Provavelmente são dead code. Mover para `io/` mas NÃO adicionar ao CMakeLists. Verificar se são referenciados por algum `#include` antes de mover.  
  
## Objetivo  
  
Reorganizar o projeto em módulos por domínio **SEM alterar funcionalidade** — apenas mover arquivos, ajustar includes, decompor god objects, e adicionar tooling. Cada fase DEVE compilar antes de ir à próxima.  
  
## Regras Gerais  
  
1. **NUNCA** altere lógica de negócio — apenas mova código e ajuste includes.  
2. **SEMPRE** mantenha o projeto compilando após cada passo/fase.  
3. **SEMPRE** atualize `source/CMakeLists.txt` ao mover/criar arquivos.  
4. **NUNCA** crie dependências circulares entre módulos.  
5. Use `#include "modulo/arquivo.h"` com paths relativos a `source/`.  
6. **Máximo 500 linhas por arquivo** (smell detector, não regra absoluta).  
7. **Máximo 50 linhas por função** (idem).  
8. Use forward declarations em headers sempre que possível. Includes pesados ficam no `.cpp`.  
9. Mantenha `source/lua/` e `source/protobuf/` intocados.  
10. **Use wrappers temporários** ao extrair métodos de god objects. Ex: `Tile::borderize()` continua existindo mas chama `TileOperations::borderize(this, map)`. Isso evita atualizar centenas de call sites de uma vez. Wrappers são removidos na Fase 11.  
11. O `main.h` continua sendo o PCH — não altere seu conteúdo. Arquivos `.cpp` continuam fazendo `#include "main.h"` como primeira linha.  
  
## Regras de Dependência entre Módulos  
  
Direção permitida (→ pode depender de):  
```  
app/       → tudo (é o bootstrap)  
ui/        → editor/, game/, map/, rendering/, brushes/, util/  
editor/    → map/, game/, brushes/, util/  
rendering/ → map/, game/, util/ (NUNCA de ui/ ou brushes/)  
brushes/   → map/, game/, util/  
io/        → map/, game/, util/ (NUNCA de ui/ ou rendering/)  
map/       → game/, util/  
game/      → util/  
live/      → editor/, map/, net/, util/  
net/       → util/  
util/      → nada (leaf module)  
```  
  
**PROIBIDO:**  
- `game/` → `editor/`, `ui/`, `rendering/`  
- `map/` → `editor/`, `ui/`, `rendering/`  
- `util/` → qualquer coisa  
- Dependências circulares entre quaisquer módulos  
  
**VIOLAÇÃO TEMPORÁRIA CONHECIDA:** `io/otbm/iomap_otbm.cpp` inclui `gui.h` para usar `g_gui.SetLoadDone()` (loading bar). Isso será corrigido na Fase 6 com o padrão ProgressCallback. Até lá, aceitar a violação.  
  
## Estrutura Alvo  
  
```  
source/  
├── app/                        # Bootstrap e configuração  
│   ├── application.cpp/h  
│   ├── artprovider.cpp/h  
│   ├── definitions.h  
│   ├── main.h  
│   ├── settings.cpp/h  
│   └── updater.cpp/h  
├── brushes/                    # Todos os brushes  
│   ├── brush.cpp/h  
│   ├── brush_enums.h  
│   ├── brush_tables.cpp  
│   ├── carpet_brush.cpp/h  
│   ├── doodad_brush.cpp/h  
│   ├── eraser_brush.cpp  
│   ├── ground_brush.cpp/h  
│   ├── house_brush.cpp/h  
│   ├── house_exit_brush.cpp/h  
│   ├── monster_brush.cpp/h  
│   ├── npc_brush.cpp/h  
│   ├── raw_brush.cpp/h  
│   ├── spawn_monster_brush.cpp/h  
│   ├── spawn_npc_brush.cpp/h  
│   ├── table_brush.cpp/h  
│   ├── wall_brush.cpp/h  
│   ├── waypoint_brush.cpp/h  
│   └── zone_brush.cpp/h  
├── editor/                     # Lógica do editor  
│   ├── action.cpp/h  
│   ├── copybuffer.cpp/h  
│   ├── editor.cpp/h            # Manter intacto por agora (decompor na Fase 10)  
│   ├── editor_tabs.cpp/h  
│   ├── hotkey_manager.cpp/h    # NOVO — extraído de gui.h (Fase 4)  
│   ├── process_com.cpp/h  
│   └── selection.cpp/h  
├── game/                       # Entidades do jogo  
│   ├── complexitem.cpp/h  
│   ├── house.cpp/h  
│   ├── item.cpp/h  
│   ├── item_attributes.cpp/h  
│   ├── items.cpp/h  
│   ├── materials.cpp/h  
│   ├── monster.cpp/h  
│   ├── monsters.cpp/h  
│   ├── npc.cpp/h  
│   ├── npcs.cpp/h  
│   ├── outfit.h  
│   ├── spawn_monster.cpp/h  
│   ├── spawn_npc.cpp/h  
│   ├── tileset.cpp/h  
│   ├── town.cpp/h  
│   ├── waypoints.cpp/h  
│   └── zones.cpp/h  
├── io/                         # I/O e serialização  
│   ├── bitmap_to_map/  
│   │   └── bitmap_to_map_converter.cpp/h  
│   ├── otbm/  
│   │   ├── header_serialization_otbm.cpp/h    # NOVO (Fase 6)  
│   │   ├── item_serialization_otbm.cpp/h      # NOVO (Fase 6)  
│   │   ├── iomap_otbm.cpp/h  
│   │   ├── otbm_types.h                       # NOVO (Fase 6) — enums/structs extraídos de iomap_otbm.h  
│   │   ├── tile_serialization_otbm.cpp/h      # NOVO (Fase 6)  
│   │   ├── town_serialization_otbm.cpp/h      # NOVO (Fase 6)  
│   │   └── waypoint_serialization_otbm.cpp/h  # NOVO (Fase 6)  
│   ├── filehandle.cpp/h  
│   ├── iomap.cpp/h  
│   ├── iomap_otmm.cpp/h       # Dead code — mover mas NÃO adicionar ao CMakeLists  
│   ├── iominimap.cpp/h  
│   └── pngfiles.cpp/h  
├── live/                       # Edição colaborativa  
│   ├── live_action.cpp/h  
│   ├── live_client.cpp/h  
│   ├── live_packets.h  
│   ├── live_peer.cpp/h  
│   ├── live_server.cpp/h  
│   ├── live_socket.cpp/h  
│   └── live_tab.cpp/h  
├── lua/                        # NÃO MEXER — já é módulo  
│   └── (manter como está)  
├── map/                        # Dados do mapa  
│   ├── basemap.cpp/h  
│   ├── map.cpp/h  
│   ├── map_allocator.h  
│   ├── map_overlay.h  
│   ├── map_region.cpp/h  
│   ├── tile.cpp/h  
│   └── tile_operations.cpp/h  # NOVO — extraído de tile.cpp (Fase 5)  
├── net/                        # Rede  
│   ├── net_connection.cpp/h  
│   └── rme_net.cpp/h  
├── protobuf/                   # NÃO MEXER — já é módulo  
│   └── (manter como está)  
├── rendering/                  # Rendering  
│   ├── gl_compat.h  
│   ├── gl_renderer.cpp/h  
│   ├── graphics.cpp/h  
│   ├── light_drawer.cpp/h  
│   ├── map_drawer.cpp/h       # Manter intacto por agora (decompor na Fase 8)  
│   ├── map_display.cpp/h  
│   ├── map_window.cpp/h  
│   ├── sprite_appearances.cpp/h  
│   └── sprites.h  
├── templates/                  # Templates de mapa  
│   ├── templatemap76-74.cpp  
│   ├── templatemap81.cpp  
│   ├── templatemap854.cpp  
│   ├── templatemapclassic.cpp  
│   └── templates.h  
├── ui/                         # Interface  
│   ├── dialogs/  
│   │   ├── about_window.cpp/h  
│   │   ├── common_windows.cpp/h  
│   │   ├── container_properties_window.cpp/h  
│   │   ├── old_properties_window.cpp/h  
│   │   ├── preferences.cpp/h  
│   │   └── properties_window.cpp/h  
│   ├── menubar/  
│   │   └── main_menubar.cpp/h  # Manter intacto por agora (decompor na Fase 7)  
│   ├── palette/  
│   │   ├── palette_brushlist.cpp/h  
│   │   ├── palette_common.cpp/h  
│   │   ├── palette_house.cpp/h  
│   │   ├── palette_monster.cpp/h  
│   │   ├── palette_npc.cpp/h  
│   │   ├── palette_waypoints.cpp/h  
│   │   ├── palette_window.cpp/h  
│   │   └── palette_zones.cpp/h  
│   ├── toolbar/  
│   │   └── main_toolbar.cpp/h  
│   ├── windows/  
│   │   ├── actions_history_window.cpp/h  
│   │   ├── add_item_window.cpp/h  
│   │   ├── add_tileset_window.cpp/h  
│   │   ├── bitmap_to_map_window.cpp/h  
│   │   ├── browse_tile_window.cpp/h  
│   │   ├── dat_debug_view.cpp/h  
│   │   ├── find_item_window.cpp/h  
│   │   ├── minimap_window.cpp/h  
│   │   ├── replace_items_window.cpp/h  
│   │   ├── result_window.cpp/h  
│   │   ├── tileset_window.cpp/h  
│   │   └── welcome_dialog.cpp/h  
│   ├── dcbutton.cpp/h  
│   ├── gui.cpp/h               # Manter intacto por agora (decompor na Fase 9)  
│   ├── gui_ids.h  
│   ├── map_tab.cpp/h  
│   ├── numbertextctrl.cpp/h  
│   └── positionctrl.cpp/h  
├── util/                       # Utilitários (leaf module)  
│   ├── common.cpp/h  
│   ├── con_vector.h  
│   ├── const.h  
│   ├── enums.h  
│   ├── lua_parser.h  
│   ├── mkpch.cpp  
│   ├── mt_rand.cpp/h  
│   ├── otml.h  
│   ├── position.h  
│   ├── rme_forward_declarations.h  
│   ├── stb_truetype_impl.cpp  
│   └── threads.h  
├── client_assets.cpp/h         # Manter na raiz — depende de protobuf + game + io  
└── CMakeLists.txt  
```  
  
---  
  
## Checklist de Validação (executar após CADA fase)  
  
```
[ ] Projeto compila sem erros  
[ ] Projeto compila sem warnings novos  
[ ] Abrir mapa existente (OTBM) — todos os tiles carregam corretamente  
[ ] Salvar mapa — reabrir — conteúdo idêntico  
[ ] Undo/Redo funciona  
[ ] Hotkeys (1-0) funcionam  
[ ] Brushes funcionam (ground, wall, doodad, carpet, table, eraser)  
[ ] Seleção funciona (select, deselect, copy, paste, cut)  
[ ] Borderize automático funciona (pintar ground e verificar bordas)  
[ ] Zoom in/out funciona  
[ ] Scroll funciona  
[ ] Trocar de floor funciona  
[ ] Light rendering funciona (View → Show Lights)  
[ ] Grid funciona (View → Show Grid)  
[ ] Minimap renderiza  
[ ] Live server/client conecta (se aplicável)  
```  
  
### Validação extra para Fase 6 (iomap_otbm):  
```bash  
# Antes da refatoração: abrir mapa X → salvar como mapa_antes.otbm  
# Depois da refatoração: abrir mapa X → salvar como mapa_depois.otbm  
diff mapa_antes.otbm mapa_depois.otbm  
# Se diff = 0 bytes → serialização idêntica, zero regressão  
```  
  
---  
  
## FASES DE IMPLEMENTAÇÃO  
  
---  
  
### Fase 1 — Adicionar `.clang-tidy` (dificuldade: trivial)  
  
Criar `.clang-tidy` na raiz do repositório:  
  
```yaml  
---  
Checks: >  
  -*,  
  bugprone-*,  
  -bugprone-easily-swappable-parameters,  
  -bugprone-narrowing-conversions,  
  misc-include-cleaner,  
  readability-function-size,  
  readability-identifier-naming,  
  performance-unnecessary-copy-initialization,  
  performance-unnecessary-value-param,  
  modernize-use-override,  
  modernize-use-nullptr,  
  modernize-use-auto  
  
CheckOptions:  
  - key: readability-function-size.LineThreshold  
    value: 50  
  - key: readability-function-size.StatementThreshold  
    value: 50  
  
WarningsAsErrors: ''  
  
HeaderFilterRegex: 'source/.*\.h$'  
```  
  
**Validação:** Rodar `clang-tidy` em 1-2 arquivos para verificar que o config é válido. Não precisa corrigir os warnings agora — o objetivo é ter o tooling disponível.  
  
---  
  
### Fase 2 — Adicionar CI de verificação de fronteiras de módulo (dificuldade: trivial)  
  
**Passo 1:** Criar `scripts/check_module_boundaries.sh`:  
  
```bash  
#!/bin/bash  
# Verifica que módulos não incluem headers de módulos proibidos.  
# Retorna exit code 1 se encontrar violações.  
  
set -euo pipefail  
  
ERRORS=0  
  
check_no_include() {  
    local dir="$1"  
    shift  
    local forbidden_patterns=("$@")  
  
    if [ ! -d "$dir" ]; then  
        return  
    fi  
  
    for pattern in "${forbidden_patterns[@]}"; do  
        matches=$(grep -rn "#include.*\"${pattern}" "$dir" --include="*.cpp" --include="*.h" \  
            | grep -v "// KNOWN_VIOLATION" || true)  
        if [ -n "$matches" ]; then  
            echo "VIOLATION: Files in $dir include '$pattern':"  
            echo "$matches"  
            ERRORS=$((ERRORS + 1))  
        fi  
    done  
}  
  
cd "$(dirname "$0")/.."  
  
# game/ não pode depender de editor/, ui/, rendering/  
check_no_include "source/game" "editor/" "ui/" "rendering/"  
  
# map/ não pode depender de editor/, ui/, rendering/  
check_no_include "source/map" "editor/" "ui/" "rendering/"  
  
# util/ não pode depender de nada interno (exceto outros util/)  
check_no_include "source/util" "editor/" "ui/" "rendering/" "game/" "map/" "brushes/" "io/" "live/" "net/" "app/"  
  
# rendering/ não pode depender de ui/ ou brushes/  
check_no_include "source/rendering" "ui/" "brushes/"  
  
# io/ não pode depender de ui/ ou rendering/  
# EXCEÇÃO TEMPORÁRIA: io/otbm/iomap_otbm.cpp inclui gui.h para Loadbar  
# Será corrigido na Fase 6 com ProgressCallback  
check_no_include "source/io" "rendering/"  
IO_UI_VIOLATIONS=$(grep -rn '#include.*"ui/' source/io --include="*.cpp" --include="*.h" \  
    | grep -v "// KNOWN_VIOLATION" \  
    | grep -v "iomap_otbm.cpp" || true)  
if [ -n "$IO_UI_VIOLATIONS" ]; then  
    echo "VIOLATION: Files in source/io include ui/ headers (excluding known iomap_otbm.cpp):"  
    echo "$IO_UI_VIOLATIONS"  
    ERRORS=$((ERRORS + 1))  
fi  
  
if [ "$ERRORS" -gt 0 ]; then  
    echo ""  
    echo "Found $ERRORS module boundary violation(s)."  
    exit 1  
fi  
  
echo "All module boundary checks passed."  
exit 0  
```  
  
**Passo 2:** Tornar executável: `chmod +x scripts/check_module_boundaries.sh`  
  
**Passo 3:** Criar `.github/workflows/module-boundaries.yml`:  
  
```yaml  
name: Module Boundaries  
on: [push, pull_request]  
jobs:  
  check:  
    runs-on: ubuntu-latest  
    steps:  
      - uses: actions/checkout@v4  
      - run: bash scripts/check_module_boundaries.sh  
```  
  
**Validação:** Rodar `bash scripts/check_module_boundaries.sh` localmente — deve passar (nenhum módulo existe ainda, então nenhuma violação).  
  
---  
  
### Fase 3 — Criar estrutura de pastas e mover arquivos (dificuldade: fácil, volume alto)  
  
**REGRA CRÍTICA:** Mover em grupos pequenos. Após cada grupo: atualizar TODOS os `#include` em TODOS os arquivos que referenciam os movidos, atualizar `source/CMakeLists.txt`, compilar e verificar.  
  
**NOTA sobre `source/CMakeLists.txt`:** O `target_include_directories` já aponta para `${CMAKE_SOURCE_DIR}/source`, então includes como `#include "util/common.h"` funcionam automaticamente. Não precisa adicionar novos include dirs.  
  
**NOTA sobre `main.h`:** `main.h` inclui `common.h`, `threads.h`, `const.h`, `con_vector.h`, `rme_forward_declarations.h`. Quando esses arquivos forem movidos para `util/`, atualizar os includes dentro de `main.h` para os novos paths (ex: `#include "util/common.h"`). Como `main.h` é incluído por quase todos os `.cpp`, isso propaga automaticamente.  
  
**NOTA sobre unity build:** Ao mover arquivos, o `SKIP_UNITY_BUILD_INCLUSION` de `gl_renderer.cpp` (CMakeLists linha 120) precisa ser atualizado para o novo path: `set_source_files_properties(rendering/gl_renderer.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)`.  
  
#### Grupo 1 — `util/` (leaf module, zero dependências internas)  
Criar `source/util/` e mover:  
- `common.cpp`, `common.h`  
- `con_vector.h`  
- `const.h`  
- `enums.h`  
- `lua_parser.h`  
- `mkpch.cpp`  
- `mt_rand.cpp`, `mt_rand.h`  
- `otml.h`  
- `position.h`  
- `rme_forward_declarations.h`  
- `stb_truetype_impl.cpp`  
- `threads.h`  
  
Atualizar `main.h` (linhas 158-163):  
```cpp  
#include "util/con_vector.h"  
#include "util/common.h"  
#include "util/threads.h"  
#include "util/const.h"  
#include "util/rme_forward_declarations.h"  
```  
  
Atualizar CMakeLists: trocar `common.cpp` → `util/common.cpp`, `mt_rand.cpp` → `util/mt_rand.cpp`, `mkpch.cpp` → `util/mkpch.cpp`, `stb_truetype_impl.cpp` → `util/stb_truetype_impl.cpp`.  
  
Buscar em TODOS os arquivos por `#include "common.h"`, `#include "position.h"`, `#include "const.h"`, `#include "enums.h"`, `#include "threads.h"`, `#include "con_vector.h"`, `#include "mt_rand.h"`, `#include "otml.h"`, `#include "rme_forward_declarations.h"`, `#include "lua_parser.h"` e atualizar para `#include "util/..."`. **EXCEÇÃO:** Se o include vem via `main.h` (que já foi atualizado), o `.cpp` não precisa mudar — mas headers `.h` que incluem diretamente precisam.  
  
Compilar e verificar.  
  
#### Grupo 2 — `game/` (entidades do jogo)  
Criar `source/game/` e mover:  
- `complexitem.cpp/h`  
- `house.cpp/h`  
- `item.cpp/h`  
- `item_attributes.cpp/h`  
- `items.cpp/h`  
- `materials.cpp/h`  
- `monster.cpp/h`  
- `monsters.cpp/h`  
- `npc.cpp/h`  
- `npcs.cpp/h`  
- `outfit.h`  
- `spawn_monster.cpp/h`  
- `spawn_npc.cpp/h`  
- `tileset.cpp/h`  
- `town.cpp/h`  
- `waypoints.cpp/h`  
- `zones.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 3 — `brushes/`  
Criar `source/brushes/` e mover:  
- `brush.cpp/h`  
- `brush_enums.h`  
- `brush_tables.cpp`  
- `carpet_brush.cpp/h`  
- `doodad_brush.cpp/h`  
- `eraser_brush.cpp`  
- `ground_brush.cpp/h`  
- `house_brush.cpp/h`  
- `house_exit_brush.cpp/h`  
- `monster_brush.cpp/h`  
- `npc_brush.cpp/h`  
- `raw_brush.cpp/h`  
- `spawn_monster_brush.cpp/h`  
- `spawn_npc_brush.cpp/h`  
- `table_brush.cpp/h`  
- `wall_brush.cpp/h`  
- `waypoint_brush.cpp/h`  
- `zone_brush.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 4 — `map/`  
Criar `source/map/` e mover:  
- `basemap.cpp/h`  
- `map.cpp/h`  
- `map_allocator.h`  
- `map_overlay.h`  
- `map_region.cpp/h`  
- `tile.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 5 — `io/`  
Criar `source/io/`, `source/io/otbm/`, `source/io/bitmap_to_map/` e mover:  
- `filehandle.cpp/h` → `io/`  
- `iomap.cpp/h` → `io/`  
- `iomap_otbm.cpp/h` → `io/otbm/` *(manter `#include "gui.h"` temporariamente — será resolvido na Fase 6)*  
- `iominimap.cpp/h` → `io/`  
- `bitmap_to_map_converter.cpp/h` → `io/bitmap_to_map/`  
- `pngfiles.cpp/h` → `io/`  
  
**NOTA:** `iomap_otmm.cpp/h` — mover para `io/` mas **NÃO adicionar ao CMakeLists** (dead code). Adicionar comentário no topo: `// Dead code — not compiled. Kept for reference.`  
  
**NOTA:** Adicionar exceção temporária no `check_module_boundaries.sh` para `iomap_otbm.cpp` incluindo `gui.h` (já previsto no script da Fase 2).  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 6 — `live/`  
Criar `source/live/` e mover:  
- `live_action.cpp/h`  
- `live_client.cpp/h`  
- `live_packets.h`  
- `live_peer.cpp/h`  
- `live_server.cpp/h`  
- `live_socket.cpp/h`  
- `live_tab.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 7 — `net/`  
Criar `source/net/` e mover:  
- `net_connection.cpp/h`  
- `rme_net.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 8 — `rendering/`  
Criar `source/rendering/` e mover:  
- `gl_compat.h`  
- `gl_renderer.cpp/h`  
- `graphics.cpp/h`  
- `light_drawer.cpp/h`  
- `map_drawer.cpp/h`  
- `map_display.cpp/h`  
- `map_window.cpp/h`  
- `sprite_appearances.cpp/h`  
- `sprites.h`  
  
Atualizar `set_source_files_properties` do unity build exclusion no CMakeLists:  
```cmake  
set_source_files_properties(rendering/gl_renderer.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)  
```  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 9 — `editor/`  
Criar `source/editor/` e mover:  
- `action.cpp/h`  
- `copybuffer.cpp/h`  
- `editor.cpp/h`  
- `editor_tabs.cpp/h`  
- `selection.cpp/h`  
- `process_com.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 10 — `ui/`  
Criar `source/ui/`, `source/ui/dialogs/`, `source/ui/menubar/`, `source/ui/palette/`, `source/ui/toolbar/`, `source/ui/windows/` e mover:  
  
**`ui/` raiz:**  
- `dcbutton.cpp/h`  
- `gui.cpp/h`  
- `gui_ids.h`  
- `map_tab.cpp/h`  
- `numbertextctrl.cpp/h`  
- `positionctrl.cpp/h`  
  
**`ui/dialogs/`:**  
- `about_window.cpp/h`  
- `common_windows.cpp/h`  
- `container_properties_window.cpp/h`  
- `old_properties_window.cpp/h`  
- `preferences.cpp/h`  
- `properties_window.cpp/h`  
  
**`ui/menubar/`:**  
- `main_menubar.cpp/h`  
  
**`ui/palette/`:**  
- `palette_brushlist.cpp/h`  
- `palette_common.cpp/h`  
- `palette_house.cpp/h`  
- `palette_monster.cpp/h`  
- `palette_npc.cpp/h`  
- `palette_waypoints.cpp/h`  
- `palette_window.cpp/h`  
- `palette_zones.cpp/h`  
  
**`ui/toolbar/`:**  
- `main_toolbar.cpp/h`  
  
**`ui/windows/`:**  
- `actions_history_window.cpp/h`  
- `add_item_window.cpp/h`  
- `add_tileset_window.cpp/h`  
- `bitmap_to_map_window.cpp/h`  
- `browse_tile_window.cpp/h`  
- `dat_debug_view.cpp/h`  
- `find_item_window.cpp/h`  
- `minimap_window.cpp/h`  
- `replace_items_window.cpp/h`  
- `result_window.cpp/h`  
- `tileset_window.cpp/h`  
- `welcome_dialog.cpp/h`  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 11 — `app/`  
Criar `source/app/` e mover:  
- `application.cpp/h`  
- `artprovider.cpp/h`  
- `definitions.h`  
- `main.h`  
- `settings.cpp/h`  
- `updater.cpp/h`  
  
**ATENÇÃO:** `main.h` é o PCH incluído por quase todos os `.cpp`. Ao movê-lo para `app/main.h`, **TODOS** os `.cpp` que fazem `#include "main.h"` precisam ser atualizados para `#include "app/main.h"`. Isso afeta ~130 arquivos. Usar find/replace global:  
```bash  
find source/ -name "*.cpp" -exec sed -i 's|#include "main.h"|#include "app/main.h"|g' {} +  
```  
  
Atualizar também o CMakeLists se `main.h` é referenciado como PCH path.  
  
Atualizar todos os `#include` e CMakeLists. Compilar e verificar.  
  
#### Grupo 12 — `templates/`  
Criar `source/templates/` e mover:  
- `templatemap76-74.cpp`  
- `templatemap81.cpp`  
- `templatemap854.cpp`  
- `templatemapclassic.cpp`  
- `templates.h`  
  
Atualizar CMakeLists. Compilar e verificar.  
  
#### Grupo 13 — Arquivos restantes na raiz  
Após todos os grupos, verificar o que sobrou em `source/` raiz. Devem restar apenas:  
- `client_assets.cpp/h` (depende de protobuf + game + io — manter na raiz)  
- `CMakeLists.txt`  
  
Se houver outros arquivos esquecidos, mover para o módulo mais adequado.  
  
**Validação final da Fase 3:** Rodar `bash scripts/check_module_boundaries.sh`. Deve passar (com a exceção conhecida de `iomap_otbm.cpp`). Executar o checklist de teste manual completo.  
  
---  
  
### Fase 4 — Extrair `HotkeyManager` de `gui.h` (dificuldade: fácil)  
  
A classe `GUI` em `ui/gui.h` contém a struct `Hotkey`, um array `Hotkey hotkeys[10]`, e métodos de hotkey (`SetHotkey`, `GetHotkey`, `SaveHotkeys`, `LoadHotkeys`, `EnableHotkeys`, `DisableHotkeys`, `AreHotkeysEnabled`). Tudo isso é independente do resto da GUI.  
  
**Passos:**  
  
1. Criar `editor/hotkey_manager.h`:  
   ```cpp  
   #pragma once  
   #include <string>  
   #include <cstdint>  
  
   struct Hotkey {  
       enum Type { NONE, BRUSH, ITEM };  
       Type type = NONE;  
       uint16_t id = 0;  
       std::string name;  
   };  
  
   class HotkeyManager {  
   public:  
       void SetHotkey(int index, const Hotkey& hotkey);  
       Hotkey GetHotkey(int index) const;  
       void SaveHotkeys() const;  
       void LoadHotkeys();  
       void EnableHotkeys();  
       void DisableHotkeys();  
       bool AreHotkeysEnabled() const;  
  
   private:  
       Hotkey hotkeys[10];  
       bool hotkeys_enabled = true;  
   };  
  
   extern HotkeyManager g_hotkeys;  
   ```  
  
2. Criar `editor/hotkey_manager.cpp`:  
   - Implementar os métodos (copiar a lógica de `gui.cpp`)  
   - Definir `HotkeyManager g_hotkeys;`  
  
3. Em `ui/gui.h`:  
   - Remover a struct `Hotkey` e os membros/métodos de hotkey  
   - Adicionar `#include "editor/hotkey_manager.h"`  
  
4. Em `ui/gui.cpp`:  
   - Remover as implementações dos métodos de hotkey  
  
5. Atualizar call sites em todo o projeto:  
   - `g_gui.SetHotkey(...)` → `g_hotkeys.SetHotkey(...)`  
   - `g_gui.GetHotkey(...)` → `g_hotkeys.GetHotkey(...)`  
   - `g_gui.SaveHotkeys()` → `g_hotkeys.SaveHotkeys()`  
   - `g_gui.LoadHotkeys()` → `g_hotkeys.LoadHotkeys()`  
   - `g_gui.EnableHotkeys()` → `g_hotkeys.EnableHotkeys()`  
   - `g_gui.DisableHotkeys()` → `g_hotkeys.DisableHotkeys()`  
   - `g_gui.AreHotkeysEnabled()` → `g_hotkeys.AreHotkeysEnabled()`  
   - Adicionar `#include "editor/hotkey_manager.h"` nos arquivos que usam essas funções  
  
6. Atualizar `source/CMakeLists.txt` — adicionar `editor/hotkey_manager.cpp`  
  
7. Compilar e verificar. Executar checklist: hotkeys (1-0) funcionam?  
  
**NOTA sobre ordem de inicialização:** NÃO chamar `LoadHotkeys()` no construtor de `HotkeyManager`. Chamar explicitamente em `app/application.cpp` após `g_settings` estar inicializado, no mesmo ponto onde `g_gui.LoadHotkeys()` era chamado antes.  
  
---  
  
### Fase 5 — Extrair `TileOperations` de `tile.h/tile.cpp` (dificuldade: média)  
  
A classe `Tile` (~369 linhas de header, ~810 de impl) mistura data struct com operações de mutação.  
  
**Passos:**  
  
1. Criar `map/tile_operations.h`:  
   ```cpp  
   #pragma once  
   #include <memory>  
   #include <vector>  
  
   class Tile;  
   class BaseMap;  
   class WallBrush;  
   class Item;  
   class House;  
  
   namespace TileOperations {  
       void borderize(Tile* tile, BaseMap* map);  
       void cleanBorders(Tile* tile);  
       void wallize(Tile* tile, BaseMap* map);  
       void cleanWalls(Tile* tile);  
       void cleanWalls(Tile* tile, WallBrush* wb);  
       void tableize(Tile* tile, BaseMap* map);  
       void cleanTables(Tile* tile);  
       void carpetize(Tile* tile, BaseMap* map);  
       std::unique_ptr<Tile> deepCopy(const Tile* tile, BaseMap& map);  
       void merge(Tile* dest, Tile* src);  
       void select(Tile* tile);  
       void deselect(Tile* tile);  
       void selectGround(Tile* tile);  
       void deselectGround(Tile* tile);  
       void update(Tile* tile);  
   }  
   ```  
   Adaptar a lista de funções conforme os métodos reais encontrados em `tile.h`. Incluir todos os métodos que são operações de mutação (não getters/queries).  
  
2. Criar `map/tile_operations.cpp`:  
   - Mover a implementação de cada método de `map/tile.cpp`  
   - Adaptar: `this->` vira `tile->`, membros acessados via `tile->`  
   - Se métodos acessam membros privados de `Tile`, adicionar `friend namespace TileOperations;` em `tile.h` OU usar getters existentes  
  
3. Em `map/tile.h` e `map/tile.cpp`:  
   - **Manter os métodos originais como wrappers temporários:**  
     ```cpp  
     // tile.h — wrapper  
     void borderize(BaseMap* parent);  
  
     // tile.cpp — wrapper  
     #include "map/tile_operations.h"  
     void Tile::borderize(BaseMap* parent) {  
         TileOperations::borderize(this, parent);  
     }  
     ```  
   - Fazer isso para TODOS os métodos extraídos  
  
4. Atualizar `source/CMakeLists.txt` — adicionar `map/tile_operations.cpp`  
  
5. Compilar e verificar. Executar checklist: borderize, wallize, tableize, carpetize, select/deselect, copy/paste funcionam?  
  
**NOTA:** Os wrappers serão removidos na Fase 11, quando todos os call sites forem atualizados para usar `TileOperations::` diretamente.  
  
---  
  
### Fase 6 — Decompor `iomap_otbm.cpp` (dificuldade: média)  
  
`io/otbm/iomap_otbm.cpp` (~2020 linhas) contém toda a serialização OTBM num único arquivo. Decompor em classes especializadas.  
  
**Passo 1:** Criar `io/otbm/otbm_types.h`:  
- Extrair de `io/otbm/iomap_otbm.h` os enums (`OTBM_NodeTypes`, `OTBM_ItemAttribute`, etc.) e structs auxiliares  
- `iomap_otbm.h` passa a incluir `otbm_types.h`  
  
**Passo 2:** Criar `io/otbm/item_serialization_otbm.h/cpp`:  
- Classe `ItemSerializationOTBM` com métodos estáticos:  
  - `static Item* readItem(BinaryNode* stream, const IOMap& maphandle);`  
  - `static void writeItem(const Item* item, NodeFileWriteHandle& writer, const IOMap& maphandle);`  
- Mover `Item::Create_OTBM` e a lógica de escrita de items de `iomap_otbm.cpp`  
  
**Passo 3:** Criar `io/otbm/tile_serialization_otbm.h/cpp`:  
- Classe `TileSerializationOTBM` com métodos estáticos:  
  - `static void readTiles(Map& map, BinaryNode* mapNode, const IOMap& maphandle, std::function<bool(int, const std::string&)> progress);`  
  - `static void writeTiles(const Map& map, NodeFileWriteHandle& writer, const IOMap& maphandle, std::function<bool(int, const std::string&)> progress);`  
- Mover a lógica de leitura/escrita de `OTBM_TILE_AREA`, `OTBM_TILE`, `OTBM_HOUSETILE`  
- Usar `ItemSerializationOTBM` para ler/escrever items dentro dos tiles  
- O parâmetro `progress` substitui as chamadas diretas a `g_gui.SetLoadDone()`  
  
**Passo 4:** Criar `io/otbm/town_serialization_otbm.h/cpp`:  
- Classe `TownSerializationOTBM` com métodos estáticos:  
  - `static void readTowns(Map& map, BinaryNode* mapNode);`  
  - `static void writeTowns(const Map& map, NodeFileWriteHandle& writer);`  
- Mover a lógica de `OTBM_TOWNS`  
  
**Passo 5:** Criar `io/otbm/waypoint_serialization_otbm.h/cpp`:  
- Classe `WaypointSerializationOTBM` com métodos estáticos:  
  - `static void readWaypoints(Map& map, BinaryNode* mapNode);`  
  - `static void writeWaypoints(const Map& map, NodeFileWriteHandle& writer);`  
- Mover a lógica de `OTBM_WAYPOINTS`  
  
**Passo 6:** Criar `io/otbm/header_serialization_otbm.h/cpp`:  
- Classe `HeaderSerializationOTBM` com métodos estáticos:  
  - `static bool readHeader(Map& map, BinaryNode* rootNode, IOMap& maphandle);`  
  - `static void writeHeader(const Map& map, NodeFileWriteHandle& writer, const IOMap& maphandle);`  
- Mover a lógica de `OTBM_ROOTV1` e `OTBM_MAP_DATA`  
  
**Passo 7:** Refatorar `io/otbm/iomap_otbm.cpp`:  
- `IOMapOTBM::loadMap` fica slim — apenas:  
  1. Abre o arquivo  
  2. Chama `HeaderSerializationOTBM::readHeader`  
  3. Chama `TileSerializationOTBM::readTiles` (passando lambda de progress)  
  4. Chama `TownSerializationOTBM::readTowns`  
  5. Chama `WaypointSerializationOTBM::readWaypoints`  
- `IOMapOTBM::saveMap` idem para escrita  
  
**Passo 8:** Remover `#include "gui.h"` de `iomap_otbm.cpp`:  
- Substituir chamadas a `g_gui.SetLoadDone()` por chamadas ao callback `progress`:  
  ```cpp  
  // ANTES:  
  g_gui.SetLoadDone(done, "Loading tiles...");  
  
  // DEPOIS (dentro de TileSerializationOTBM):  
  if (progress) progress(done, "Loading tiles...");  
  
  // O lambda é criado em quem chama loadMap:  
  auto progress = [](int done, const std::string& msg) -> bool {  
      return g_gui.SetLoadDone(done, wxString(msg));  
  };  
  ```  
- Remover a exceção temporária do `check_module_boundaries.sh`  
  
**Passo 9:** Atualizar `source/CMakeLists.txt` — adicionar os 5 novos `.cpp`.  
  
**Passo 10:** Compilar e verificar. **Executar validação binária:**  
```bash  
# Abrir mapa → salvar como mapa_antes.otbm (ANTES da fase 6)  
# Abrir mapa → salvar como mapa_depois.otbm (DEPOIS da fase 6)  
diff mapa_antes.otbm mapa_depois.otbm  
# Deve ser 0 bytes de diferença  
```  
  
---  
  
### Fase 7 — Decompor `main_menubar.cpp` (dificuldade: alta)  
  
`ui/menubar/main_menubar.cpp` (~2677 linhas) contém ~170 ActionIDs, construção de menus, e ~80 event handlers.  
  
**Passos:**  
  
1. Criar `ui/menubar/menu_ids.h`:  
   - Extrair todos os `enum ActionID` de `main_menubar.h`  
   - `main_menubar.h` passa a incluir `menu_ids.h`  
  
2. Criar `ui/menubar/file_menu.cpp/h`:  
   - Classe `FileMenu` ou free functions em namespace `FileMenuHandlers`  
   - Mover: `OnNew`, `OnOpen`, `OnSave`, `OnSaveAs`, `OnClose`, `OnGenerateMap`, `OnPreferences`, `OnQuit`, recent files logic  
   - `MainMenuBar` delega para `FileMenu`  
  
3. Criar `ui/menubar/edit_menu.cpp/h`:  
   - Mover: `OnUndo`, `OnRedo`, `OnCut`, `OnCopy`, `OnPaste`, `OnDelete`, `OnFind`, `OnReplace`, `OnSelectAll`, `OnBorderize`, etc.  
  
4. Criar `ui/menubar/map_menu.cpp/h`:  
   - Mover: `OnMapProperties`, `OnMapStatistics`, `OnMapCleanup`, `OnMapRemoveItems`, `OnMapEditTowns`, `OnMapEditHouses`, etc.  
  
5. Criar `ui/menubar/view_menu.cpp/h`:  
   - Mover: `OnToggleGrid`, `OnToggleLight`, `OnToggleCreatures`, `OnToggleSpawns`, `OnZoomIn`, `OnZoomOut`, etc.  
  
6. Criar `ui/menubar/window_menu.cpp/h`:  
   - Mover: `OnMinimapWindow`, `OnPaletteWindow`, `OnToolbar`, etc.  
  
7. Simplificar `ui/menubar/main_menubar.cpp`:  
   - Fica com: construção dos menus (chama `FileMenu::Build`, `EditMenu::Build`, etc.) e `Update()` que delega  
   - Cada handler no event table chama o handler do sub-módulo correspondente  
  
8. Atualizar CMakeLists. Compilar e verificar.  
  
**Teste:** Todos os menus abrem? Todos os handlers funcionam (New, Open, Save, Undo, Redo, Find, etc.)?  
  
---  
  
### Fase 8 — Decompor `map_drawer.cpp` (dificuldade: alta)  
  
`rendering/map_drawer.cpp` (~2411 linhas) mistura rendering de tiles, grid, luz, seleção, overlays, criaturas, tooltips.  
  
**Passo 1:** Criar `RenderContext` struct:  
```cpp  
// rendering/render_context.h  
#pragma once  
struct RenderContext {  
    float zoom;  
    int scroll_x, scroll_y;  
    int start_x, start_y, end_x, end_y;  
    int floor;  
    int screensize_x, screensize_y;  
    int viewport_x, viewport_y;  
    const DrawingOptions* options;  
    // ... outros campos compartilhados  
};  
```  
`MapDrawer` cria um `RenderContext` no início de cada frame e passa por `const&` aos sub-drawers.  
  
**Passo 2:** Criar `rendering/drawers/grid_drawer.cpp/h`:  
- Free function ou classe: `void drawGrid(const RenderContext& ctx, GLRenderer& renderer);`  
- Extrair a lógica de desenho de grid de `map_drawer.cpp`  
  
**Passo 3:** Criar `rendering/drawers/selection_drawer.cpp/h`:  
- `void drawSelection(const RenderContext& ctx, GLRenderer& renderer, const Map& map);`  
- Extrair a lógica de overlay de seleção  
  
**Passo 4:** Criar `rendering/drawers/creature_drawer.cpp/h`:  
- `void drawCreatures(const RenderContext& ctx, GLRenderer& renderer, const Map& map);`  
- Extrair a lógica de rendering de criaturas/spawns  
  
**Passo 5:** Criar `rendering/drawers/overlay_drawer.cpp/h`:  
- `void drawOverlays(const RenderContext& ctx, GLRenderer& renderer, const Map& map);`  
- Extrair tooltips, waypoint labels, house overlays, etc.  
  
**Passo 6:** Simplificar `rendering/map_drawer.cpp`:  
- `MapDrawer::Draw()` fica como orquestrador:  
  1. Cria `RenderContext`  
  2. Chama `drawTiles(ctx, ...)` (lógica principal de tiles — fica no próprio `map_drawer.cpp`)  
  3. Chama `drawGrid(ctx, ...)`  
  4. Chama `drawCreatures(ctx, ...)`  
  5. Chama `drawSelection(ctx, ...)`  
  6. Chama `drawOverlays(ctx, ...)`  
  
**Passo 7:** Atualizar CMakeLists. Compilar e verificar.  
  
**Teste:** Mapa renderiza corretamente? Grid aparece? Seleção aparece? Criaturas/spawns visíveis? Tooltips funcionam? Zoom/scroll/floor change funcionam?  
  
---  
  
### Fase 9 — Decompor `gui.cpp` (dificuldade: alta)  
  
`ui/gui.cpp` (~2055 linhas) é o god object central. Após a Fase 4 (HotkeyManager extraído), ainda contém loading bar, brush state, palette management, file operations, minimap, clipboard, e mais.  
  
**Passos:**  
  
1. Criar `ui/loading_bar.cpp/h`:  
   - Classe `LoadingBar` com: `Create(msg)`, `SetDone(done, msg)`, `SetScale(from, to)`, `Destroy()`  
   - Extrair de `GUI` os métodos `CreateLoadBar`, `SetLoadDone`, `SetLoadScale`, `DestroyLoadBar` e membros associados  
   - `GUI` mantém um `LoadingBar` como membro e delega  
  
2. Criar `ui/brush_state.cpp/h`:  
   - Classe `BrushState` com: `GetCurrentBrush()`, `SetBrush(brush)`, `GetBrushSize()`, `SetBrushSize(size)`, `GetBrushShape()`, `SetBrushShape(shape)`, `GetBrushVariation()`, etc.  
   - Extrair de `GUI` todos os membros e métodos relacionados a brush state  
   - `GUI` mantém um `BrushState` como membro e delega  
  
3. Criar `ui/palette_manager.cpp/h`:  
   - Classe `PaletteManager` com: `AddPalette`, `GetPalette`, `GetSelectedPalette`, `SelectPalette`, `RefreshPalettes`, `RebuildPalettes`  
   - Extrair de `GUI` a `PaletteList` e métodos de palette  
   - `GUI` mantém um `PaletteManager` como membro e delega  
  
4. Criar `ui/file_operations.cpp/h`:  
   - Free functions ou classe estática: `NewMap(GUI&)`, `OpenMap(GUI&)`, `SaveMap(GUI&)`, `SaveMapAs(GUI&)`, `CloseMap(GUI&)`  
   - Extrair de `GUI::NewMap`, `GUI::OpenMap`, etc.  
   - Recebem `GUI&` como parâmetro para acessar loading bar, tabs, etc.  
  
5. Simplificar `ui/gui.cpp`:  
   - `GUI` fica com: mode management, tab management, rendering lock, title/status bar, e orquestração  
   - Delega para `LoadingBar`, `BrushState`, `PaletteManager`, `FileOperations`  
  
6. Atualizar CMakeLists. Compilar e verificar.  
  
**Teste:** Abrir/salvar mapa funciona? Loading bar aparece durante load? Brushes trocam corretamente? Palettes abrem/fecham? Brush size/shape muda?  
  
---  
  
### Fase 10 — Decompor `editor.cpp` (dificuldade: alta)  
  
`editor/editor.cpp` (~2169 linhas) mistura undo/redo, clipboard, map operations, selection, e live editing.  
  
**Passos:**  
  
1. Criar `editor/action_queue.cpp/h`:  
   - Classe `ActionQueue` com: `addAction`, `undo`, `redo`, `canUndo`, `canRedo`, `clear`, `getActions`  
   - Extrair de `Editor` os membros e métodos de undo/redo (action stack, batch actions, etc.)  
   - `Editor` mantém um `ActionQueue` como membro e delega  
  
2. Criar `editor/operations/map_operations.cpp/h`:  
   - Free functions: `borderizeSelection(Editor&)`, `wallizeSelection(Editor&)`, `randomizeSelection(Editor&)`, `clearSelection(Editor&)`, `moveSelection(Editor&, Position offset)`, etc.  
   - Extrair de `Editor` os métodos de operação em massa sobre o mapa  
  
3. Simplificar `editor/editor.cpp`:  
   - `Editor` fica com: map ownership, selection state, live state, e orquestração  
   - Delega para `ActionQueue` e `MapOperations`  
  
4. Atualizar CMakeLists. Compilar e verificar.  
  
**Teste:** Undo/redo funciona (múltiplos níveis)? Borderize selection funciona? Clear selection funciona? Move selection funciona? Live editing funciona?  
  
---  
  
### Fase 11 — Remover wrappers temporários de `Tile` (dificuldade: fácil, volume médio)  
  
Os wrappers criados na Fase 5 agora podem ser removidos.  
  
**Passos:**  
  
1. Buscar todos os call sites que usam os wrappers em `Tile`:  
   ```bash  
grep -rn "tile->borderize\|tile->wallize\|tile->tableize\|tile->carpetize\|tile->deepCopy\|tile->merge\|tile->select()\|tile->deselect()\|tile->selectGround\|tile->deselectGround\|tile->popSelectedItems\|tile->getSelectedItems\|tile->getTopSelectedItem\|tile->addBorderItem\|tile->addWallItem\|tile->addHouseExit\|tile->removeHouseExit" source/  
   ```  
  
2. Substituir cada call site pelo equivalente `TileOperations::`:  
   - `tile->borderize(map)` → `TileOperations::borderize(tile, map)`  
   - `tile->wallize(map)` → `TileOperations::wallize(tile, map)`  
   - `tile->tableize(map)` → `TileOperations::tableize(tile, map)`  
   - `tile->carpetize(map)` → `TileOperations::carpetize(tile, map)`  
   - `tile->deepCopy(map)` → `TileOperations::deepCopy(tile, map)`  
   - `tile->merge(src)` → `TileOperations::merge(tile, src)`  
   - `tile->select()` → `TileOperations::select(tile)`  
   - `tile->deselect()` → `TileOperations::deselect(tile)`  
   - `tile->selectGround()` → `TileOperations::selectGround(tile)`  
   - `tile->deselectGround()` → `TileOperations::deselectGround(tile)`  
   - `tile->popSelectedItems(flag)` → `TileOperations::popSelectedItems(tile, flag)`  
   - `tile->getSelectedItems()` → `TileOperations::getSelectedItems(tile)`  
   - `tile->getTopSelectedItem()` → `TileOperations::getTopSelectedItem(tile)`  
   - `tile->addBorderItem(item)` → `TileOperations::addBorderItem(tile, item)`  
   - `tile->addWallItem(item)` → `TileOperations::addWallItem(tile, item)`  
   - `tile->addHouseExit(h)` → `TileOperations::addHouseExit(tile, h)`  
   - `tile->removeHouseExit(h)` → `TileOperations::removeHouseExit(tile, h)`  
  
3. Adicionar `#include "map/tile_operations.h"` em cada arquivo que agora chama `TileOperations::` diretamente.  
  
4. Remover os wrappers de `map/tile.h` (declarações) e `map/tile.cpp` (implementações).  
  
5. Compilar e verificar.  
  
**Teste:** Borderize, wallize, tableize, carpetize, select/deselect, copy/paste, undo/redo — tudo funciona?  
  
---  
  
### Fase 12 — Extrair XML serializers de `iomap_otbm.cpp` (dificuldade: média)  
  
Os métodos de spawn/house/npc/zone XML em `iomap_otbm.cpp` são independentes do formato OTBM binário e podem ser extraídos para arquivos separados.  
  
**Passos:**  
  
1. Criar `io/xml/spawn_serialization_xml.cpp/h`:  
   - Classe `SpawnSerializationXML` com métodos estáticos `loadSpawns(Map&, const std::string& path)` e `saveSpawns(const Map&, const std::string& path)`  
   - Extrair de `IOMapOTBM` a lógica de leitura/escrita de spawns XML (monster spawns e NPC spawns)  
   - Usar `ProgressCallback` para reportar progresso (mesmo pattern da Fase 6)  
  
2. Criar `io/xml/house_serialization_xml.cpp/h`:  
   - Classe `HouseSerializationXML` com `loadHouses(Map&, const std::string& path)` e `saveHouses(const Map&, const std::string& path)`  
   - Extrair leitura/escrita de houses XML  
  
3. Criar `io/xml/zone_serialization_xml.cpp/h`:  
   - Classe `ZoneSerializationXML` com `loadZones(Map&, const std::string& path)` e `saveZones(const Map&, const std::string& path)`  
   - Extrair leitura/escrita de zones XML  
  
4. Atualizar `IOMapOTBM` para delegar aos novos serializers XML:  
   ```cpp  
   // Em io/otbm/iomap_otbm.cpp  
   #include "io/xml/spawn_serialization_xml.h"  
   #include "io/xml/house_serialization_xml.h"  
   #include "io/xml/zone_serialization_xml.h"  
  
   // Onde antes tinha a lógica inline:  
   SpawnSerializationXML::loadSpawns(map, spawnfile);  
   HouseSerializationXML::loadHouses(map, housefile);  
   ZoneSerializationXML::loadZones(map, zonefile);  
   ```  
  
5. Atualizar CMakeLists — adicionar os novos `.cpp`.  
  
6. Compilar e verificar.  
  
**Validação:** Abrir mapa com spawns/houses/zones → salvar → reabrir → tudo presente e correto? Usar diff binário no `.otbm` e diff textual nos `.xml` para garantir que a saída é idêntica.  
  
---  
  
## Checklist de Teste Manual (usar após CADA fase)  
  
```
[ ] Abrir mapa existente (OTBM) — todos os tiles carregam?  
[ ] Salvar mapa — reabrir — está idêntico?  
[ ] Undo/Redo funciona (3+ níveis)?  
[ ] Hotkeys funcionam (1-0)?  
[ ] Brushes funcionam (ground, wall, doodad, carpet, table, eraser)?  
[ ] Seleção funciona (select, deselect, copy, paste, cut)?  
[ ] Borderize automático funciona?  
[ ] Live server/client conecta?  
[ ] Minimap renderiza?  
[ ] Light rendering funciona?  
[ ] Zoom in/out funciona?  
[ ] Scroll funciona?  
[ ] Trocar de floor funciona?  
[ ] Criaturas/spawns aparecem no mapa?  
[ ] Houses renderizam corretamente?  
[ ] Find/Replace funciona?  
[ ] Properties window abre e salva?  
```  
  
## Validação Binária (usar nas Fases 6, 10 e 12)  
  
```bash  
# ANTES da refatoração:  
# Abrir mapa X → salvar como mapa_antes.otbm  
  
# DEPOIS da refatoração:  
# Abrir mapa X → salvar como mapa_depois.otbm  
  
diff mapa_antes.otbm mapa_depois.otbm  
# Se diff = 0 bytes → serialização idêntica → GARANTIA de que nada mudou  
  
# Para XMLs (Fase 12):  
diff spawns_antes.xml spawns_depois.xml  
diff houses_antes.xml houses_depois.xml  
diff zones_antes.xml zones_depois.xml  
```  
  
## Assertions Temporárias (usar nas Fases 4-6)  
  
Adicionar `assert()` nos pontos críticos durante o desenvolvimento. Remover depois que a fase estiver estável:  
  
```cpp  
// Em editor/hotkey_manager.cpp — verificar que hotkeys carregaram  
void HotkeyManager::LoadHotkeys() {  
    // ... load logic ...  
    assert(hotkeys_enabled_ || "HotkeyManager: hotkeys failed to initialize");  
}  
  
// Em map/tile_operations.cpp — verificar que tile não é null  
void TileOperations::borderize(Tile* tile, BaseMap* map) {  
    assert(tile != nullptr && "TileOperations::borderize called with null tile");  
    assert(map != nullptr && "TileOperations::borderize called with null map");  
    // ... logic ...  
}  
  
// Em io/otbm/item_serialization_otbm.cpp — verificar integridade  
Item* ItemSerializationOTBM::readItem(BinaryNode* stream, const IOMap& maphandle) {  
    assert(stream != nullptr && "readItem called with null stream");  
    // ... logic ...  
}  
  
// Em io/otbm/tile_serialization_otbm.cpp — verificar contagem  
void TileSerializationOTBM::readTiles(Map& map, BinaryNode* mapNode, int& itemCount, ProgressCallback progress) {  
    assert(mapNode != nullptr && "readTiles called with null mapNode");  
    // ... logic ...  
    // No final:  
    spdlog::info("TileSerializationOTBM: loaded {} items", itemCount);  
}  
```  
  
## Fases Futuras (Rodada 3 — opcional, após Rodada 2 estável)  
  
| Tarefa | Descrição |  
|---|---|  
| Adicionar testes unitários | Para `TileOperations`, serializers OTBM/XML, `HotkeyManager`, `ActionQueue` |  
| Limpar includes desnecessários | Usar output do `misc-include-cleaner` do clang-tidy |  
| Eliminar dependências circulares residuais | Verificar com `check_module_boundaries.sh` e corrigir |  
| Decompor arquivos >500 linhas restantes | Verificar se algum arquivo ainda passa do threshold após Rodada 2 |  
| Criar `ARCHITECTURE.md` | Documentar a estrutura de módulos, regras de dependência, e como adicionar novos módulos |  
| Slim down `main.h` | Remover includes que não são usados por >50% dos `.cpp`. Mover includes específicos para os `.cpp` que precisam |  
| Adicionar `#pragma once` guards | Verificar que todos os `.h` usam `#pragma once` consistentemente |

O plano está completo. Verifiquei todos os arquivos em source/ contra a estrutura alvo e encontrei apenas 3 problemas menores:

1. process_com.cpp/h está no módulo errado
No prompt está em editor/, mas é código de IPC (inter-process communication via wxIPC) — usado para single-instance do app (quando você abre um .otbm e o editor já está rodando). Deveria estar em app/, não em editor/. process_com.h:25-49

Correção: Mover process_com.cpp/h de editor/ para app/ na Estrutura Alvo e no Grupo 9→Grupo 11.

2. eraser_brush.h não existe
O prompt lista eraser_brush.cpp em brushes/ — correto. Mas não existe eraser_brush.h no repositório. Só o .cpp. Isso está OK no prompt (ele só lista eraser_brush.cpp), mas vale confirmar que não vai tentar mover um .h que não existe.

3. rme-install — diretório vazio
Existe um diretório source/rme-install que está vazio. Não é mencionado no prompt e não precisa ser — pode ser ignorado ou deletado.