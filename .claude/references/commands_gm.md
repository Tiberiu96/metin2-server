# GM Commands — metin2-server

Sursa: `src/server/game/src/cmd.cpp`

## Niveluri GM

| Nivel | Constant | Descriere |
|-------|----------|-----------|
| 0 | `GM_PLAYER` | Jucator normal |
| 1 | `GM_LOW_WIZARD` | GM basic |
| 2 | `GM_WIZARD` | GM mediu |
| 3 | `GM_HIGH_WIZARD` | GM avansat |
| 4 | `GM_GOD` | Admin |
| 5 | `GM_IMPLEMENTOR` | Developer |

Setat in `common.gmlist` coloana `mAuthority`.

---

## Evenimente

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `eventflag` | `/eventflag <name> <value>` | HIGH_WIZARD | Seteaza un event flag global (pid=0 in player.quest) |
| `geteventflag` | `/geteventflag` | LOW_WIZARD | Afiseaza toate event flagurile active |
| `weeklyevent` | `/weeklyevent` | LOW_WIZARD | Gestioneaza evenimentele saptamanale |
| `eventhelper` | `/eventhelper` | HIGH_WIZARD | Helper pentru evenimente |
| `eclipse` | `/eclipse` | HIGH_WIZARD | Porneste/opreste eclipsa |
| `xmas_boom` | `/xmas_boom` | HIGH_WIZARD | Efect explozii Craciun |
| `xmas_snow` | `/xmas_snow` | HIGH_WIZARD | Ninsoare Craciun |
| `xmas_santa` | `/xmas_santa` | HIGH_WIZARD | Santa Craciun |

---

## Teleport / Miscare

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `goto` | `/goto <x> <y>` | LOW_WIZARD | Teleport la coordonate (metri) pe harta curenta |
| `goto` | `/goto #<map_index>` | LOW_WIZARD | Teleport pe alta harta |
| `goto` | `/goto #<map_index> <empire>` | LOW_WIZARD | Teleport pe harta + imperiu |
| `warp` | `/warp <x> <y>` | LOW_WIZARD | Teleport la coordonate absolute |
| `transfer` | `/transfer <player_name>` | LOW_WIZARD | Teleporteaza alt jucator la tine |

---

## Monstri / Mobs

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `mob` | `/mob <vnum>` | HIGH_WIZARD | Spawneaza un mob la pozitia ta |
| `mob_ld` | `/mob_ld <vnum> <x> <y> <dir>` | HIGH_WIZARD | Spawneaza mob la coordonate specifice |
| `ma` | `/ma <vnum>` | HIGH_WIZARD | Spawneaza mob agresiv |
| `mc` | `/mc <vnum>` | HIGH_WIZARD | Spawneaza mob laș (coward) |
| `mm` | `/mm <vnum>` | HIGH_WIZARD | Spawneaza mob pe toata harta |
| `kill` | `/kill` | HIGH_WIZARD | Omoara mob-ul targetat |
| `purge` | `/purge` | WIZARD | Sterge toti mob-ii de pe harta |
| `ipurge` | `/ipurge` | HIGH_WIZARD | Sterge un item de pe jos |
| `respawn` | `/respawn` | WIZARD | Respawneaza mob-ii pe harta |
| `aggregate` | `/aggregate` | LOW_WIZARD | Aduna toti mob-ii la tine |
| `attract_ranger` | `/attract_ranger` | LOW_WIZARD | Atrage mob-ii in raza |
| `pull_monster` | `/pull_monster` | LOW_WIZARD | Trage un mob la tine |
| `weaken` | `/weaken` | GOD | Slabeste mob-ul targetat |
| `forgetme` | `/forgetme` | LOW_WIZARD | Mob-ii te uita (ies din agro) |

---

## Iteme

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `item` | `/item <vnum> [count]` | GOD | Da un item jucatorului curent |
| `socketitem` | `/socketitem <vnum>` | IMPLEMENTOR | Creaza item cu sockete |
| `change_attr` | `/change_attr` | IMPLEMENTOR | Schimba atributele unui item |
| `add_attr` | `/add_attr` | IMPLEMENTOR | Adauga atribute unui item |
| `add_socket` | `/add_socket` | IMPLEMENTOR | Adauga socket unui item |
| `polyitem` | `/polyitem <vnum>` | HIGH_WIZARD | Polymorph item |

---

