# Plan integrare Sonitex Offline Private Shop — CLIENT (metin2-client)

> Acest plan este executat de Claude din proiectul `C:\Users\skema\Desktop\metin2-client\`.
> Server-side (proiectul `metin2-server`) este DEJA integrat si testat. Clientul trebuie sa oglindeasca protocolul si sa adauge UI-ul.

## Context obligatoriu

- **Plugin sursa (referinta vizuala/structurala):** `C:\Users\skema\Desktop\Yeni Sonitex Basic Offline Shop\Client\` (Python+UI script) si `C:\Users\skema\Desktop\Yeni Sonitex Basic Offline Shop\Source\Client\` (C++ binar).
- **Server reference (deja implementat):**
  - `C:\Users\skema\Desktop\metin2-server\src\server\common\length.h` — constante (`PRIVATE_SHOP_WIDTH=5`, `HEIGHT=8`, `PAGE_MAX_NUM=2`, `HOST_ITEM_MAX_NUM=80`, `INVENTORY_PAGE_*`)
  - `C:\Users\skema\Desktop\metin2-server\src\server\common\service.h` — `#define WJ_PREMIUM_PRIVATE_SHOP`
  - `C:\Users\skema\Desktop\metin2-server\src\server\common\tables.h` — `TPrivateShop`, `TItemPrice` (doar `llGold`), `TPlayerPrivateShopItem`, `TPacketDG*`, `TPacketGD*`
  - `C:\Users\skema\Desktop\metin2-server\src\server\game\src\packet.h` — `HEADER_CG_PRIVATE_SHOP=230`, `HEADER_GC_PRIVATE_SHOP=230`, enum `EPrivateShopGCSubheader` (19 valori), enum `EPrivateShopCGSubheader` (15 valori — fara search), structurile `TPacketCGPrivateShop*` / `TPacketGCPrivateShop*` / `TPrivateShopItem` / `TPrivateShopItemData`
- **Decizii arhitecturale obligatorii (oglindesc serverul, NU le incalca):**
  1. **Yang-only.** TItemPrice are doar `long long llGold`. NU `dwCheque`, NU `Won`, NU `WJ_PRIVATE_SHOP_CHEQUE`.
  2. **Fara search.** SUBHEADER `SEARCH_*`, fisierele `uiprivateshopsearch.py` / `privateshopsearchwindow.py`, `gameoptiondialog.py` (filtru), si tot codul din `PythonPrivateShop*` legat de cautare se EXCLUD complet.
  3. **Fara premium UI** in Faza 1. Itemul 71221 (Premium Bundle) si folderul `patch_premium_private_shop/` se IGNORA. Doar itemul `50200` deschide panoul.
  4. **Subheader-ele si headerele DEFINITION trebuie sa fie BINARY-IDENTICE** cu serverul. Orice mismatch = pachete corupte = crash.
  5. **Comunicare:** Romana per CLAUDE.md global.
  6. **Branch git:** `main`. Nu modifica git config. Nu da commit decat dupa testare in joc.
- **Loguri:** la fiecare modificare neviabila in C++ (network handler, panic path), adauga `Tracef("PRIVATESHOP_CLIENT: ...")` sau echivalent de debug folosit deja in proiect. NU logging excesiv in py (UI-ul e mut).

## Definitii oglinda obligatorii

Aceste valori NU se schimba — sunt enumerate aici ca tu, Claude, sa le verifici la fiecare scriere de packet.

