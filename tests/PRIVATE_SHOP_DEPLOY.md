# Private shop: instalare si verificare

## Comportamentul dupa corectie

- Crearea pastreaza datele itemelor in asteptarea DB. Daca proprietarul se deconecteaza, raspunsul DB finalizeaza shopul offline sau pastreaza inventarul la refuz.
- Logoutul, inchiderea clientului si pierderea conexiunii detaseaza sesiunea. La transfer intre core-uri, starea offline este provizorie pana la reasocierea sesiunii destinatie.
- Relogarea incarca itemele si soldul din starea autoritara DB/cache, inclusiv la player-cache hit, si opreste evenimentul offline.
- Fallbackul fara premium ramane **300 de secunde**, ca in configuratia de test existenta. Termenul este acum trimis spre persistare, nu doar setat in memorie.
- La ultima vanzare dispare entitatea, dar soldul ramane in shop. La expirare sunt pastrate si itemele nevandute; nu se muta nimic automat in safebox.
- La relogare, stocul inchis/expirat revine in mod MODIFY pentru recuperare prin panoul **U**. Recuperarea pastreaza ID-urile, socketurile si atributele itemelor.
- Soldul se retrage din panou sau prin **/shop_collect**. Comanda permite transe pana la limita de Yang a personajului; restul ramane in shop. Este utila cand validarea veche din client refuza soldul total.
- Dupa golirea inventarului shopului si a soldului, shopul este eliminat. Pana atunci, randul persistent reprezinta bunuri recuperabile, nu un shop activ fantoma.
- Rezervarile de cumparare au identificator. Dupa 30 de secunde DB cere anulare confirmata pe conexiunea game; un raspuns vechi nu elibereaza o rezervare noua. Daca peer-ul a disparut, rezervarea se elibereaza.

## Ce era gresit

Despawnul stergea obiectul DB, iar apelantul il reutiliza. Expirarea trimitea SQL trunchiat si stergea apoi sursa itemelor. Soldurile erau trimise intr-un safebox fara recuperare functionala si puteau fi plafonate cu pierdere. Loginul din cache si handle-urile vechi desincronizau proprietarul. Crearea pastra pointeri spre inventarul personajului, iar cumpararile intrerupte lasau rezervari blocate.

## Pasii pe FreeBSD

Operatiunile de mai jos se executa manual. Nu au fost executate prin SSH de Codex.

1. Opreste toate procesele Metin2 din consola FreeBSD:

```sh
cd /usr/metin2/server && sh close.sh
```

Asteapta terminarea lor si verifica oprirea inclusiv a DB. Nu continua cu procese game vechi ramase active.

2. Fa un backup al bazelor si al binarelor `db`/`game` existente. Pentru bazele MyISAM, dupa oprirea proceselor:

```sh
mysqldump -u root -p --lock-all-tables --databases account player common log > /root/private-shop-before-fix.sql
```

Foloseste un nume nou daca fisierul exista deja. Pastreaza separat copiile binarelor din `/usr/metin2/server/share/bin/`.

3. Sincronizeaza manual continutul local `metin2-server/src/` in `/usr/metin2/src/`, incluzand fisierele noi ale integrarii shop. Nu suprascrie CONFIG-urile runtime sau credentialele. Corectia nu cere o migratie SQL noua; presupune schema integrarii existente (`002_private_shop.sql` si `003_private_shop_bundle_expire.sql`) deja instalata.

4. Recompileaza complet DB, deoarece s-au schimbat pachete interne game-DB:

```sh
cd /usr/metin2/src/server/db/src
gmake clean && gmake dep && gmake -j9
```

5. Recompileaza complet game:

```sh
cd /usr/metin2/src/server/game/src
gmake clean && gmake dep && gmake -j9
```

Makefile-urile muta binarele in `/usr/metin2/server/share/bin/`. Daca oricare compilare esueaza, pastreaza serverul oprit si trimite prima eroare completa. Nu porni cu DB nou si game vechi sau invers.

