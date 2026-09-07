# Shop cu termen fix

Durata pentru shopuri NOI: 120 secunde, in `src/server/common/private_shop_lifetime.h`.
Pentru productie: seteaza `PRIVATE_SHOP_LIFETIME_SECONDS = 48 * 60 * 60`, recompileaza DB.
Shopurile existente isi pastreaza termenul; configuratia noua nu le reinnoieste.
Durata itemului 71221 ramane separata (7 zile). Nu modifica item_proto pentru durata shopului.

## Deploy coordonat (manual)

1. Opreste toate procesele game/auth/DB si fa backup bazei player, inclusiv private_shop/private_shop_item.
2. Sincronizeaza `src/server/common/`, `src/server/db/src/`, `src/server/game/src/` si migratia noua.
3. Aplica pe FreeBSD: `mysql -u root -p player < /usr/metin2/sql/migrations/005_private_shop_lifetime.sql`.
4. Compileaza DB: `cd /usr/metin2/src/server/db/src && gmake clean && gmake dep && gmake -j9`.
5. Compileaza game: `cd /usr/metin2/src/server/game/src && gmake clean && gmake dep && gmake -j9`.
6. Client: foloseste noul `ClientVS22/binary/zgamecore.exe`; copiaza-l in client cu jocul inchis.
7. Repack MANUAL pentru `ClientIgnition/Eternexus/root/` (uiprivateshop.py); distribuie root.epk/eix impreuna cu executabilul.
8. Reporneste serverul numai dupa migratie si instalarea ambelor binare. Nu combina versiuni de protocol vechi/noi.

Migratia adauga starea RECOVERY si lifetime_seconds, fara stergerea itemelor/soldului.
Un rollback necesita restaurarea coordonata a binarelor si schemei/backupului, nu doar schimbarea executabilului.

## Verificare

- Shop nou: 02:00; verde 120..73 secunde, albastru 72..18, rosu 17..0.
- Proprietar online: la zero modelul si vizitatorii dispar; panoul permite recuperarea, nu redeschiderea.
- Proprietar offline, relogare si schimbare CH: acelasi termen, fara timp suplimentar.
- Restart inainte si dupa termen: nu reapare public un shop expirat.
- Cumparare la expirare: cererile noi sunt refuzate; rezervarea acceptata se finalizeaza/anuleaza fara stergerea sursei.
- Sold si iteme ramase: retrage prin 71221; inventarul plin nu trebuie sa piarda iteme. Dupa golire, creeaza alt shop.
- Vanzarea ultimului item inchide shopul mai devreme; soldul ramane recuperabil.
- Coloana lifetime_seconds pastreaza procentul corect dupa restart/modificarea configuratiei.

Teste locale: `tests/run_private_shop_lifecycle.ps1` si `tests/run_private_shop_timer.ps1`.
Timerul foloseste text numeric si cheia de expirare deja tradusa; nu s-au modificat packurile locale.
Verificarea automata UI este fara randare; aspectul final si fluxurile FreeBSD trebuie validate in joc.

## Curatare

Eliminate lista veche de evenimente premium offline, typedeful aferent, metodele End/IsPremiumEvent
si Set/UpdatePremiumTime din obiectul DB, devenite nefolosite. Clasele shop ramase sunt referentiate.
Nu au fost sterse fisiere intregi doar pe baza numelui sau absentei unui apel direct.