```cpp
HEADER_CG_PRIVATE_SHOP = 230
HEADER_GC_PRIVATE_SHOP = 230

enum EPrivateShopGCSubheader (in ordinea exacta):
  SUBHEADER_GC_PRIVATE_SHOP_ADD_ENTITY = 0
  SUBHEADER_GC_PRIVATE_SHOP_DEL_ENTITY
  SUBHEADER_GC_PRIVATE_SHOP_TITLE
  SUBHEADER_GC_PRIVATE_SHOP_LOAD
  SUBHEADER_GC_PRIVATE_SHOP_SET_ITEM
  SUBHEADER_GC_PRIVATE_SHOP_BALANCE_UPDATE
  SUBHEADER_GC_PRIVATE_SHOP_OPEN_PANEL
  SUBHEADER_GC_PRIVATE_SHOP_CLOSE_PANEL
  SUBHEADER_GC_PRIVATE_SHOP_CLOSE
  SUBHEADER_GC_PRIVATE_SHOP_START
  SUBHEADER_GC_PRIVATE_SHOP_END
  SUBHEADER_GC_PRIVATE_SHOP_REMOVE_ITEM
  SUBHEADER_GC_PRIVATE_SHOP_REMOVE_MY_ITEM
  SUBHEADER_GC_PRIVATE_SHOP_ADD_ITEM
  SUBHEADER_GC_PRIVATE_SHOP_STATE_UPDATE
  SUBHEADER_GC_PRIVATE_SHOP_WITHDRAW
  SUBHEADER_GC_PRIVATE_SHOP_ITEM_PRICE_CHANGE
  SUBHEADER_GC_PRIVATE_SHOP_ITEM_MOVE
  SUBHEADER_GC_PRIVATE_SHOP_TITLE_CHANGE

enum EPrivateShopCGSubheader (in ordinea exacta):
  SUBHEADER_CG_PRIVATE_SHOP_BUILD = 0
  SUBHEADER_CG_PRIVATE_SHOP_CLOSE
  SUBHEADER_CG_PRIVATE_SHOP_PANEL_OPEN
  SUBHEADER_CG_PRIVATE_SHOP_PANEL_CLOSE
  SUBHEADER_CG_PRIVATE_SHOP_START
  SUBHEADER_CG_PRIVATE_SHOP_END
  SUBHEADER_CG_PRIVATE_SHOP_BUY
  SUBHEADER_CG_PRIVATE_SHOP_WITHDRAW
  SUBHEADER_CG_PRIVATE_SHOP_MODIFY
  SUBHEADER_CG_PRIVATE_SHOP_STATE_UPDATE
  SUBHEADER_CG_PRIVATE_SHOP_ITEM_PRICE_CHANGE
  SUBHEADER_CG_PRIVATE_SHOP_ITEM_MOVE
  SUBHEADER_CG_PRIVATE_SHOP_ITEM_CHECKIN
  SUBHEADER_CG_PRIVATE_SHOP_ITEM_CHECKOUT
  SUBHEADER_CG_PRIVATE_SHOP_TITLE_CHANGE

Constante grid:
  PRIVATE_SHOP_WIDTH = 5
  PRIVATE_SHOP_HEIGHT = 8
  PRIVATE_SHOP_PAGE_MAX_NUM = 2
  PRIVATE_SHOP_PAGE_ITEM_MAX_NUM = 40
  PRIVATE_SHOP_HOST_ITEM_MAX_NUM = 80
  TITLE_MAX_LEN = 32
  TITLE_MIN_LEN = 1
  SHOP_SIGN_MAX_LEN = 32
  ITEM_SOCKET_MAX_NUM = 3
  ITEM_ATTRIBUTE_MAX_NUM = 7

TItemPrice {
  long long llGold;  // SINGURUL camp. NU adauga dwCheque.
}
```

## Faze (executa in ordine, oprire dupa fiecare daca apar erori)

### C1 — Define + Locale_inc

**Fisier:** `ClientVS22/source/UserInterface/Locale_inc.h`

- Adauga `#define WJ_PREMIUM_PRIVATE_SHOP` (mirror la `service.h:13` server)
- NU adauga `WJ_PRIVATE_SHOP_CHEQUE` (cheque-ul e exclus)

### C2 — Packet.h client mirror

**Fisier:** `ClientVS22/source/UserInterface/Packet.h`