## Jucator / Character

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `level` | `/level <value>` | LOW_WIZARD | Seteaza levelul jucatorului |
| `stat` | `/stat` | PLAYER | Creste un stat |
| `stat-` | `/stat-` | PLAYER | Scade un stat |
| `stat_reset` | `/stat_reset` | LOW_WIZARD | Reseteaza statele |
| `advance` | `/advance <job> <level>` | GOD | Schimba clasa/levelul |
| `reset` | `/reset` | HIGH_WIZARD | Reseteaza jucatorul |
| `greset` | `/greset` | HIGH_WIZARD | Reseteaza grupul |
| `setskill` | `/setskill <skill_id> <level>` | LOW_WIZARD | Seteaza un skill |
| `setskillother` | `/setskillother <player> <skill_id> <level>` | HIGH_WIZARD | Seteaza skill la alt jucator |
| `setskillpoint` | `/setskillpoint <value>` | IMPLEMENTOR | Seteaza puncte skill |
| `setjob` | `/setjob <group>` | IMPLEMENTOR | Seteaza grupa de skill |
| `reset_subskill` | `/reset_subskill` | HIGH_WIZARD | Reseteaza sub-skilluri |
| `state` | `/state` | LOW_WIZARD | Afiseaza starea jucatorului |
| `polymorph` | `/polymorph <vnum>` | LOW_WIZARD | Transforma jucatorul |
| `invisible` | `/invisible` | LOW_WIZARD | Toggle invizibilitate |
| `stun` | `/stun` | LOW_WIZARD | Stuneaza jucatorul |
| `slow` | `/slow` | LOW_WIZARD | Incetineste jucatorul |
| `dc` | `/dc <player_name>` | LOW_WIZARD | Deconecteaza un jucator |
| `user` | `/user` | HIGH_WIZARD | Info despre utilizatori conectati |
| `who` | `/who` | IMPLEMENTOR | Lista jucatori online |
| `inventory` | `/inventory` | LOW_WIZARD | Afiseaza inventarul |
| `view_equip` | `/view_equip <player_name>` | PLAYER | Vede echipamentul unui jucator |
| `con+` | `/con+ <value>` | LOW_WIZARD | Creste CON |
| `int+` | `/int+ <value>` | LOW_WIZARD | Creste INT |
| `str+` | `/str+ <value>` | LOW_WIZARD | Creste STR |
| `dex+` | `/dex+ <value>` | LOW_WIZARD | Creste DEX |

---

## Quest Flags

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `getqf` | `/getqf <quest_name> <flag_name>` | LOW_WIZARD | Citeste o variabila de quest |
| `setqf` | `/setqf <quest_name> <flag_name> <value>` | LOW_WIZARD | Seteaza o variabila de quest |
| `delqf` | `/delqf <quest_name>` | LOW_WIZARD | Sterge un quest flag |
| `qf` | `/qf` | IMPLEMENTOR | Debug quest flags |
| `clear_quest` | `/clear_quest <quest_name>` | HIGH_WIZARD | Reseteaza un quest complet |
| `set_state` | `/set_state <quest_name> <state>` | LOW_WIZARD | Seteaza starea unui quest |

---

## Gilda

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `makeguild` | `/makeguild <name>` | HIGH_WIZARD | Creaza o gilda |
| `deleteguild` | `/deleteguild <name>` | HIGH_WIZARD | Sterge o gilda |
| `gwlist` | `/gwlist` | LOW_WIZARD | Lista razboaie de gilda |
| `gwstop` | `/gwstop <guild1> <guild2>` | LOW_WIZARD | Opreste razboi gilda |
| `gwcancel` | `/gwcancel` | LOW_WIZARD | Anuleaza razboi gilda |
| `gstate` | `/gstate <guild>` | LOW_WIZARD | Starea unei gilde |
| `gskillup` | `/gskillup` | PLAYER | Creste skill gilda |

---

## Cal (Horse)

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `horse_state` | `/horse_state` | HIGH_WIZARD | Starea calului |
| `horse_level` | `/horse_level <level>` | HIGH_WIZARD | Seteaza levelul calului |
| `horse_ride` | `/horse_ride` | HIGH_WIZARD | Incaleca |
| `horse_summon` | `/horse_summon` | HIGH_WIZARD | Cheama calul |
| `horse_unsummon` | `/horse_unsummon` | HIGH_WIZARD | Trimite calul inapoi |
| `horse_set_stat` | `/horse_set_stat <stat> <value>` | HIGH_WIZARD | Seteaza stat cal |
| `mount` | `/mount <vnum>` | PLAYER | Montura |
| `unmount` | `/unmount` | PLAYER | Jos de pe montura |

---

