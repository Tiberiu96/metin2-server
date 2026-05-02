# Plan integrare — Sonitex Basic Offline Shop + Search

Sursa: `C:\Users\skema\Desktop\Yeni Sonitex Basic Offline Shop\`
Thread: metin2.forum/system-shop-offline-sonitex-t2259.html
Define master: `WJ_PREMIUM_PRIVATE_SHOP` (+ `WJ_PRIVATE_SHOP_CHEQUE`)

## 1. Ce face pachetul

**Offline Private Shop** — jucatorul deschide magazin personal care ramane in joc dupa disconnect:
- Entitate spawnata (`CEntity : ENTITY_PRIVATE_SHOP`) pe mapa/canal, sectree-managed
- 2 pagini x 5x8 = 80 sloturi
- 2 monede: Yang (gold) + Won (cheque)
- Premium 7 zile (`premium_privateshop_expire` pe account) — fara premium, magazin nu deschide
- Respawn automat la startup din DB
- Titlu personalizabil, 3-32 caractere

**Search Chat** — fereastra cauta items in toate magazinele active (all channels):
- Filtrare pe: vnum, type/subtype, level, refine, job, pret, nume, vanzator
- Click "Buy" redirecteaza jucatorul pe canalul/porttul shopului (cross-channel buy)

**Tabele DB noi (player):**
- `private_shop` (owner_id PK, state enum CLOSED/OPEN/MODIFY, title, vnum, x/y/map/channel/port, gold/cheque, premium_time)
- `private_shop_item` (id AUTO_INCREMENT=70000046, owner_id, pos, count, vnum, price, checkin, sockets 0-2, attrs 0-6)
- ALTER `account.account` → `premium_privateshop_expire DATETIME`

## 2. Fisiere — volume

| Parte | Fisiere noi | Linii noi | Modificari |
|---|---|---|---|
| Server/Game | 6 (private_shop.{cpp,h}, private_shop_manager.{cpp,h}, private_shop_util.{cpp,h}) | ~2060 | char*.cpp, db.cpp, desc_client.cpp, input_*.cpp (5), item.{cpp,h}, main.cpp, p2p.{cpp,h}, packet_info.cpp, questlua_pc.cpp, Makefile |
| Server/Db | 2 (PrivateShop.{cpp,h}) + PrivateShopUtils.h + ItemIDRangeManager.cpp | ~480 | Cache.{cpp,h}, ClientManager*.cpp, LoginData.{cpp,h}, Peer.{cpp,h}, Makefile |
| Server/Common | 0 | 0 | length.h, service.h, tables.h (packet binary — **rebuild total**) |
| Client/UserInterface | 3 (PythonPrivateShop.{cpp,h}, PythonPrivateShopManager.cpp, PythonPrivateShopModule.cpp) | ~1400 | ~22 fisiere cpp/h |
| Client/root | 3 (uiprivateshop.py, uiprivateshopsearch.py, playersettingmodule?) | - | 13 py (constinfo, game, grid, interfacemodule, localeInfo, ui, uicommon, uidragonsoul, uigameoption, uiinventory, uitooltip, uiaffectshower) |
| Client/uiscript | 3 (privateshopwindow.py, privateshopsearchwindow.py, gameoptiondialog.py) | - | - |
| Client/locale | 4 txt | - | locale_game/interface + item_desc/list (turca → trad RO/EN/15 limbi) |
| Client/patch | DDS/TGA/SUB + icon item + effect | - | copiere in pack client |

**Total:** ~4000 linii cod C++ nou + ~30 fisiere modificate.

## 3. Dependente noi in proiectul actual (LIPSESC)

Verificat — nu exista nicio urma in `src/server/game/src/`:
- `class CPrivateShop`, `CPrivateShopManager`, `ENTITY_PRIVATE_SHOP`
- `HEADER_GC_PRIVATE_SHOP`, `HEADER_GD_PRIVATE_SHOP` (141 G→D, 182 D→G)
- `SUBHEADER_GC_PRIVATE_SHOP_*` (~20 subheadere in packet.h)
- `CItem::BindPrivateShop/GetPrivateShop/SetGoldPrice/SetChequePrice/SetSkipSave`
- `CCharacter::SetViewingPrivateShop/GetViewingPrivateShop/RemovePrivateShopItem/CopyDragonSoulItemGrid`
- `CanBuildPrivateShop(ch)`, `CheckTradeWindows(ch)`, `GetEmptyInventory(ch,item)` — utilitare
- `TPlayerItemAttribute`, `TPlayerItem`, `TItemPos` — unele exista, altele de extins
- **Cheque/Won system** — zero — impactul cel mai mare (vezi sectiunea 4)

## 4. Riscuri critice (in ordine descrescatoare)

### R1. Cheque/Won — scope creep majora
Macroul `WJ_PRIVATE_SHOP_CHEQUE` introduce **a doua moneda virtuala** peste toata economia:
- `CCharacter::PointChange` trebuie sa suporte `POINT_CHEQUE`
- Quest/shop/exchange/bank/drop/log — toate trebuie sa stie de cheque
- Client: tooltip, HUD, inventory balance display, chat messages
- DB: coloana `cheque` in `player` si `account`? Sursa nu clarifica unde tine soldul curent al jucatorului — doar shop.gold/cheque.

**Propunere:** in pasul 1 integrez **numai Yang** (dezactivez `WJ_PRIVATE_SHOP_CHEQUE`). Adaug cheque doar daca il vrei explicit, intr-un branch separat.

### R2. Packet binary compatibility
Modificari in `common/tables.h` (`TPacketLoginOnSetup` adauga 3 campuri) + subheadere noi → binarele game/db/client trebuie rebuilded simultan. Orice uitare = jucatori kickati sau freeze.

### R3. Headers 141/182 deja folosite?
Verificat: `HEADER_GD_REQUEST_CHANNELSTATUS=140`, `HEADER_DG_RESPOND_CHANNELSTATUS=181`. 141/182 par libere, dar trebuie verificat intregul range in `service.h` inainte de assign.

### R4. ItemID range 70M+
`private_shop_item.id AUTO_INCREMENT = 70000046`. Daca `player.item.id` a depasit 70M, apare conflict. Nevoie de `ItemIDRangeManager` care aloca un range dedicat private_shop (face parte din pachet — de integrat in db).

### R5. Cross-channel buy — complexitate p2p
Shop-ul are `channel` + `port`. Search clickBuy face handoff jucatorul pe alt channel. Risc: pierderi tranzactii la timeout, items duplicate daca fail-ul e la mijloc. Necesita testare exhaustiva multi-channel.

### R6. Locale turca → RO/EN/15 limbi
~60 stringuri in `locale_interface.txt` + ~60 in `locale_game.txt` — toate in turca. Trebuie traduse complet inainte de a face live. Per fiecare limba: `locale_string_xx.txt` actualizat.

### R7. Encoding EUC-KR coruptie
Pachetul e UTF-8. Daca il merge-uim direct in fisiere existente cu EUC-KR (ex. `item.cpp`, `char.cpp`) — coruptie sigura pe comentarii coreene. Regula CLAUDE.md aplica: editam pe FreeBSD sau cu `perl -i` care pastreaza encoding.

### R8. MyISAM engine
`private_shop*` e MyISAM — nu InnoDB. Pierdere ACID, nu corupe doar shopul in caz de crash. Eu as propune InnoDB. Validam cu tine.

### R9. Makefile dependencies
Fisiere .cpp noi → `gmake dep` obligatoriu. Daca lipseste, compileaza dar linker rateaza.

### R10. 32-bit FreeBSD + time_t
`tPremiumTime time_t` trimis prin packet DB↔game. time_t=32-bit pe FreeBSD 32b → sigur OK pana 2038, dar atentie: client x86 Windows interpreteaza la fel (DWORD).

## 5. Plan in 10 faze (secvential)

### Faza 0 — pregatire (0.5 zi)
- [ ] Backup DB complet (mysqldump player + account + common)
- [ ] Backup `src/` complet (tarball sau branch git)
- [ ] Creat branch `feature/private-shop-sonitex`
- [ ] Decizie: cheque **DA/NU** (recomand NU in prima faza)
- [ ] Decizie: MyISAM vs InnoDB pentru tabele noi

### Faza 1 — schema DB (0.5 zi)
- [ ] `sql/migrations/002_private_shop.sql` (idempotent, IF NOT EXISTS)
- [ ] Convertit AUTO_INCREMENT 70000046 din hardcode → ALTER based on MAX(item.id)+safety_gap
- [ ] Aplicat pe MariaDB dev
- [ ] Verificat engine + collation (latin1_swedish_ci ca restul sau utf8mb4 pentru titles?)

### Faza 2 — common/ (0.5 zi)
- [ ] Adaug in `length.h`: `EPrivateShop`, `EShopSearchMode`, `EPrivateShopState`
- [ ] Adaug in `service.h`: `HEADER_GD_PRIVATE_SHOP=141`, `HEADER_DG_PRIVATE_SHOP=182`, `TPacketLoginOnSetup` extins
- [ ] Adaug in `tables.h`: `TPrivateShop`, `TItemPrice`, `TPlayerPrivateShopItem`, subheadere + packete GD/DG
- [ ] Verificat nicio colistie packet header
- [ ] `#define WJ_PREMIUM_PRIVATE_SHOP` singur (fara CHEQUE)

