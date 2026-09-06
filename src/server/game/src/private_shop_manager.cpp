#include "stdafx.h"
#include "private_shop_manager.h"
#include "private_shop.h"
#include "private_shop_util.h"
#include "char.h"
#include "char_manager.h"
#include "desc_client.h"
#include "mob_manager.h"
#include "config.h"
#include "db.h"
#include "item_manager.h"
#include "item.h"
#include "utils.h"
#include "entity.h"
#include "sectree_manager.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "desc_manager.h"
#include "p2p.h"
#include "DragonSoul.h"
#include "log.h"

void CPrivateShopManager::Destroy()
{
	m_map_privateShop.clear();
}

LPPRIVATE_SHOP CPrivateShopManager::CreatePrivateShop(DWORD dwPID)
{
	sys_log(0, "PRIVATESHOP_GAME: create_shop_begin pid=%u current_count=%u vid_count=%u",
		dwPID, static_cast<unsigned>(m_map_privateShop.size()), m_dwVIDCount);

	if (GetPrivateShop(dwPID))
	{
		sys_err("PRIVATESHOP_GAME: create_shop_fail pid=%u reason=already_exists", dwPID);
		return nullptr;
	}

	std::unique_ptr<CPrivateShop> upPrivateShop = std::make_unique<CPrivateShop>();
	LPPRIVATE_SHOP pPrivateShop = upPrivateShop.get();
	if (!pPrivateShop)
	{
		sys_err("PRIVATESHOP_GAME: create_shop_fail pid=%u reason=alloc_null", dwPID);
		return nullptr;
	}
	sys_log(0, "PRIVATESHOP_GAME: create_shop_alloc_ok pid=%u ptr=%p", dwPID, pPrivateShop);

	DWORD dwVID = AllocVID();
	upPrivateShop->SetVID(dwVID);
	sys_log(0, "PRIVATESHOP_GAME: create_shop_vid_ok pid=%u vid=%u", dwPID, dwVID);

	m_map_privateShop.emplace(dwPID, std::move(upPrivateShop));
	sys_log(0, "PRIVATESHOP_GAME: create_shop_owner_map_ok pid=%u count=%u", dwPID, static_cast<unsigned>(m_map_privateShop.size()));

	m_map_privateShopVID.emplace(dwVID, pPrivateShop);
	sys_log(0, "PRIVATESHOP_GAME: create_shop_vid_map_ok pid=%u vid=%u count=%u", dwPID, dwVID, static_cast<unsigned>(m_map_privateShopVID.size()));

	return pPrivateShop;
}

LPPRIVATE_SHOP CPrivateShopManager::GetPrivateShop(DWORD dwPID)
{
	auto it = m_map_privateShop.find(dwPID);
	if (it == m_map_privateShop.end())
		return nullptr;

	return it->second.get();
}

LPPRIVATE_SHOP CPrivateShopManager::GetPrivateShopByOwnerName(const char* c_szOwnerName)
{
	for (auto it = m_map_privateShop.begin(); it != m_map_privateShop.end(); ++it)
	{
		LPPRIVATE_SHOP pPrivateShop = it->second.get();

		if (pPrivateShop->GetOwnerName().compare(c_szOwnerName) == 0)
			return pPrivateShop;
	}

	return nullptr;
}

LPPRIVATE_SHOP CPrivateShopManager::GetPrivateShopByVID(DWORD dwVID)
{
	auto it = m_map_privateShopVID.find(dwVID);
	if (it == m_map_privateShopVID.end())
		return nullptr;

	return it->second;
}

bool CPrivateShopManager::DeletePrivateShop(DWORD dwPID)
{
	auto it = m_map_privateShop.find(dwPID);
	if (it == m_map_privateShop.end())
		return false;

	DWORD dwVID = it->second->GetVID();

	m_map_privateShopVID.erase(dwVID);
	m_map_privateShop.erase(it);
	return true;
}