Adauga in sectiunea `#pragma pack(push, 1)`:
- `HEADER_CG_PRIVATE_SHOP = 230`, `HEADER_GC_PRIVATE_SHOP = 230` (atentie la duplicate cu alte 230 deja existente — daca exista, REZOLVA conflictul; pluginul se asuma 230 liber)
- Enum `EPrivateShopGCSubheader` complet (vezi mai sus)
- Enum `EPrivateShopCGSubheader` complet (vezi mai sus)
- Constante grid (`PRIVATE_SHOP_WIDTH=5` etc.)
- Structuri OBLIGATORII (binary-identice cu `metin2-server/src/server/game/src/packet.h` liniile ~2473-2600):
  - `TPrivateShopItem` (TItemPos TPos + TItemPrice TPrice)
  - `TPrivateShopItemData` (dwVnum, TPrice, tCheckin, dwCount, wPos, alSockets[3], aAttr[7])
  - `TPacketCGPrivateShop` (bHeader, bSubHeader)
  - `TPacketCGPrivateShopBuild` (szTitle[33], dwPolyVnum, bTitleType, bPageCount, wItemCount)
  - `TPacketCGPrivateShopItemPriceChange` (wPos, TPrice)
  - `TPacketCGPrivateShopItemMove` (wPos, wChangePos)
  - `TPacketCGPrivateShopItemCheckin` (TItemPos TSrcPos, llGold, iDstPos)
  - `TPacketCGPrivateShopItemCheckout` (wSrcPos, iDstPos)
  - `TPacketGCPrivateShop` (bHeader, wSize, bSubHeader)
  - `TPacketGCPrivateShopAddEntity` (lX, lY, lZ, dwVID, dwVnum, szName[25], bTitleType, szTitle[33])
  - `TPacketGCPrivateShopDelEntity` (dwVID)
  - `TPacketGCPrivateShopTitle` (dwVID, bTitleType, szTitle[33])
  - `TPacketGCPrivateShopLoad` (szTitle[33], llGold, lX, lY, bChannel, bState, bPageCount) — **NU dwCheque**
  - `TPacketGCPrivateShopOpen` (szTitle[33], aItems[80] de tip TPrivateShopItemData)
  - `TPacketGCPrivateStateUpdate` (bState, bIsMainPlayerPrivateShop)
  - `TPacketGCPrivateShopItemPriceChange` (wPos, TPrice)
  - `TPacketGCPrivateShopItemMove` (wPos, wChangePos)
  - `TPacketGCPrivateShopBalanceUpdate` (TPrice)

**EXCLUDE:** orice struct `TPacket*Search*`, `TPrivateShopSearchData`, `TPrivateShopSearchFilter`. Nu copia de la plugin.

### C3 — Network stream handler

**Fisiere:**
- `ClientVS22/source/UserInterface/PythonNetworkStream.h` — declara `bool RecvPrivateShopPacket()`, `void SendPrivateShop*()` per subheader CG
- `ClientVS22/source/UserInterface/PythonNetworkStream.cpp` — implementeaza Send-urile (Build, Close, PanelOpen, PanelClose, Start, End, Buy, Withdraw, Modify, ItemPriceChange, ItemMove, ItemCheckin, ItemCheckout, TitleChange) — **15 send functions**
- `ClientVS22/source/UserInterface/PythonNetworkStreamPhaseGame.cpp` — in `RecvGamePhase()` switch adauga `case HEADER_GC_PRIVATE_SHOP: RecvPrivateShopPacket(); break;`
  - In `RecvPrivateShopPacket()`: switch pe subheader → 19 cazuri, fiecare apeleaza un callback Python (`PyCallClassMemberFunc(self, "OnPrivateShop_<Name>", ...)`) sau direct `CPythonPrivateShopManager::Instance().<Method>()`

**Referinta:** `Yeni Sonitex Basic Offline Shop\Source\Client\UserInterface\PythonNetworkStream*.cpp` — sterge tot ce e Search.

### C4 — Manager + module Python (NEW C++ files)

**Fisiere noi (copiaza din plugin, sterge search):**
- `ClientVS22/source/UserInterface/PythonPrivateShop.h`
- `ClientVS22/source/UserInterface/PythonPrivateShop.cpp`
- `ClientVS22/source/UserInterface/PythonPrivateShopManager.cpp`
- `ClientVS22/source/UserInterface/PythonPrivateShopModule.cpp` — registreaza modulul `privateshop` in Python (functii: BuildShop, CloseShop, OpenPanel, ClosePanel, StartShopping, EndShopping, BuyItem, Withdraw, RequestModify, ChangePrice, MoveItem, AddItem, RemoveItem, ChangeTitle + getters: GetShopTitle, GetShopGold, GetShopItem(pos), GetShopItemCount, GetState etc.)