### Faza 3 — DB side (1 zi)
- [ ] Copiat `PrivateShop.{cpp,h}`, `PrivateShopUtils.h`, `ItemIDRangeManager.cpp`
- [ ] Patch-uri in `Cache`, `ClientManager*`, `LoginData`, `Peer` conform sursei
- [ ] Integrat QID header in `QID.h` (daca modific enum-ul, nu break existing)
- [ ] Adaugat in Makefile
- [ ] `gmake dep && gmake -j9` pe FreeBSD
- [ ] Pornit db binar, verificat syserr curat

### Faza 4 — game side server (2 zile)
- [ ] Copiat `private_shop*.{cpp,h}` in `src/server/game/src/`
- [ ] Adaugat ENTITY_PRIVATE_SHOP in `entity.h` enum
- [ ] Adaugat `BindPrivateShop/GetPrivateShop/SetGoldPrice/SetChequePrice/SetSkipSave` in `item.{cpp,h}`
- [ ] Adaugat `SetViewingPrivateShop/GetViewingPrivateShop/RemovePrivateShopItem/CopyDragonSoulItemGrid` in `char.{cpp,h}`
- [ ] Patch-uri in `char.cpp, char_battle.cpp, char_item.cpp, db.cpp, desc_client.cpp, main.cpp, p2p.{cpp,h}, packet_info.cpp, questlua_pc.cpp`
- [ ] Adaugat handlere `input_*.cpp` pentru packete CG shop
- [ ] Adaugat in Makefile
- [ ] `gmake dep && gmake -j9`
- [ ] Pornit un channel, verificat syserr curat, shop entity nu se spawneaza spontan