bool CPrivateShopManager::BuildPrivateShop(LPCHARACTER pShopOwner, const char* c_szTitle, DWORD dwPolyVnum, BYTE bTitleType, BYTE bPageCount, const std::vector<TPrivateShopItem>& c_vec_shopItem)
{
	sys_log(0, "PRIVATESHOP_GAME: manager_build_begin pid=%u name=%s title=%s poly=%u title_type=%u page_count=%u item_count=%u",
		pShopOwner ? pShopOwner->GetPlayerID() : 0, pShopOwner ? pShopOwner->GetName() : "UNKNOWN", c_szTitle ? c_szTitle : "", dwPolyVnum, bTitleType, bPageCount, static_cast<unsigned>(c_vec_shopItem.size()));

	if (!pShopOwner)
	{
		sys_err("PRIVATESHOP_GAME: manager_build_fail pid=0 reason=no_owner");
		return false;
	}

	const CMob* pMobTable = CMobManager::Instance().Get(dwPolyVnum);
	if (!pMobTable)
	{
		sys_err("PRIVATESHOP_GAME: manager_build_fail pid=%u reason=no_mob_proto poly=%u", pShopOwner ? pShopOwner->GetPlayerID() : 0, dwPolyVnum);
		return false;
	}
	sys_log(0, "PRIVATESHOP_GAME: manager_build_mob_ok pid=%u poly=%u", pShopOwner->GetPlayerID(), dwPolyVnum);

	LPPRIVATE_SHOP pPrivateShop = CreatePrivateShop(pShopOwner->GetPlayerID());
	if (!pPrivateShop)
	{
		sys_err("PRIVATESHOP_GAME: manager_build_fail pid=%u reason=create_instance name=%s", pShopOwner->GetPlayerID(), pShopOwner->GetName());
		return false;
	}
	sys_log(0, "PRIVATESHOP_GAME: manager_build_instance_ok pid=%u vid=%u", pShopOwner->GetPlayerID(), pPrivateShop->GetVID());

	std::vector<TPlayerPrivateShopItem> vec_privateShopItem;
	for (const auto& c_rShopItem : c_vec_shopItem)
	{
		LPITEM pItem = pShopOwner->GetItem(c_rShopItem.TPos);
		if (!pItem)
		{
			sys_err("PRIVATESHOP_GAME: manager_build_fail pid=%u reason=item_missing window=%u cell=%u display_pos=%u",
				pShopOwner->GetPlayerID(), c_rShopItem.TPos.window_type, c_rShopItem.TPos.cell, c_rShopItem.wDisplayPos);
			DeletePrivateShop(pShopOwner->GetPlayerID());
			return false;
		}

		if (c_rShopItem.wDisplayPos >= PRIVATE_SHOP_HOST_ITEM_MAX_NUM)
		{
			sys_err("PRIVATESHOP_GAME: manager_build_fail pid=%u reason=display_pos_invalid item_id=%u display_pos=%u max=%u",
				pShopOwner->GetPlayerID(), pItem->GetID(), c_rShopItem.wDisplayPos, PRIVATE_SHOP_HOST_ITEM_MAX_NUM);
			DeletePrivateShop(pShopOwner->GetPlayerID());
			return false;
		}

		sys_log(0, "PRIVATESHOP_GAME: manager_build_item pid=%u item_id=%u vnum=%u inv_window=%u inv_cell=%u display_pos=%u price=%lld cheque=%u",
			pShopOwner->GetPlayerID(), pItem->GetID(), pItem->GetVnum(), c_rShopItem.TPos.window_type, c_rShopItem.TPos.cell,
			c_rShopItem.wDisplayPos, c_rShopItem.TPrice.llGold, c_rShopItem.TPrice.dwCheque);

		pItem->Lock(true);//Darklovers_Fix_Offline_Shop

		TPlayerPrivateShopItem t{};

		CopyItemData(pItem, t);

		t.TPrice.llGold = c_rShopItem.TPrice.llGold;
		t.TPrice.dwCheque = c_rShopItem.TPrice.dwCheque;
		t.wPos = c_rShopItem.wDisplayPos;
		t.tCheckin = time(0);

		vec_privateShopItem.push_back(t);
	}

	std::string strTitle(c_szTitle);

	pPrivateShop->SetID(pShopOwner->GetPlayerID());
	pPrivateShop->SetVnum(dwPolyVnum);
	pPrivateShop->SetOwnerName(pShopOwner->GetName());
	pPrivateShop->SetTitle(c_szTitle);
	pPrivateShop->SetTitleType(bTitleType);
	pPrivateShop->SetState(STATE_OPEN);
	pPrivateShop->SetPageCount(bPageCount);

	// Pending creation owns values only, never pointers into a character inventory.
	std::unique_ptr<CGrid> grid[PRIVATE_SHOP_PAGE_MAX_NUM];
	for (auto& page : grid)
		page = std::make_unique<CGrid>(PRIVATE_SHOP_WIDTH, PRIVATE_SHOP_HEIGHT);
	for (const auto& item : vec_privateShopItem)
	{
		const TItemTable* proto = ITEM_MANAGER::Instance().GetTable(item.dwVnum);
		const WORD page = item.wPos / PRIVATE_SHOP_PAGE_ITEM_MAX_NUM;
		const WORD pos = item.wPos % PRIVATE_SHOP_PAGE_ITEM_MAX_NUM;
		if (!proto || !grid[page]->IsEmpty(pos, 1, proto->bSize))
		{
			sys_err("PRIVATESHOP_GAME: build_invalid_grid pid=%u item=%u pos=%u", pShopOwner->GetPlayerID(), item.dwID, item.wPos);
			DeletePrivateShop(pShopOwner->GetPlayerID());
			return false;
		}
		grid[page]->Put(pos, 1, proto->bSize);
	}
	for (const auto& item : vec_privateShopItem)
	{
		LPITEM original = ITEM_MANAGER::Instance().Find(item.dwID);
		ITEM_MANAGER::Instance().FlushDelayedSave(original);
		original->SetSkipSave(true);
	}

	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_CREATE;
	WORD wCount = vec_privateShopItem.size();

	if (wCount == 0)
	{
		sys_err("PRIVATESHOP_GAME: manager_build_fail pid=%u reason=no_items_after_copy", pShopOwner->GetPlayerID());
		DeletePrivateShop(pShopOwner->GetPlayerID());
		return false;
	}

	TPrivateShop t{};

	t.dwOwner = pShopOwner->GetPlayerID();
	strlcpy(t.szTitle, c_szTitle, sizeof(t.szTitle));
	strlcpy(t.szOwnerName, pShopOwner->GetName(), sizeof(t.szOwnerName));
	t.dwVnum = dwPolyVnum;
	t.bTitleType = bTitleType;
	t.lMapIndex = pShopOwner->GetMapIndex();
	t.lX = pShopOwner->GetX();
	t.lY = pShopOwner->GetY();
	t.bChannel = g_bChannel;
	t.wPort = mother_port;
	t.bState = STATE_OPEN;
	t.llGold = 0;
	t.dwCheque = 0;
	t.bPageCount = bPageCount;
	t.tPremiumTime = pShopOwner->GetPremiumRemainSeconds(PREMIUM_PRIVATE_SHOP) + time(0);

	PendingBuild pending;
	pending.items = vec_privateShopItem;
	pending.ownerHandle = pShopOwner->GetDesc()->GetHandle();
	m_pendingBuilds.emplace(t.dwOwner, std::move(pending));
	sys_log(0, "PRIVATESHOP_GAME: build_pending pid=%u items=%u", t.dwOwner, wCount);
	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, pShopOwner->GetDesc()->GetHandle(), sizeof(BYTE) + sizeof(TPrivateShop) + sizeof(WORD) + sizeof(TPlayerPrivateShopItem) * wCount);
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&t, sizeof(TPrivateShop));
	db_clientdesc->Packet(&wCount, sizeof(WORD));
	db_clientdesc->Packet(&vec_privateShopItem[0], sizeof(TPlayerPrivateShopItem) * wCount);

	sys_log(0, "PRIVATESHOP_GAME: manager_build_sent_db pid=%u handle=%u item_count=%u map=%ld x=%ld y=%ld port=%u",
		pShopOwner->GetPlayerID(), pShopOwner->GetDesc()->GetHandle(), wCount, t.lMapIndex, t.lX, t.lY, t.wPort);

	return true;
}