**EXCLUDE:** functii Search* / SearchItem / FilterItem.

**Adauga in:** `ClientVS22/source/UserInterface/UserInterface.cpp` — apel `initprivateshop()` in `initpython()` la initializare module; in solution Properties/file list, asigura ca cele 3 fisiere noi sunt in proiectul VS22 (`UserInterface.vcxproj`).

### C5 — Patch existing C++

- `ClientVS22/source/UserInterface/PythonPlayer.h/cpp` — adauga `m_pkPrivateShop`, `SetPrivateShop()`, `GetPrivateShop()`, `IsPrivateShopOwner()`, `GetMyPrivateShopGold()` etc. Mirror dupa plugin.
- `ClientVS22/source/UserInterface/PythonItemModule.cpp` — daca pluginul adauga functii (UseItem 50200 hook etc.), copiaza-le.
- `ClientVS22/source/UserInterface/PythonPlayerInputMouse.cpp` — daca offline shop entity (vnum 30000-30008) trebuie sa raspunda la click pentru `StartShopping`, adauga handler.
- `ClientVS22/source/UserInterface/PythonCharacterModule.cpp` / `PythonCharacterManagerModule.cpp` — entity types pentru polymorph offline shop.
- `ClientVS22/source/UserInterface/PythonTextTail.cpp/h` — text tail deasupra entity-ului offline shop (titlul shop-ului).
- `ClientVS22/source/GameLib/ItemManager.h/cpp` — daca pluginul are modificari (ex: GetUsedItemTable cu offline-shop fake item), copiaza.
- `ClientVS22/source/EterLib/GrpImageInstance.h/cpp` — patch din plugin (probabil pentru rendering UI specific).
- `ClientVS22/source/EterPythonLib/PythonWindow.h/cpp` + `PythonWindowManagerModule.cpp` — daca pluginul adauga widget nou (probabil pentru grid 5×8). Copiaza diff-ul.
- `ClientVS22/source/ScriptLib/PythonUtils.h/cpp` — utilitare (ex: format number cu separator).

### C6 — Python root scripts

**Path:** UNDE sunt fisierele `.py` ale clientului (`uiinventory.py` etc.) in proiectul tau? In repo-ul `metin2-client` NU exista — sunt impachetate in `.epk` files. **Pasi:**