### Faza 5 — client side cpp (2 zile)
- [ ] Copiat PythonPrivateShop*.{cpp,h} in `Source/Client/UserInterface/`
- [ ] Patch-uri in PythonNetworkStreamPhaseGame, PythonNetworkStreamModule, PythonPlayer*, PythonItemModule, PythonSystem*, PythonTextTail, PythonApplication, PythonCharacterModule, InstanceBase.h, Packet.h, Locale_inc.h (enable define)
- [ ] Modificari EterLib/GrpImageInstance (pentru title-uri)
- [ ] Modificari GameLib/ItemManager (sync item data)
- [ ] Modificari ScriptLib/PythonUtils
- [ ] Build VS2022 Release, verificat zero warning-uri linker
- [ ] Test: client porneste, nu crasheaza la login

### Faza 6 — client side python (1 zi)
- [ ] Copiat 3 .py in `root/` noi + 3 .py in `uiscript/` noi
- [ ] Patch pe 13 fisiere `.py` existente (diff line-by-line din sursa)
- [ ] Integrat in pack client (packer + encrypt daca aplica)
- [ ] Test: fara erori syntax, UI nu strica

### Faza 7 — assets grafice (0.5 zi)
- [ ] Copiat `patch_premium_private_shop/` in pack client (`ymir work/ui/game/premium_private_shop/`, `ymir work/ui/privatesearch/`)
- [ ] Copiat icon item in `icon/item/` client
- [ ] Copiat effect etc in `effect/` client
- [ ] Test: texturi se incarca fara pink placeholder

### Faza 8 — locale + proto (1 zi)
- [ ] Tradus `locale_game.txt` + `locale_interface.txt` din turca in EN (baseline)
- [ ] Adaugate string-urile in `locale_string.txt` (EN→EN identity) + `locale_string_ro.txt`
- [ ] Replicat pe toate cele 15 limbi (cel putin RO/EN/DE/TR obligatoriu)
- [ ] Merge-uit `item_names.txt`, `item_proto.txt`, `item_list.txt`, `item_desc.txt` — atentie: item_proto trebuie mergedat, nu suprascris!

### Faza 9 — testing (2 zile)
**Golden path:**
- [ ] Premium time acordat, deschis shop cu items, logout, shop ramane, alt char cumpara, reconnect → gold primit
- [ ] Search din alt canal → click buy → handoff channel → tranzactie completa
- [ ] Close shop, items reintra in inventar