void CPrivateShopManager::SetPendingBuildBundle(DWORD pid, const TPlayerItem& bundle)
{
	auto it = m_pendingBuilds.find(pid);
	if (it != m_pendingBuilds.end())
	{
		it->second.bundle = bundle;
		sys_log(0, "PRIVATESHOP_GAME: pending_bundle_snapshot pid=%u item=%u", pid, bundle.id);
	}
}

void CPrivateShopManager::PreservePendingBuild(LPCHARACTER owner)
{
	auto it = m_pendingBuilds.find(owner->GetPlayerID());
	if (it == m_pendingBuilds.end())
		return;
	it->second.disconnected = true;
	for (const auto& item : it->second.items)
	{
		LPITEM original = ITEM_MANAGER::Instance().Find(item.dwID);
		if (!original || original->GetOwner() != owner)
			continue;
		original->SetSkipSave(true);
		original->RemoveFromCharacter();
		M2_DESTROY_ITEM(original);
		sys_log(0, "PRIVATESHOP_GAME: pending_item_detached pid=%u item=%u", owner->GetPlayerID(), item.dwID);
	}
}

void CPrivateShopManager::BuildPrivateShopResult(DWORD dwPID, TPrivateShop* table, bool success)
{
	auto it = m_pendingBuilds.find(dwPID);
	if (it == m_pendingBuilds.end())
	{
		sys_err("PRIVATESHOP_GAME: build_result_without_pending pid=%u success=%d", dwPID, success);
		return;
	}
	PendingBuild pending = std::move(it->second);
	m_pendingBuilds.erase(it);
	LPCHARACTER owner = CHARACTER_MANAGER::Instance().FindByPID(dwPID);
	const bool online = !pending.disconnected && owner && owner->GetDesc() && owner->GetDesc()->GetHandle() == pending.ownerHandle;
	if (online)
	{
		owner->ClosePrivateShopPanel(true);
		for (const auto& item : pending.items)
		{
			LPITEM original = ITEM_MANAGER::Instance().Find(item.dwID);
			if (!original || original->GetOwner() != owner)
				continue;
			original->Lock(false);
			original->SetSkipSave(success);
			if (success)
			{
				owner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, original->GetCell(), 255);
				original->RemoveFromCharacter();
				M2_DESTROY_ITEM(original);
			}
		}
	}
	DeletePrivateShop(dwPID);
	if (!success)
	{
		if (online)
		{
			if (pending.bundle.id)
			{
				owner->AutoGiveItem(pending.bundle.vnum);
				sys_log(0, "PRIVATESHOP_GAME: build_bundle_refunded pid=%u bundle_vnum=%u", dwPID, pending.bundle.vnum);
			}
			else
				sys_log(0, "PRIVATESHOP_GAME: build_no_bundle_refund pid=%u reason=not_consumed", dwPID);
			owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot build a personal shop at this moment. ", owner->GetLanguage()));
		}
		else if (pending.bundle.id)
		{
			// Failure leaves inventory rows intact; restore the consumed bundle snapshot.
			db_clientdesc->DBPacket(HEADER_GD_ITEM_SAVE, 0, &pending.bundle, sizeof(pending.bundle));
		}
		sys_log(0, "PRIVATESHOP_GAME: build_failed_inventory_retained pid=%u online=%d", dwPID, online);
		return;
	}
	SpawnPrivateShop(table, pending.items);
	if (online)
	{
		owner->SetPrivateShopTable(*table);
		for (const auto& item : pending.items)
			owner->SetPrivateShopItem(item);
		LogManager::Instance().CharLog(owner, 0, "PRIVATE SHOP BUILT", "");
	}
	else
	{
		BYTE header = PRIVATE_SHOP_GD_SUBHEADER_LOGOUT;
		db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, pending.ownerHandle, sizeof(header) + sizeof(dwPID));
		db_clientdesc->Packet(&header, sizeof(header));
		db_clientdesc->Packet(&dwPID, sizeof(dwPID));
	}
	sys_log(0, "PRIVATESHOP_GAME: build_completed pid=%u online=%d items=%u", dwPID, online, static_cast<unsigned>(pending.items.size()));
}