## Monarh

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `elect` | `/elect` | HIGH_WIZARD | Alege monarh |
| `rmcandidacy` | `/rmcandidacy` | LOW_WIZARD | Elimina candidatura |
| `setmonarch` | `/setmonarch <player>` | LOW_WIZARD | Seteaza monarh |
| `rmmonarch` | `/rmmonarch` | LOW_WIZARD | Elimina monarh |
| `mto` | `/mto <player>` | PLAYER | Monarh warp to |
| `mtr` | `/mtr <player>` | PLAYER | Monarh transfer |
| `minfo` | `/minfo` | PLAYER | Info monarh |
| `mtax` | `/mtax <value>` | PLAYER | Taxa monarh |
| `mmob` | `/mmob <vnum>` | PLAYER | Monarh spawneaza mob |
| `mnotice` | `/mnotice <message>` | PLAYER | Mesaj monarh |
| `check_mmoney` | `/check_mmoney` | IMPLEMENTOR | Verifica banii monarhului |

---

## Arena / Duel

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `duel` | `/duel <player1> <player2>` | LOW_WIZARD | Porneste duel |
| `end_duel` | `/end_duel` | LOW_WIZARD | Termina duel |
| `end_all_duel` | `/end_all_duel` | LOW_WIZARD | Termina toate duelurile |
| `show_arena_list` | `/show_arena_list` | LOW_WIZARD | Lista arena |

---

## OX Event

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `show_quiz` | `/show_quiz` | LOW_WIZARD | Afiseaza intrebarea curenta OX |
| `log_oxevent` | `/log_oxevent` | LOW_WIZARD | Log OX event |
| `get_oxevent_att` | `/get_oxevent_att` | LOW_WIZARD | Numarul de participanti OX |

---

## Privilegii Imperiu

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `priv_empire` | `/priv_empire <empire> <type> <value> <duration>` | HIGH_WIZARD | Bonus pentru imperiu |
| `priv_guild` | `/priv_guild <guild> <type> <value> <duration>` | HIGH_WIZARD | Bonus pentru gilda |

---

## Sistem / Server

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `shutdown` | `/shutdown` | HIGH_WIZARD | Opreste serverul |
| `reload` | `/reload` | IMPLEMENTOR | Reincarca configuratia |
| `flush` | `/flush` | IMPLEMENTOR | Flush date |
| `notice` | `/notice <message>` | HIGH_WIZARD | Mesaj global (toti jucatorii) |
| `notice_map` | `/notice_map <message>` | LOW_WIZARD | Mesaj pe harta curenta |
| `big_notice` | `/big_notice <message>` | HIGH_WIZARD | Mesaj mare global |
| `console` | `/console` | LOW_WIZARD | Consola debug |
| `detaillog` | `/detaillog` | LOW_WIZARD | Logging detaliat |
| `monsterlog` | `/monsterlog` | LOW_WIZARD | Log monstri |
| `cooltime` | `/cooltime` | HIGH_WIZARD | Reseteaza cooldown-uri |
| `effect` | `/effect <id>` | LOW_WIZARD | Afiseaza un efect vizual |
| `build` | `/build` | PLAYER | Construieste |
| `clear_land` | `/clear_land` | HIGH_WIZARD | Curata terenul |
| `siege` | `/siege` | LOW_WIZARD | Asediu |
| `frog` | `/frog` | HIGH_WIZARD | Test frog |
| `temp` | `/temp` | IMPLEMENTOR | Comanda temporara test |

---

## Diverse Jucator

| Comanda | Sintaxa | Nivel | Descriere |
|---------|---------|-------|-----------|
| `hair` | `/hair <style>` | PLAYER | Schimba parul |
| `safebox` | `/safebox <size>` | HIGH_WIZARD | Seteaza marimea seifului |
| `war` | `/war <guild1> <guild2>` | PLAYER | Declara razboi |
| `nowar` | `/nowar <guild1> <guild2>` | PLAYER | Opreste razboi |
| `pkmode` | `/pkmode` | PLAYER | Toggle PK mode |
| `block_chat` | `/block_chat <player>` | PLAYER | Blocheaza chat |
| `jy` | `/jy <player>` | HIGH_WIZARD | Block chat (alias) |
| `affect_remove` | `/affect_remove <type>` | LOW_WIZARD | Sterge un efect de pe jucator |
| `group` | `/group <vnum>` | HIGH_WIZARD | Spawneaza un grup de monstri |
| `grrandom` | `/grrandom` | HIGH_WIZARD | Spawneaza grup random |
| `book` | `/book <skill_id>` | IMPLEMENTOR | Da o carte de skill |
| `gift` | `/gift` | PLAYER | Trimite cadou |
| `set` | `/set <attribute> <value>` | IMPLEMENTOR | Seteaza atribute jucator |

---

## Note importante

- Comenzile fara `/` in fata nu functioneaza — **obligatoriu slash**
- Nivelul GM se seteaza in `common.gmlist` coloana `mAuthority`
- `GM_PLAYER` = disponibil tuturor, nu necesita GM
- Comenzile cu `GM_IMPLEMENTOR` sunt doar pentru dezvoltatori
- `/mob <vnum>` = comanda corecta pentru spawn mob (nu `/spawn`)