1. Identifica fisierele `.epk` care contin `root.epk` / `uiscript.epk` / `locale_*.epk` (cauta in `binary/pack/` sau `pack/` sau langa `Metin2Release.exe`).
2. Despacheteaza-le cu un tool potrivit (`Eternexus`, `Lemur` extractor — userul va sti instrumentul).
3. Patch-uri (mirror cu `Yeni Sonitex Basic Offline Shop\Client\root\`):
   - `root/constinfo.py` — adauga `OFFLINE_SHOP_*` constants
   - `root/game.py` — handler open/close panel events, key bindings (daca e cazul)
   - `root/grid.py` — daca pluginul defineste grid container nou (5×8 cu page tabs)
   - `root/interfacemodule.py` — register `OnPrivateShop_*` callback methods (mapeaza la Send/Recv din C4)
   - `root/localeinfo.py` — daca adauga string keys
   - `root/playersettingmodule.py` — settings Persisten (ex: ultimul titlu shop)
   - `root/ui.py` — widget factory daca e adaugat ceva (rar)
   - `root/uicommon.py` — common dialog hooks (price input, title input)
   - `root/uiinventory.py` — drag-drop intre inventar si offline shop panel (handler `OnTopWindow` cand e deschis panel)
   - `root/uitooltip.py` — tooltip pentru item in offline shop (afiseaza pretul + checkin time)
   - `root/uiprivateshop.py` — **fisier NOU** copiat din plugin, sterge tot ce e search
   - `root/uiaffectshower.py`, `root/uidragonsoul.py`, `root/uigameoption.py` — daca pluginul are diff-uri (probabil minore), copiaza
4. **EXCLUDE complet:**
   - `root/uiprivateshopsearch.py` — NU copia
   - tot ce e legat de premium private shop UI (cards, buttons separate)

5. Repacheteaza in `.epk` cu acelasi tool.

### C7 — Python uiscript

Mirror cu `Yeni Sonitex Basic Offline Shop\Client\uiscript\`:

- `uiscript/privateshopwindow.py` — NEW (layout fereastra principala: panel, grid 2 pagini × 5×8, butoane state, gold display, withdraw)
- **EXCLUDE:** `uiscript/privateshopsearchwindow.py`, `uiscript/gameoptiondialog.py` (daca contine doar setari search)

### C8 — Locale

Mirror cu `Yeni Sonitex Basic Offline Shop\Client\locale\`:

- `locale/<empire>/locale_game.txt` — append chei noi (`PRIVATESHOP_*`)
- `locale/<empire>/locale_interface.txt` — append etichete UI
- `locale/<empire>/item_list.txt` — append item 50200 daca lipseste sau patch nume
- `locale/<empire>/item_desc.txt` — append descriere item 50200

**Limbi:** **doar `EN` si `RO`** in Faza 1 (per `metin2-server/CLAUDE.md` plan). Restul limbilor raman ne-traduse — poti pune key-ul ca fallback.

**EXCLUDE:** orice cheie `PRIVATESHOP_SEARCH_*`, `PREMIUM_PRIVATESHOP_*`.

### C9 — Assets

Mirror cu `Yeni Sonitex Basic Offline Shop\Client\patch_premium_private_shop\` MAI PUTIN:

- `icon/item/` — copiaza ICON-ul pentru item 50200 daca difera (probabil identic, skip)
- `ymir work/ui/` — copiaza tga/dds-urile pentru fereastra offline shop (background panel, butoane state)
- `ymir work/effect/` — daca exista efect spawn shop, copiaza
- **EXCLUDE complet:** `patch_premium_private_shop/` daca exista subfolder-e dedicate premium UI (verifica numele tga-urilor; daca toate sunt prefix `private_shop_` fara `premium`, copiaza-le; daca prefix `premium_`, skip)

Plaseaza-le in pack-urile corespunzatoare ale clientului tau (`ymir_work.epk`, `icon.epk`).

### C10 — Build VS22

```
ClientVS22/client.sln  →  Open in Visual Studio 2022
Build configuration: Release | Win32 (asa cum e configurat)
Build → Rebuild Solution
```

Output: `ClientVS22/binary/Metin2Release.exe`

Fix orice eroare de compilare aparuta. Erori comune:
- structuri din `Packet.h` cu padding diferit fata de server → adauga `#pragma pack(push, 1)` corect
- `Locale_inc.h` define lipsa → adauga `#define WJ_PREMIUM_PRIVATE_SHOP`
- include circular Python ↔ PrivateShop → forward declare `class CPythonPrivateShop`

### C11 — Repack + deploy + test

1. Repacheteaza root.epk + uiscript.epk + locale_<empire>.epk + ymir_work.epk + icon.epk cu tool-ul folosit.
2. Copiaza `Metin2Release.exe` + `.epk`-urile in folderul de instalare al clientului local.
3. Ruleaza clientul, conecteaza-te la `192.168.184.131` (FreeBSD-ul cu serverul integrat).
4. **Test in-game:**
   - Intra cu un caracter pe map 1 (sau 21 / 41) — singurele harti unde `CanBuildPrivateShop()` returneaza true (per `private_shop_util.cpp:14-23` server)
   - Adauga in inventar item 50200 (`/give 50200 1` ca GM)
   - Dublu-click 50200 → trebuie sa se deschida `OpenPrivateShopPanel`
   - Pune item-uri in grid, seteaza preturi, seteaza titlu, click "Build/Open Shop"
   - Verifica spawn entity polymorph (vnum 30000-30008) cu titlul tau
   - Cu alt caracter, click pe entity → deschide viewer shop → cumpara item
   - Inchide shop, withdraw gold, verifica balance update
5. **Loguri server in paralel** (FreeBSD):
   ```sh
   tail -f /usr/metin2/server/db/syserr /usr/metin2/server/channel1/game1/syserr
   ```
   Cauta `PRIVATESHOP_GAME:` / `PRIVATESHOP_DB:` evenimente. Orice `sys_err` cu acest prefix = bug.

## Decizii anti-cheque (oglinda fata de server)

In TOATE fisierele unde pluginul are `#ifdef WJ_PRIVATE_SHOP_CHEQUE`, ELIMINA blocul. NU defini `WJ_PRIVATE_SHOP_CHEQUE` nicaieri. Pretul are doar `llGold`.

## Decizii anti-search

Skip TOTAL:
- toate functiile/metodele cu `Search`, `SearchItem`, `FilterItem`, `OpenSearch`, `CloseSearch`
- subheaderele `SUBHEADER_*_SEARCH_*`
- packetele `TPacket*Search*`
- fisierele `uiprivateshopsearch.py`, `privateshopsearchwindow.py`
- diff-urile pentru `gameoptiondialog.py` daca sunt doar pentru setari search

## Decizii anti-premium UI (Faza 1)

- NU adauga handler pentru itemul `71221` (Premium Bundle)
- NU folosi `OpenPrivateShopPanel` cu `dwPolyVnum > 30000` sau `bTitleType > 0` din UI client (limiteaza UI-ul la valori default 30000 + 0)
- NU copia folderul `patch_premium_private_shop/` daca contine asset-uri exclusiv premium

## Reguli de executie

1. **Citeste plugin-ul intai.** Pentru fiecare faza, citeste fisierele plugin corespunzatoare INAINTE sa scrii cod — formatul "//ARA :", "//ALTINA EKLE :" indica unde se insereaza in fisierul existent.
2. **Compileaza dupa fiecare faza C1-C5.** Nu acumula erori.
3. **Loguri minimale.** `Tracef("PRIVATESHOP_CLIENT: ...")` doar in handlere de network nu in UI.
4. **NU da commit pana cand testul in joc nu functioneaza** (per feedback memory `feedback_no_commit_before_test.md` din proiect server — aplica si aici).
5. **Comunicare:** Romana. Per CLAUDE.md global.
6. **Branch:** `main`.

## Ordine recomandata pentru sesiune Claude

1. Faza C1 + C2 — define + Packet.h mirror (rapid)
2. Faza C3 — network stream (Send + Recv switch fara handler logic)
3. Faza C4 — module + manager (compileaza minimal)
4. **Build VS22** — verifica ca compileaza inainte sa atingi Python
5. Faza C5 — patch existing C++ (PythonPlayer etc.)
6. **Build VS22** din nou
7. Faza C6 + C7 + C8 + C9 — Python + locale + assets
8. **Repack + deploy**
9. Faza C11 — test in-game

## Definition of Done

- [ ] Cient compileaza fara erori sau warnings noi in VS22 Release Win32
- [ ] Pack-urile root/uiscript/locale repachetate cu .py noi
- [ ] Conectare la server FreeBSD reuseste, intrare in joc
- [ ] Item 50200 deschide panel; pun item, set price (Yang), set title, deschide shop
- [ ] Spawn entity polymorph apare cu titlul shop-ului in lume
- [ ] Alt caracter cumpara item → tranzactie OK, balance update vizibil
- [ ] Withdraw gold → caracterul primeste yang
- [ ] Server syserr fara erori `PRIVATESHOP_*`
- [ ] Migratia DB ramane consistenta (verifica `SELECT * FROM player.private_shop;`)

## Output asteptat de la sesiunea Claude

La final, sesiunea client trebuie sa raporteze:
1. Lista fisierelor modificate (relative la `metin2-client/`)
2. Lista fisierelor noi adaugate
3. Eventuale .epk-uri care trebuie repachetate manual de user
4. Comenzi de build VS22 (probabil GUI doar)
5. Pasi de instalare in folderul client local
6. Daca apar erori la build sau in joc, pause si cere clarificari userului