void CPrivateShopManager::ClosePrivateShop(LPCHARACTER pShopOwner)
{
	DWORD dwPID = pShopOwner->GetPlayerID();

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		pShopOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You are too far away from your personal shop.", pShopOwner->GetLanguage()));
		return;
	}

	// Cancel if the player is not near his shop character
	long lShopMapIndex = pPrivateShop->GetMapIndex();
	if (pShopOwner->GetMapIndex() != lShopMapIndex)
	{
		pShopOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot close a personal shop from a different location.", pShopOwner->GetLanguage()));
		return;
	}

	if (DISTANCE_APPROX(pPrivateShop->GetX() - pShopOwner->GetX(), pPrivateShop->GetY() - pShopOwner->GetY()) > 3000)
	{
		pShopOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You are too far away from your personal shop.", pShopOwner->GetLanguage()));
		return;
	}

	pPrivateShop->SetClosing(true);
	if (pPrivateShop->TransferItems(pShopOwner))
	{
		// Close the window at customer's client
		pPrivateShop->CleanShopViewers();

		BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_DELETE;

		db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(DWORD));
		db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
		db_clientdesc->Packet(&dwPID, sizeof(DWORD));

		DeletePrivateShop(dwPID);

		pShopOwner->ClosePrivateShop();

		sys_log(0, "%s PRIVATE_SHOP: SUCCESS Shop entity closed", pShopOwner->GetName());
		LogManager::Instance().CharLog(pShopOwner, 0, "PRIVATE SHOP CLOSED", "");
	}
	else
	{
		sys_err("Failed to transfer items back to owner %u", pShopOwner->GetPlayerID());
		pPrivateShop->SetClosing(false);
	}
}

void CPrivateShopManager::SpawnPrivateShop(TPrivateShop* pPrivateShopTable, const std::vector<TPlayerPrivateShopItem>& c_vec_shopItem)
{
	const CMob* pMobTable = CMobManager::Instance().Get(pPrivateShopTable->dwVnum);
	if (!pMobTable)
	{
		sys_err("Cannot find mob table for vnum %u", pPrivateShopTable->dwVnum);
		return;
	}

	if (GetPrivateShop(pPrivateShopTable->dwOwner))
		DeletePrivateShop(pPrivateShopTable->dwOwner);

	LPPRIVATE_SHOP pPrivateShop = CreatePrivateShop(pPrivateShopTable->dwOwner);
	if (!pPrivateShop)
	{
		sys_err("Cannot create a private shop instance for player %s %u", pPrivateShopTable->szOwnerName, pPrivateShopTable->dwOwner);
		return;
	}

	LPSECTREE pSectree = SECTREE_MANAGER::instance().Get(pPrivateShopTable->lMapIndex, pPrivateShopTable->lX, pPrivateShopTable->lY);

	if (!pSectree)
	{
		sys_err("Cannot find sectree %dx%d mapindex %d for private shop %s %d", pPrivateShopTable->lY, pPrivateShopTable->lY, pPrivateShopTable->lMapIndex,
			pPrivateShopTable->szOwnerName, pPrivateShopTable->dwOwner);
		return;
	}

	if (pPrivateShopTable->bState < STATE_OPEN)
	{
		sys_err("Cannot spawn a private shop %u with state %u", pPrivateShopTable->dwOwner, pPrivateShopTable->bState);
		return;
	}

	// Bind data to the private shop
	pPrivateShop->SetID(pPrivateShopTable->dwOwner);
	pPrivateShop->SetVnum(pPrivateShopTable->dwVnum);
	pPrivateShop->SetOwnerName(pPrivateShopTable->szOwnerName);
	pPrivateShop->SetTitle(pPrivateShopTable->szTitle);
	pPrivateShop->SetTitleType(pPrivateShopTable->bTitleType);
	pPrivateShop->SetState(pPrivateShopTable->bState);
	pPrivateShop->SetPageCount(pPrivateShopTable->bPageCount);

	// Create items and transfer them to the private shop
	if (!pPrivateShop->Initialize(c_vec_shopItem, true))
	{
		sys_err("Cannot initialize items to private shop");
		DespawnPrivateShop(pPrivateShopTable->dwOwner);

		sys_log(0, "%s PRIVATE_SHOP: FAILURE Shop entity not spawned", pPrivateShopTable->szOwnerName);
		return;
	}

	pPrivateShop->Show(pPrivateShopTable->lX, pPrivateShopTable->lY, 0, pPrivateShopTable->lMapIndex);

	sys_log(0, "%s PRIVATE_SHOP: SUCCESS Shop entity spawned", pPrivateShopTable->szOwnerName);

	LogManager::Instance().CharLog(pPrivateShopTable->dwOwner, pPrivateShopTable->lX, pPrivateShopTable->lY, 0, "PRIVATE SHOP SPAWN", "", "");
}

void CPrivateShopManager::DespawnPrivateShop(DWORD dwPID)
{
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_DESPAWN;
	DWORD dwOwner = dwPID;

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(DWORD));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&dwOwner, sizeof(DWORD));

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		sys_err("Cannot find private shop with id %u", dwPID);
		return;
	}

	sys_log(0, "%s PRIVATE_SHOP: Shop entity despawned", pPrivateShop->GetOwnerName().c_str());
	DeletePrivateShop(dwPID);
}

