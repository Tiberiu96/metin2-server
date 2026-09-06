# Premium bundle 71221

- Sold at shops that already sell bundle 50200 (General Store, NPC 9003 in the base schema).
- Base price: 500000 Yang for one item. Existing foreign-empire shop pricing still applies.
- Reusable: creating a shop does not consume 71221. Its existing seven-day REAL_TIME limit is unchanged.
- Failed creation does not refund a second reusable bundle, online or offline.
- Bundle 50200 remains consumable. U and /shop_collect remain available for recovery without another purchase.

## Manual deployment

1. Stop game and DB, and back up the current sources, runtime proto and database.
2. Sync src/server/game/src/char.cpp and private_shop_manager.cpp to the matching FreeBSD source directory.
3. Sync server/share/conf/item_proto.txt to /usr/metin2/server/share/conf/item_proto.txt.
4. Sync sql/migrations/004_premium_bundle_npc_price.sql to /usr/metin2/sql/migrations/.
5. Run mysql -u root -p player < /usr/metin2/sql/migrations/004_premium_bundle_npc_price.sql.
6. In /usr/metin2/src/server/game/src run gmake clean && gmake dep && gmake -j9. Start the server only after success.

No client executable or pack update is required for this change. Tests are not runtime files.

## Checks

- Buy 71221 at the General Store in the character's own empire: exactly 500000 Yang deducted.
- Open a premium shop: the same bundle ID, count and expiry remain in inventory.
- Sell the last item, collect earnings, and create another shop using the same bundle.
- Repeat failed creation online and during disconnect: no additional bundle is created.
- Apply the migration twice: no duplicate shop offer is added.
- Restart DB: the NPC price remains 500000.

Local harness: powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_private_shop_lifecycle.ps1.
Full FreeBSD compilation, SQL execution and in-game checks must follow deployment.