**Edge cases:**
- [ ] Inventar plin la close → items raman in shop, mesaj "no space"
- [ ] Premium expirat → shop despawned automat
- [ ] Kick tranzactie mid-flight (kill channel) → rollback items
- [ ] 2 cumparatori simultan acelasi item → unul esueaza clean
- [ ] Item cu socket activ/cooldown → nu se poate pune in shop
- [ ] DragonSoul items → sloturi DS inventory, nu regular
- [ ] Cheque pus dezactivat → nu apare UI won

**Regresii:**
- [ ] NPC shop normal inca functional (CShop)
- [ ] Exchange player-player inca functional
- [ ] Auction inca functional (daca e activ)
- [ ] Item.id-urile noi (normal drops) nu intra in range 70M+

### Faza 10 — deploy + commit (0.5 zi)
- [ ] Commit per faza (2-3, 4-5 mari, 6-9 mici) — **dupa ce testul trece**
- [ ] Migration aplicat pe prod db
- [ ] Deploy binare game/db pe FreeBSD
- [ ] Deploy client patch pe patcher
- [ ] Monitor syserr 24h

**Total estimat:** 10-12 zile lucratoare (un singur dev, cu bug-fix buffer inclus).

## 6. Decizii finalizate

| # | Intrebare | Decizie |
|---|---|---|
| 1 | Moneda | Yang only, plafon 2.1mld (DWORD cap wallet player). `WJ_PRIVATE_SHOP_CHEQUE` rămâne `#undef`. |
| 2 | Engine DB | MyISAM — consistent cu restul schemei (72/72 tabele = MyISAM). Fara `START TRANSACTION`. |
| 3 | Premium time | Item din itemshop (vnum TBD, ex 71084), right-click activeaza premium 7 zile via quest `.quest` care updateaza `account.premium_privateshop_expire = NOW() + 7 DAY`. |
| 4 | GM command | Skip — `/item 71084 1` (existing) suficient pentru support/testing. |
| 5 | Scope | Split pe 2 deploy-uri: **Faza 1** = offline shop only, stabilizare 1-2 saptamani, **Faza 2** = search chat addon. |
| 6 | Limbi | RO + EN complet in Faza 1. Celelalte 13 limbi → fallback pe EN (literal in client UI; LC_TEXT fallback identitate pe server). Traducere completa dupa stabilizare. |

## 7. Implicatii deciziilor in scope

**Din plan se ELIMINA / simplifica (Faza 1):**
- [ ] Tot codul `#ifdef WJ_PRIVATE_SHOP_CHEQUE` — ramane compilat fara
- [ ] `CPrivateShopManager::AddSearchItem / RemoveSearchItem / SearchItem`
- [ ] `TTypeItemMap`, `TSubTypeItemMap`, `TItemList` in manager — reduc la ~60% din `private_shop_manager.cpp`
- [ ] `TPrivateShopSearchFilter`, `TPrivateShopSearchData` in tables.h
- [ ] Packete DG/GD search-related
- [ ] Client: `PythonPrivateShopManager.cpp` (526 linii), `uiprivateshopsearch.py`, `uiscript/privateshopsearchwindow.py`
- [ ] Client patch: folder `privatesearch/` din `ymir work/ui/`
- [ ] Cross-channel buy handoff (se pastreaza doar local-channel buy)
- [ ] Traducere 13 limbi (raman fallback pe EN)

**Ramane in Faza 1 (~60% din pachet):**
- Shop entity spawnable + 2 pagini × 5×8 sloturi
- Premium via item + quest
- CheckIn/CheckOut items from inventory
- Buy (local channel only)
- Price change, move item, title change
- Balance withdraw
- Respawn la startup din DB
- State CLOSED/OPEN/MODIFY
- Close shop → transfer items inapoi in inventar

**Efort revizuit Faza 1:**
- Server C++: ~1200 linii noi (vs 2060 full bundle)
- Client C++: ~900 linii noi (vs 1400)
- Client Python: 2 py noi (vs 3)
- Timp estimat: **7-8 zile** (vs 10-12 full bundle)

## 8. Urmatorul pas — Faza 0 (0.5 zi)

- [ ] Backup MariaDB: `mysqldump -u root -p player account common log > backup_pre_shop_$(date +%F).sql`
- [ ] Branch git: `git checkout -b feature/private-shop-sonitex`
- [ ] Verificat vnum `71084` liber in `player.item_proto` (sau ales alt vnum)
- [ ] Creat folder local de work: `src/server/game/src/` pregatit pentru fisiere noi
- [ ] Decizia TBD ramane: vnum item premium (default 71084?), numele item in game ("Premium Shop 7 zile"?)