bool CPrivateShopManager::StopShopping(LPCHARACTER pShopViewer)
{
	LPPRIVATE_SHOP pPrivateShop = pShopViewer->GetViewingPrivateShop();
	if (!pPrivateShop)
		return false;

	pPrivateShop->RemoveShopViewer(pShopViewer);
	pShopViewer->SetMyShopTime();

	return true;
}

void CPrivateShopManager::ItemCheckin(LPCHARACTER pOwner, const TPlayerPrivateShopItem* c_pShopItem)
{
	if (db_clientdesc->GetSocket() == INVALID_SOCKET)
		return;

	LPITEM pItem = ITEM_MANAGER::Instance().Find(c_pShopItem->dwID);
	if (!pItem)
	{
		sys_err("Cannot checkin item %u vnum %u for private shop %u (item not found)", c_pShopItem->dwID, c_pShopItem->dwVnum, pOwner->GetPlayerID());
		return;
	}

	if (!pItem->GetOwner())
	{
		sys_err("Cannot checkin item %u vnum %u for private shop %u (owner not found)", c_pShopItem->dwID, c_pShopItem->dwVnum, pOwner->GetPlayerID());
		return;
	}

	if (pItem->GetOwner() != pOwner)
	{
		sys_err("Cannot checkin item %u vnum %u for private shop %u (owner not equal to private shop owner)", c_pShopItem->dwID, c_pShopItem->dwVnum, pOwner->GetPlayerID());
		return;
	}

	pOwner->SetPrivateShopItem(*c_pShopItem);

	// Send check-in update back to db
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKIN_UPDATE;

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, pOwner->GetDesc()->GetHandle(), sizeof(BYTE) + sizeof(TPlayerPrivateShopItem));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(c_pShopItem, sizeof(TPlayerPrivateShopItem));

	char szHint[128 + 1]{};
	snprintf(szHint, sizeof(szHint), "%s x%u pos %u gold %lld cheque %u",
		pItem->GetName(), pItem->GetCount(), c_pShopItem->wPos, c_pShopItem->TPrice.llGold, c_pShopItem->TPrice.dwCheque);
	LogManager::Instance().ItemLog(pItem->GetOwner(), pItem, "PRIVATE SHOP CHECKIN", szHint);

	// Do not save the removal of item, will be done later on
	pItem->SetSkipSave(true);
	ITEM_MANAGER::Instance().RemoveItem(pItem);
}

void CPrivateShopManager::ItemCheckout(LPCHARACTER pOwner, WORD wSrcPos, TItemPos TDstPos)
{
	if (db_clientdesc->GetSocket() == INVALID_SOCKET)
		return;

	const TPlayerPrivateShopItem* c_pShopItem = pOwner->GetPrivateShopItem(wSrcPos);

	if (!c_pShopItem)
	{
		sys_err("cannot find private shop item at pos %u player %u", wSrcPos, pOwner->GetPlayerID());
		return;
	}

	LPITEM pItem = ITEM_MANAGER::Instance().Find(c_pShopItem->dwID);

	if (!pItem)
	{
		pItem = ITEM_MANAGER::Instance().CreateItem(c_pShopItem->dwVnum, c_pShopItem->dwCount, c_pShopItem->dwID);
		if (!pItem)
		{
			sys_err("cannot create item by vnum %u (id %u)", c_pShopItem->dwVnum, c_pShopItem->dwID);
			return;
		}

		pItem->OnAfterCreatedItem();
		CopyItemData(*c_pShopItem, pItem);
	}

	// Necessary to avoid AddToCharacter failure
	pItem->SetCell(nullptr, 0);

	// Try to move the item to the owner
	if (!pItem->AddToCharacter(pOwner, TItemPos(TDstPos.window_type, TDstPos.cell)))
	{
		sys_err("Cannot checkout item %u vnum %u from private shop %u", c_pShopItem->dwID, c_pShopItem->dwVnum, pOwner->GetPlayerID());

		// If item had no private shop bound it was re-created and can be removed
		if (!pItem->GetPrivateShop())
			M2_DESTROY_ITEM(pItem);
		else
			pItem->SetCell(nullptr, c_pShopItem->wPos); // Revert the cell back :)

		return;
	}

	// Item was successfully transfered to owner, reset private shop values
	pItem->BindPrivateShop(nullptr);
	pItem->SetGoldPrice(0);
	pItem->SetChequePrice(0);

	// Enable saving of the item
	pItem->SetSkipSave(false);

	// Remove the private shop item
	pOwner->RemovePrivateShopItem(wSrcPos);

	// Send check-out update back to db
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKOUT_UPDATE;

	TPlayerItem TItem {};
	CopyItemData(pItem, TItem);

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, pOwner->GetDesc()->GetHandle(), sizeof(BYTE) + sizeof(WORD) + sizeof(TPlayerItem));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&wSrcPos, sizeof(WORD));
	db_clientdesc->Packet(&TItem, sizeof(TPlayerItem));

	char szHint[128 + 1]{};
	snprintf(szHint, sizeof(szHint), "%s x%u", pItem->GetName(), pItem->GetCount());
	LogManager::Instance().ItemLog(pItem->GetOwner(), pItem, "PRIVATE SHOP CHECKOUT", szHint);
}