6. Porneste numai dupa ce ambele compilari reusesc:

```sh
cd /usr/metin2/server && sh start.sh
```

Nu este necesara recompilarea clientului sau repack pentru aceasta corectie.

7. Foloseste doua conturi de test si iteme fara valoare pentru scenariile de mai jos.

| Scenariu | Rezultat asteptat |
| --- | --- |
| Creeaza shop, logout si relogare rapida | U afiseaza stocul si soldul corect; administrarea functioneaza. |
| Schimba canalul si cumpara de pe celalalt cont | Proprietarul primeste actualizarea vanzarii pe sesiunea noua. |
| Inchide complet clientul proprietarului | Shopul ramane offline si respecta termenul. |
| Cumpara ultimul item cu proprietarul offline | Entitatea dispare; DB ramane activ; soldul este recuperabil la relogare. |
| Lasa sa expire cele 300 s fara premium | Entitatea dispare; la relogare stocul apare in MODIFY, cu soldul pastrat. |
| Retrage itemele nevandute si banii | Niciun item duplicat/pierdut; dupa recuperare completa se poate crea alt shop. |
| Foloseste /shop_collect cu sold peste spatiul disponibil de Yang | Se crediteaza numai cat incape; restul ramane in shop. Elibereaza Yang si repeta dupa minimum 10 s. |
| Inchide clientul imediat dupa confirmarea crearii | Shopul acceptat apare offline; la refuz itemele raman in inventarul persistent. |
| Deconecteaza cumparatorul in timpul cererii | Itemul nefinalizat poate fi cumparat ulterior, dupa anularea confirmata. |
| Restart controlat in interiorul celor 300 s | Shopul offline reapare pana la termenul acordat. |
| Restart dupa expirare si dupa retragere completa | Nu reapar vanzari active, bani retrasi sau iteme deja recuperate. |

Testele de cursa pot necesita repetare; o singura incercare fara defect nu confirma acoperirea ferestrei de timp.

8. Verifica logurile direct in directoarele proceselor:

```sh
tail -n 100 /usr/metin2/server/db/syserr
tail -n 100 /usr/metin2/server/channel1/game1/syserr
grep 'PRIVATESHOP_DB:' /usr/metin2/server/db/syslog | tail -n 60
grep 'PRIVATESHOP_GAME:' /usr/metin2/server/channel1/game1/syslog | tail -n 60
```

Repeta pentru canalul/core-ul folosit. Markerii utili sunt `owner_rebound`, `offline_deadline_saved`, `expired_recovery_retained`, `sold_out_balance_retained`, `withdrawal_confirmed`, `reservation_timeout_cancel` si `build_completed`.

## Verificari locale efectuate

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_private_shop_lifecycle.ps1
```

Doua suite C++ compileaza si executa functii extrase direct din sursele modificate, cu peer-uri/cache/ceas simulate: ciclul DB si crearea in asteptare pe game. Au trecut. Nu inlocuiesc compilarea completa FreeBSD, testele SQL reale sau verificarea in joc. Nu a fost executata scriere, compilare ori repornire prin SSH.

## Fisiere modificate in aceasta corectie

- `src/server/common/tables.h`
- `src/server/db/src/ClientManagerPrivateShop.cpp`
- `src/server/db/src/ClientManagerPlayer.cpp`
- `src/server/db/src/PrivateShop.cpp`
- `src/server/db/src/PrivateShop.h`
- `src/server/game/src/char.cpp`
- `src/server/game/src/cmd.cpp`
- `src/server/game/src/input_db.cpp`
- `src/server/game/src/input_main.cpp`
- `src/server/game/src/private_shop_manager.cpp`
- `src/server/game/src/private_shop_manager.h`
- `tests/private_shop_lifecycle.cpp`
- `tests/private_shop_pending_build.cpp`
- `tests/run_private_shop_lifecycle.ps1`
- `tests/PRIVATE_SHOP_DEPLOY.md`

Nu s-au modificat `metin2-client/` sau asseturi din `ClientIgnition/Eternexus/`.