bool CPrivateShopManager::ItemTransaction(LPCHARACTER pCustomer, TPlayerPrivateShopItem* c_pShopItem, DWORD dwReservation)
{
	if (db_clientdesc->GetSocket() == INVALID_SOCKET)
		return false;

	if (!c_pShopItem->dwOwner)
		return false;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(c_pShopItem->dwOwner);

	// Check if the item already exists, if not create it
	LPITEM pItem = ITEM_MANAGER::Instance().Find(c_pShopItem->dwID);

	if (pPrivateShop && !pItem)
		sys_err("No item data found for item %u vnum %u on private shop %u", c_pShopItem->dwID, c_pShopItem->dwVnum, c_pShopItem->dwOwner);

	bool bNewItem = false;
	if (!pItem)
	{
		pItem = ITEM_MANAGER::Instance().CreateItem(c_pShopItem->dwVnum, c_pShopItem->dwCount, c_pShopItem->dwID);
		if (!pItem)
		{
			sys_err("Cannot create item %u vnum %u", c_pShopItem->dwID, c_pShopItem->dwVnum);

			SendItemTransactionFailedResult(c_pShopItem->dwOwner, c_pShopItem->wPos, dwReservation);
			return false;
		}

		CopyItemData(*c_pShopItem, pItem);
		bNewItem = true;
	}

	// Check if there is enough space for the item
	int iPos = GetEmptyInventory(pCustomer, pItem);

	if (iPos < 0)
	{
		sys_err("Cannot find empty cell for item %u vnum %u", c_pShopItem->dwID, c_pShopItem->dwVnum);

		pCustomer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You don't have enough space in your inventory.", pCustomer->GetLanguage()));
		SendItemTransactionFailedResult(c_pShopItem->dwOwner, c_pShopItem->wPos, dwReservation);

		if (bNewItem)
			M2_DESTROY_ITEM(pItem);

		return false;
	}

	// Was already checked on db?
	if (pCustomer->GetGold() < c_pShopItem->TPrice.llGold)
	{
		sys_log(0, "PRIVATE_SHOP: Insufficient gold at customer %s to buy item %u vnum %u", pCustomer->GetName(), c_pShopItem->dwID, c_pShopItem->dwVnum);

		pCustomer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You don't have enough Yang.", pCustomer->GetLanguage()));
		SendItemTransactionFailedResult(c_pShopItem->dwOwner, c_pShopItem->wPos, dwReservation);

		if (bNewItem)
			M2_DESTROY_ITEM(pItem);

		return false;
	}

	if (c_pShopItem->TPrice.dwCheque != 0)
	{
		sys_err("PRIVATESHOP_GAME: reason=cheque_disabled buyer=%u shop_owner=%u item_id=%u vnum=%u cheque=%u",
			pCustomer->GetPlayerID(), c_pShopItem->dwOwner, c_pShopItem->dwID, c_pShopItem->dwVnum, c_pShopItem->TPrice.dwCheque);

		pCustomer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("This personal shop item has an invalid price.", pCustomer->GetLanguage()));
		SendItemTransactionFailedResult(c_pShopItem->dwOwner, c_pShopItem->wPos, dwReservation);
		return false;
	}

	// Bind data to the item
	pItem->SetSkipSave(true);
	pItem->BindPrivateShop(nullptr); // Break the private shop connection if the item was taken directly from shop (not p2p)

	// Remove private shop data
	pItem->SetGoldPrice(0);
	pItem->SetChequePrice(0);
	pItem->SetCheckinTime(0);

	// Reset item's ownership and cell
	pItem->SetCell(nullptr, 0);

	// Enable saving of the item
	pItem->SetSkipSave(false);

	BYTE bWindow = pItem->GetType() == ITEM_DS ? DRAGON_SOUL_INVENTORY : INVENTORY;
	if (!pItem->AddToCharacter(pCustomer, TItemPos(bWindow, iPos)))
	{
		// Bind private shop to the item if it exists on this core
		if (pPrivateShop)
		{
			CopyItemData(*c_pShopItem, pItem);
			pItem->BindPrivateShop(pPrivateShop);
			pItem->SetSkipSave(true);
		}
		else
		{
			M2_DESTROY_ITEM(pItem);
		}

		sys_log(0, "PRIVATE_SHOP: Failed to add item %u vnum %u to customer %s", c_pShopItem->dwID, c_pShopItem->dwVnum, pCustomer->GetName());
		SendItemTransactionFailedResult(c_pShopItem->dwOwner, c_pShopItem->wPos, dwReservation);
		return false;
	}

	// Remove item from the shop before resetting owner/position (if the shop is in another core, request from db will be sent)
	if (pPrivateShop)
		pPrivateShop->RemoveItem(c_pShopItem->wPos);

	pCustomer->PointChange(POINT_GOLD, -c_pShopItem->TPrice.llGold);

	ITEM_MANAGER::Instance().SaveSingleItem(pItem);

	if (pItem->GetCount() > 1)
		pCustomer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You have bought x%d %s.", pCustomer->GetLanguage()), pItem->GetCount(), ITEM_MANAGER::instance().GetLocaleName(pItem->GetVnum(), pCustomer->GetLanguage()));
	else
		pCustomer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You have bought %s.", pCustomer->GetLanguage()), ITEM_MANAGER::instance().GetLocaleName(pItem->GetVnum(), pCustomer->GetLanguage()));

	sys_log(0, "PRIVATE_SHOP: SUCCESS Item transaction for customer %s item %u %u from shop %u", pCustomer->GetName(), c_pShopItem->dwID, c_pShopItem->dwVnum, c_pShopItem->dwOwner);

	// Send result back to db
	SendItemTransaction(c_pShopItem, pCustomer, dwReservation);

	char szHint[128 + 1]{};
	snprintf(szHint, sizeof(szHint), "%s x%u (Private Shop %u)", pItem->GetName(), pItem->GetCount(), c_pShopItem->dwOwner);
	LogManager::Instance().ItemLog(pItem->GetOwner(), pItem, "PRIVATE SHOP ITEM TRANSACTION", szHint);

	return true;
}

void CPrivateShopManager::SendItemTransaction(const TPlayerPrivateShopItem* c_pShopItem, LPCHARACTER pCustomer, DWORD dwReservation)
{
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_BUY;

	TPacketGDPrivateShopBuy subPacket{};
	subPacket.dwReservation = dwReservation;

	std::memcpy(&subPacket.TItem, c_pShopItem, sizeof(subPacket.TItem));
	strlcpy(subPacket.szCustomerName, pCustomer->GetName(), sizeof(subPacket.szCustomerName));
	subPacket.dwCustomer = pCustomer->GetPlayerID();

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketGDPrivateShopBuy));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&subPacket, sizeof(TPacketGDPrivateShopBuy));
}

void CPrivateShopManager::SendItemTransactionFailedResult(DWORD dwShopID, WORD wPos, DWORD dwReservation)
{
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_FAILED_BUY;

	TPacketGDPrivateShopFailedBuy subPacket{};
	subPacket.dwReservation = dwReservation;

	subPacket.dwShopID = dwShopID;
	subPacket.wPos = wPos;

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketGDPrivateShopFailedBuy));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&subPacket, sizeof(TPacketGDPrivateShopFailedBuy));
}

void CPrivateShopManager::SendItemTransfer(const TPlayerItem* c_pItem)
{
	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_ITEM_TRANSFER;

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPlayerItem));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(c_pItem, sizeof(TPlayerItem));
}

void CPrivateShopManager::SendItemExpire(LPITEM pItem)
{
	LPPRIVATE_SHOP pPrivateShop = pItem->GetPrivateShop();

	BYTE bSubHeader = PRIVATE_SHOP_GD_SUBHEADER_ITEM_EXPIRE;

	TPacketGDPrivateShopItemExpire subPacket{};
	subPacket.dwShopID = pPrivateShop->GetID();
	subPacket.wPos = pItem->GetCell();

	db_clientdesc->DBPacketHeader(HEADER_GD_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketGDPrivateShopItemExpire));
	db_clientdesc->Packet(&bSubHeader, sizeof(BYTE));
	db_clientdesc->Packet(&subPacket, sizeof(TPacketGDPrivateShopItemExpire));

	pPrivateShop->RemoveItem(pItem->GetCell());
}

void CPrivateShopManager::AddSearchItem(LPITEM pItem)
{
	BYTE bItemType = pItem->GetType();
	BYTE bItemSubType = pItem->GetSubType();

	TItemList& itemList = m_map_searchItem[bItemType][bItemSubType];
	itemList.emplace(pItem);
}

void CPrivateShopManager::RemoveSearchItem(LPITEM pItem)
{
	if (!pItem)
		return;

	BYTE bItemType = pItem->GetType();
	BYTE bItemSubType = pItem->GetSubType();

	auto typeIt = m_map_searchItem.find(bItemType);
	if (typeIt == m_map_searchItem.end())
		return;

	TSubTypeItemMap& map_itemSubType = typeIt->second;
	auto subTypeIt = map_itemSubType.find(bItemSubType);
	if (subTypeIt == map_itemSubType.end())
		return;

	TItemList& itemList = subTypeIt->second;

	for (auto it = itemList.begin(); it != itemList.end(); ++it)
	{
		LPITEM pListItem = *it;
		if (pListItem->GetID() == pItem->GetID())
		{
			itemList.erase(it);
			return;
		}
	}
}

bool FilterItem(LPITEM pItem, TPrivateShopSearchFilter& rFilter, bool bUseFilter)
{
	if (!pItem->GetPrivateShop())
	{
		sys_err("Could not find private shop for item %u", pItem->GetID());
		return false;
	}

	if (rFilter.iItemType >= 0)
	{
		if (pItem->GetType() != rFilter.iItemType)
			return false;
	}

	if (rFilter.iItemSubType >= 0)
	{
		if (pItem->GetSubType() != rFilter.iItemSubType)
			return false;
	}

	if (bUseFilter)
	{
		if (rFilter.dwVnum)
		{
			if (pItem->GetVnum() != rFilter.dwVnum)
				return false;
		}

		if (rFilter.iItemType == ITEM_WEAPON || rFilter.iItemType == ITEM_ARMOR)
		{
			if (rFilter.bMinRefine > 0)
			{
				if ((pItem->GetVnum() % 10) < rFilter.bMinRefine) return false;
			}

			if (rFilter.bMaxRefine > 0)
			{
				if ((pItem->GetVnum() % 10) > rFilter.bMaxRefine) return false;
			}
		}

		if (rFilter.iJob >= 0)
		{
			switch (rFilter.iJob)
			{
			case JOB_WARRIOR:
				if (pItem->GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
					return false;
				break;

			case JOB_ASSASSIN:
				if (pItem->GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
					return false;
				break;

			case JOB_SHAMAN:
				if (pItem->GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
					return false;
				break;

			case JOB_SURA:
				if (pItem->GetAntiFlag() & ITEM_ANTIFLAG_SURA)
					return false;

				break;
			}
		}

		if (rFilter.dwMinLevel > 0)
		{
			if (pItem->GetLevelLimit() && static_cast<DWORD>(pItem->GetLevelLimit()) < rFilter.dwMinLevel) return false;
		}

		if (rFilter.dwMaxLevel > 0)
		{
			if (static_cast<DWORD>(pItem->GetLevelLimit()) > rFilter.dwMaxLevel) return false;
		}

		if (rFilter.llMinGold > pItem->GetGoldPrice() ||
			rFilter.llMaxGold < pItem->GetGoldPrice())
		{
			return false;
		}

		if (rFilter.wMinCheque > pItem->GetChequePrice() ||
			rFilter.wMaxCheque < pItem->GetChequePrice())
		{
			return false;
		}
	}

	return true;
}

void CPrivateShopManager::SearchItem(LPDESC pDesc, TPrivateShopSearchFilter& rFilter, bool bUseFilter, DWORD dwCustomerPID /* = 0 */)//Darklovers_Fix_Offline_Shop
{
	TEMP_BUFFER buf {};
	DWORD dwCount = 0;
	const static size_t SAFE_RECV_BUFSIZE = 8192;

//Darklovers_Fix_Offline_Shop
	const auto SendData = [&buf, &dwCount, pDesc, dwCustomerPID]()
	{
		if (dwCustomerPID)
		{
			TPacketGGPrivateShopItemSearchResult mainPacket{};
			mainPacket.bHeader = HEADER_GG_PRIVATE_SHOP_ITEM_SEARCH_RESULT;
			mainPacket.wSize = buf.size();
			mainPacket.dwCustomerID = dwCustomerPID;

			pDesc->BufferedPacket(&mainPacket, sizeof(mainPacket));
			pDesc->LargePacket(buf.read_peek(), buf.size());
		}
		else
		{
			if (!dwCount)
				return;

			TPacketGCPrivateShop mainPacket{};
			mainPacket.bHeader = HEADER_GC_PRIVATE_SHOP;
			mainPacket.wSize = sizeof(TPacketGCPrivateShop) + dwCount * sizeof(TPrivateShopSearchData);
			mainPacket.bSubHeader = SUBHEADER_GC_PRIVATE_SHOP_SEARCH_RESULT;

			pDesc->BufferedPacket(&mainPacket, sizeof(TPacketGCPrivateShop));
			pDesc->LargePacket(buf.read_peek(), buf.size());
		}

		buf.reset();
		dwCount = 0;
	};
//Darklovers_Fix_Offline_Shop

	if (rFilter.dwVnum)
	{
		TItemTable* pItemTable = ITEM_MANAGER::Instance().GetTable(rFilter.dwVnum);
		if (!pItemTable)
			return;

		rFilter.iItemType = pItemTable->bType;
		rFilter.iItemSubType = pItemTable->bSubType;

		auto typeIt = m_map_searchItem.find(rFilter.iItemType);
		if (typeIt == m_map_searchItem.end())
			return;

		TSubTypeItemMap& map_itemSubType = typeIt->second;
		auto subTypeIt = map_itemSubType.find(rFilter.iItemSubType);
		if (subTypeIt == map_itemSubType.end())
			return;

		TItemList& itemList = subTypeIt->second;

		for (const auto& pItem : itemList)
		{
			if (buf.size() + sizeof(TPrivateShopSearchData) > SAFE_RECV_BUFSIZE)
				SendData();//Darklovers_Fix_Offline_Shop

			if (FilterItem(pItem, rFilter, bUseFilter))
			{
				TPrivateShopSearchData item{};
				CopyItemData(pItem, item);

				buf.write(&item, sizeof(TPrivateShopSearchData));//Darklovers_Fix_Offline_Shop
				++dwCount;
			}
		}
	}
	else if (rFilter.iItemType >= 0)
	{
		auto typeIt = m_map_searchItem.find(rFilter.iItemType);
		if (typeIt == m_map_searchItem.end())
			return;

		TSubTypeItemMap& map_itemSubType = typeIt->second;

		// Search by type-subtype
		if (rFilter.iItemSubType >= 0)
		{
			auto subTypeIt = map_itemSubType.find(rFilter.iItemSubType);
			if (subTypeIt == map_itemSubType.end())
				return;

			TItemList& itemList = subTypeIt->second;
			for (const auto& pItem : itemList)
			{
				if (buf.size() + sizeof(TPrivateShopSearchData) > SAFE_RECV_BUFSIZE)
					SendData();//Darklovers_Fix_Offline_Shop

				if (FilterItem(pItem, rFilter, bUseFilter))
				{
					TPrivateShopSearchData item{};
					CopyItemData(pItem, item);

					buf.write(&item, sizeof(TPrivateShopSearchData));//Darklovers_Fix_Offline_Shop
					++dwCount;
				}
			}
		}
		// Search by type
		else
		{
			for (const auto& kv : map_itemSubType)
			{
				for (const auto& pItem : kv.second)
				{
					if (buf.size() + sizeof(TPrivateShopSearchData) > SAFE_RECV_BUFSIZE)
						SendData();//Darklovers_Fix_Offline_Shop

					if (FilterItem(pItem, rFilter, bUseFilter))
					{
						TPrivateShopSearchData item{};
						CopyItemData(pItem, item);

						buf.write(&item, sizeof(TPrivateShopSearchData));//Darklovers_Fix_Offline_Shop
						++dwCount;
					}
				}
			}
		}
	}

//Darklovers_Fix_Offline_Shop
	if (dwCount)
		SendData();
//Darklovers_Fix_Offline_Shop
}

