#include "stdafx.h"
#include "ClientManager.h"
#include "Cache.h"
#include "HB.h"
#include "Main.h"
#include "QID.h"
#include "PrivateShop.h"
#include "PrivateShopUtils.h"
#include "../../common/private_shop_lifetime.h"
#include "../../libgame/include/grid.h"

extern int g_test_server;
extern bool g_bHotBackup;
extern int g_log;

///////////////////////////////////////////////////////////////////////////////////////
//  Query Results
///////////////////////////////////////////////////////////////////////////////////////
void CClientManager::RESULT_PRIVATE_SHOP_LOAD(CPeer* pPeer, MYSQL_RES* pRes, DWORD dwHandle, DWORD dwPID)
{
	UpdatePrivateShopPremiumEvent();
	TPrivateShop TShop {};

	LPPRIVATE_SHOP existing = GetPrivateShop(dwPID);
	const bool recreateRecovery = existing && existing->GetState() == STATE_CLOSED;
	CPrivateShopCache* cache = GetPrivateShopCache(dwPID);
	if (existing)
		TShop = existing->GetTable();
	else if (cache)
		TShop = *cache->Get();
	else if (!CreatePrivateShopTableFromRes(pRes, TShop))
		return;
	if (!TShop.dwVnum)
		return;
	if (!existing && !GetPrivateShopItemCacheSet(dwPID))
	{
		sys_err("PRIVATESHOP_DB: load_missing_item_result pid=%u", dwPID);
		return;
	}

	std::vector<TPlayerPrivateShopItem> items;
	if (existing)
	{
		for (const auto& item : existing->GetItemContainer())
			items.push_back(item.TItem);
	}
	else if (auto itemCache = GetPrivateShopItemCacheSet(dwPID))
	{
		for (auto item : *itemCache)
			if (item->Get()->dwVnum)
				items.push_back(*item->Get());
	}
	// Closed/expired stock is restored for recovery, never directly for sale.
	if (!items.empty() && (TShop.bState == STATE_CLOSED || TShop.tPremiumTime <= time(0)))
	{
		TShop.bState = STATE_RECOVERY;
		if (existing)
			existing->ChangeState(STATE_RECOVERY);
		sys_log(0, "PRIVATESHOP_DB: recovery_load pid=%u items=%u", dwPID, static_cast<unsigned>(items.size()));
	}
	WORD itemCount = items.size();
	BYTE itemHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_LOAD;
	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(WORD) + sizeof(TPlayerPrivateShopItem) * itemCount);
	pPeer->Encode(&itemHeader, sizeof(itemHeader));
	pPeer->EncodeWORD(itemCount);
	if (itemCount)
		pPeer->Encode(items.data(), sizeof(TPlayerPrivateShopItem) * itemCount);

	// Send data to the player
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_LOAD;
	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPrivateShop));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&TShop, sizeof(TPrivateShop));

	// Add table to cache
	PutPrivateShopCache(&TShop);

	sys_log(0, "PRIVATE_SHOP_LOAD: pid %u", dwPID);

	// Use the same filtered snapshot for owner and authoritative instance.
	if (!GetPrivateShop(dwPID))
	{
		if (!PrivateShopCreate(&TShop, items))
		{
			sys_err("Cannot spawn private shop %u", dwPID);
			return;
		}

	}
	if (auto shop = GetPrivateShop(dwPID))
	{
		shop->SetOwnerHandle(dwHandle);
		shop->BindOwnerPeerHandle(pPeer->GetHandle());
		if (shop->GetState() != STATE_CLOSED && (!existing || recreateRecovery || !GetPeer(shop->GetShopPeerHandle())))
			PrivateShopSpawn(dwPID);
		sys_log(0, "PRIVATESHOP_DB: owner_rebound pid=%u peer=%u handle=%u", dwPID, pPeer->GetHandle(), dwHandle);
	}
}

void CClientManager::RESULT_PRIVATE_SHOP_ITEM_LOAD(CPeer* pPeer, MYSQL_RES* pRes, DWORD dwHandle, DWORD dwPID)
{
	// The following shop result sends one authoritative item/balance snapshot.
	if (GetPrivateShop(dwPID) || GetPrivateShopItemCacheSet(dwPID))
	{
		sys_log(0, "PRIVATESHOP_DB: item_load_use_cache pid=%u", dwPID);
		return;
	}
	std::vector<TPlayerPrivateShopItem> vec_privateShopItem;
	if (!pRes)
	{
		sys_err("PRIVATESHOP_DB: item_load_sql_failed pid=%u", dwPID);
		return;
	}
	CreatePrivateShopItemTableFromRes(pRes, &vec_privateShopItem, dwPID);

	WORD wCount = vec_privateShopItem.size();

	CreatePrivateShopItemCacheSet(dwPID);

	sys_log(0, "PRIVATE_SHOP_ITEM_LOAD: count %u pid %u", wCount, dwPID);

	if (wCount)
	{
		for (DWORD i = 0; i < wCount; ++i)
			PutPrivateShopItemCache(&vec_privateShopItem[i], true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////
//  Private Shop Cache
///////////////////////////////////////////////////////////////////////////////////////
void CClientManager::FlushPrivateShopCache(DWORD dwPID)
{
	auto it = m_map_privateShopCache.find(dwPID);

	if (it != m_map_privateShopCache.end())
	{
		CPrivateShopCache* c = it->second.get();

		c->Flush();
		m_map_privateShopCache.erase(it);
	}
}

CPrivateShopCache* CClientManager::GetPrivateShopCache(DWORD dwPID)
{
	auto it = m_map_privateShopCache.find(dwPID);

	if (it == m_map_privateShopCache.end())
		return nullptr;

	return it->second.get();
}

void CClientManager::PutPrivateShopCache(TPrivateShop* pTable)
{
	CPrivateShopCache* c;

	c = GetPrivateShopCache(pTable->dwOwner);

	if (!c)
	{
		std::unique_ptr<CPrivateShopCache> upShopCache = std::make_unique<CPrivateShopCache>();
		c = upShopCache.get();

		m_map_privateShopCache.emplace(pTable->dwOwner, std::move(upShopCache));
	}

	if (g_bHotBackup)
		PlayerHB::Instance().Put(pTable->dwOwner);

	c->Put(pTable);
}

bool CClientManager::DeletePrivateShopCache(DWORD dwPID)
{
	CPrivateShopCache* c = GetPrivateShopCache(dwPID);

	if (!c)
		return false;

	c->Delete();
	return true;
}

void CClientManager::UpdatePrivateShopCache()
{
	if (m_iCacheFlushCount >= m_iCacheFlushCountLimit)
		return;

	auto it = m_map_privateShopCache.begin();

	while (it != m_map_privateShopCache.end())
	{
		CPrivateShopCache* c = (it++)->second.get();

		if (c->CheckFlushTimeout())
		{
			if (g_test_server)
				sys_log(0, "UpdatePrivateShopCache ==> Flush() owner %d", c->Get()->dwOwner);

			c->Flush();

			if (++m_iCacheFlushCount >= m_iCacheFlushCountLimit)
				break;
		}
	}
}

CClientManager::TPrivateShopItemCacheSet* CClientManager::GetPrivateShopItemCacheSet(DWORD dwPID)
{
	auto it = m_map_pPrivateShopItemCacheSetPtr.find(dwPID);

	if (it == m_map_pPrivateShopItemCacheSetPtr.end())
		return nullptr;

	return it->second.get();
}

void CClientManager::CreatePrivateShopItemCacheSet(DWORD dwPID)
{
	if (m_map_pPrivateShopItemCacheSetPtr.find(dwPID) != m_map_pPrivateShopItemCacheSetPtr.end())
		return;

	std::unique_ptr<TPrivateShopItemCacheSet> upSet = std::make_unique<TPrivateShopItemCacheSet>();
	m_map_pPrivateShopItemCacheSetPtr.emplace(dwPID, std::move(upSet));

	if (g_log)
		sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: new cache %u", dwPID);
}

void CClientManager::FlushPrivateShopItemCacheSet(DWORD dwPID)
{
	auto it = m_map_pPrivateShopItemCacheSetPtr.find(dwPID);

	if (it == m_map_pPrivateShopItemCacheSetPtr.end())
	{
		sys_log(0, "FLUSH_PRIVATE_SHOP_ITEM_CACHESET : No PrivateShopItemCacheSet pid(%d)", dwPID);
		return;
	}

	TPrivateShopItemCacheSet* pSet = it->second.get();
	auto it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CPrivateShopItemCache* c = *it_set++;
		c->Flush();

		m_map_privateShopItemCache.erase(c->Get()->dwID);
	}

	pSet->clear();
	m_map_pPrivateShopItemCacheSetPtr.erase(it);

	if (g_log)
		sys_log(0, "FLUSH_PRIVATE_SHOP_ITEM_CACHESET : Deleted pid(%d)", dwPID);
}

bool CClientManager::DeletePrivateShopItemCacheSet(DWORD dwPID)
{
	auto it = m_map_pPrivateShopItemCacheSetPtr.find(dwPID);

	if (it == m_map_pPrivateShopItemCacheSetPtr.end())
	{
		sys_log(0, "DELETE_PRIVATE_SHOP_ITEM_CACHESET : No PrivateShopItemCacheSet pid(%d)", dwPID);
		return false;
	}

	TPrivateShopItemCacheSet* pSet = it->second.get();
	auto it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CPrivateShopItemCache* c = *it_set++;
		c->Delete();
		c->Flush();

		m_map_privateShopItemCache.erase(c->Get()->dwID);
	}

	pSet->clear();
	m_map_pPrivateShopItemCacheSetPtr.erase(it);

	if (g_log)
		sys_log(0, "DELETE_PRIVATE_SHOP_ITEM_CACHESET : Deleted pid(%d)", dwPID);

	return true;
}

CPrivateShopItemCache* CClientManager::GetPrivateShopItemCache(DWORD dwID)
{
	auto it = m_map_privateShopItemCache.find(dwID);

	if (it == m_map_privateShopItemCache.end())
		return nullptr;

	return it->second.get();
}

void CClientManager::PutPrivateShopItemCache(TPlayerPrivateShopItem* pNew, bool bSkipQuery)
{
	CPrivateShopItemCache* c;

	c = GetPrivateShopItemCache(pNew->dwID);
	if (!c)
	{
		if (g_log)
			sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: PutPrivateShopItemCache ==> New CPrivateShopItemCache id%d vnum%d new owner%d", pNew->dwID, pNew->dwVnum, pNew->dwOwner);

		std::unique_ptr< CPrivateShopItemCache> upPrivateShopItemCache = std::make_unique<CPrivateShopItemCache>();
		c = upPrivateShopItemCache.get();

		m_map_privateShopItemCache.emplace(pNew->dwID, std::move(upPrivateShopItemCache));
	}
	else
	{
		if (g_log)
			sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: PutPrivateShopItemCache ==> Have Cache");
		if (pNew->dwOwner != c->Get()->dwOwner)
		{
			auto it = m_map_pPrivateShopItemCacheSetPtr.find(c->Get()->dwOwner);

			if (it != m_map_pPrivateShopItemCacheSetPtr.end())
			{
				if (g_log)
					sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: delete owner %u id %u new owner %u", c->Get()->dwOwner, c->Get()->dwID, pNew->dwOwner);

				TPrivateShopItemCacheSet* pSet = it->second.get();
				pSet->erase(c);
			}
		}
	}
	c->Put(pNew, bSkipQuery);

	auto it = m_map_pPrivateShopItemCacheSetPtr.find(c->Get()->dwOwner);

	if (it != m_map_pPrivateShopItemCacheSetPtr.end())
	{
		if (g_log)
			sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: save %u id %u", c->Get()->dwOwner, c->Get()->dwID);
		else
			sys_log(1, "PRIVATE_SHOP_ITEM_CACHE: save %u id %u", c->Get()->dwOwner, c->Get()->dwID);

		TPrivateShopItemCacheSet* pSet = it->second.get();
		pSet->insert(c);
	}
	else
	{
		if (g_log)
			sys_log(0, "PRIVATE_SHOP_ITEM_CACHE: direct save %u id %u", c->Get()->dwOwner, c->Get()->dwID);
		else
			sys_log(1, "PRIVATE_SHOP_ITEM_CACHE: direct save %u id %u", c->Get()->dwOwner, c->Get()->dwID);

		c->OnFlush();
	}
}

bool CClientManager::DeletePrivateShopItemCache(DWORD dwID)
{
	CPrivateShopItemCache* c = GetPrivateShopItemCache(dwID);

	if (!c)
		return false;

	c->Delete();
	return true;
}

void CClientManager::UpdatePrivateShopItemCache()
{
	if (m_iCacheFlushCount >= m_iCacheFlushCountLimit)
		return;

	auto it = m_map_privateShopItemCache.begin();

	while (it != m_map_privateShopItemCache.end())
	{
		CPrivateShopItemCache* c = (it++)->second.get();

		if (c->CheckFlushTimeout())
		{
			if (g_test_server)
				sys_log(0, "UpdatePrivateShopItemCache ==> Flush() vnum %d owner %d", c->Get()->dwVnum, c->Get()->dwOwner);

			c->Flush();

			if (++m_iCacheFlushCount >= m_iCacheFlushCountLimit)
				break;
		}
	}
}

void CClientManager::UpdatePrivateShopItemCacheSet(DWORD dwPID)
{
	auto it = m_map_pPrivateShopItemCacheSetPtr.find(dwPID);

	if (it == m_map_pPrivateShopItemCacheSetPtr.end())
	{
		if (g_test_server)
			sys_log(0, "UPDATE_PRIVATE_SHOP_ITEM_CACHESET : UpdatePrivateShopItemCacheSet ==> No PrivateShopItemCacheSet pid(%d)", dwPID);
		return;
	}

	TPrivateShopItemCacheSet* pSet = it->second.get();
	auto it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CPrivateShopItemCache* c = *it_set++;
		c->Flush();
	}

	if (g_log)
		sys_log(0, "UPDATE_PRIVATE_SHOP_ITEM_CACHESET : UpdatePrivateShopItemCacheSet pid(%d)", dwPID);
}

///////////////////////////////////////////////////////////////////////////////////////
//  Private Shop Management
///////////////////////////////////////////////////////////////////////////////////////
LPPRIVATE_SHOP CClientManager::CreatePrivateShop(DWORD dwPID)
{
	sys_log(0, "PRIVATESHOP_DB: create_shop_begin pid=%u count=%u", dwPID, static_cast<unsigned>(m_map_privateShop.size()));
	auto it = m_map_privateShop.find(dwPID);

	if (it != m_map_privateShop.end())
	{
		sys_err("PRIVATESHOP_DB: create_shop_duplicate pid=%u", dwPID);
		m_map_privateShop.erase(it);
		sys_log(0, "PRIVATESHOP_DB: create_shop_duplicate_erased pid=%u count=%u", dwPID, static_cast<unsigned>(m_map_privateShop.size()));
	}

	std::unique_ptr<CPrivateShop> upPrivateShop = std::make_unique<CPrivateShop>();
	LPPRIVATE_SHOP pPrivateShop = upPrivateShop.get();
	sys_log(0, "PRIVATESHOP_DB: create_shop_alloc_ok pid=%u ptr=%p", dwPID, pPrivateShop);

	m_map_privateShop.emplace(dwPID, std::move(upPrivateShop));
	sys_log(0, "PRIVATESHOP_DB: create_shop_done pid=%u count=%u", dwPID, static_cast<unsigned>(m_map_privateShop.size()));

	return pPrivateShop;
}

bool CClientManager::DeletePrivateShop(DWORD dwPID)
{
	auto it = m_map_privateShop.find(dwPID);

	if (it == m_map_privateShop.end())
	{
		sys_err("Could not find private shop with id %d", dwPID);
		return false;
	}

	m_map_privateShop.erase(it);
	return true;
}

LPPRIVATE_SHOP CClientManager::GetPrivateShop(DWORD dwPID)
{
	auto it = m_map_privateShop.find(dwPID);

	if (it == m_map_privateShop.end())
		return nullptr;
	
	return it->second.get();
}

void CClientManager::ProcessPrivateShopPacket(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const BYTE bSubHeader = *reinterpret_cast<const BYTE*>(c_szData);
	c_szData += sizeof(BYTE);
	sys_log(0, "PRIVATESHOP_DB: process_packet_begin peer=%p handle=%u subheader=%u", pPeer, dwHandle, bSubHeader);

	switch (bSubHeader)
	{
		/* Core Mechanics */
		case PRIVATE_SHOP_GD_SUBHEADER_LOGOUT:
		{
			const DWORD pid = *reinterpret_cast<const DWORD*>(c_szData);
			LPPRIVATE_SHOP shop = GetPrivateShop(pid);
			if (shop && (shop->GetOwnerPeerHandle() != pPeer->GetHandle() || shop->GetOwnerHandle() != dwHandle))
			{
				sys_log(0, "PRIVATESHOP_DB: stale_logout_ignored pid=%u peer=%u handle=%u", pid, pPeer->GetHandle(), dwHandle);
				break;
			}
			sys_log(0, "PRIVATESHOP_DB: process_packet_dispatch subheader=LOGOUT pid=%u", *(DWORD*)c_szData);
			PrivateShopStartPremiumEvent(*(DWORD*)c_szData);
			break;
		}

		case PRIVATE_SHOP_GD_SUBHEADER_CREATE:
			sys_log(0, "PRIVATESHOP_DB: process_packet_dispatch subheader=CREATE handle=%u", dwHandle);
			PrivateShopBuild(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_CLOSE:
			PrivateShopClose(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_DELETE:
			PrivateShopDelete(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_DESPAWN:
			PrivateShopDespawn(pPeer, dwHandle, c_szData);
			break;

		/* Requests */
		case PRIVATE_SHOP_GD_SUBHEADER_WITHDRAW_REQUEST:
			PrivateShopWithdrawRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_MODIFY_REQUEST:
			PrivateShopModifyRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_BUY_REQUEST:
			PrivateShopBuyRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_PRICE_CHANGE_REQUEST:
			PrivateShopItemPriceChangeRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_MOVE_REQUEST:
			PrivateShopItemMoveRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKIN_REQUEST:
			PrivateShopItemCheckinRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKIN_UPDATE:
			PrivateShopItemCheckinUpdate(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKOUT_REQUEST:
			PrivateShopItemCheckoutRequest(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_CHECKOUT_UPDATE:
			PrivateShopItemCheckoutUpdate(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_TITLE_CHANGE_REQUEST:
			PrivateShopTitleChangeRequest(pPeer, dwHandle, c_szData);
			break;

		/* Actions */
		case PRIVATE_SHOP_GD_SUBHEADER_WITHDRAW:
			PrivateShopWithdraw(c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_BUY:
			PrivateShopBuy(pPeer, dwHandle, c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_FAILED_BUY:
			PrivateShopFailedBuy(c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_TRANSFER:
			PrivateShopItemTransfer((TPlayerItem*)c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_DELETE:
			PrivateShopItemDelete(c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_ITEM_EXPIRE:
			PrivateShopItemExpire(c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_PREMIUM_TIME_UPDATE:
			PrivateShopPremiumTimeUpdate(c_szData);
			break;

		case PRIVATE_SHOP_GD_SUBHEADER_INIT:
			//PrivateShopPeerSpawn(pPeer);
			break;

		default:
			sys_err("PRIVATESHOP_DB: process_packet_fail reason=unknown_subheader subheader=%u handle=%u", bSubHeader, dwHandle);
			break;
	}
	sys_log(0, "PRIVATESHOP_DB: process_packet_done handle=%u subheader=%u", dwHandle, bSubHeader);
}

LPPRIVATE_SHOP CClientManager::PrivateShopSpawn(DWORD dwShopID)
{
	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwShopID);

	// If shop does not exist, fetch its data first
	if (!pPrivateShop)
	{
		TPrivateShop									privateShopTable{};
		std::vector<TPlayerPrivateShopItem>				vec_privateShopItem;

		if (!PrivateShopFetchData(dwShopID, privateShopTable, vec_privateShopItem))
		{
			sys_err("Cannot fetch data for private shop %u", dwShopID);
			return nullptr;
		}

		pPrivateShop = PrivateShopCreate(&privateShopTable, vec_privateShopItem);
		if (!pPrivateShop)
		{
			sys_err("Failed to create private shop %u", dwShopID);
			return nullptr;
		}
	}

	CPeer* pShopPeer = GetPrivateShopPeer(pPrivateShop->GetChannel(), pPrivateShop->GetPort());
	if (!pShopPeer)
	{
		sys_err("Cannot find peer for private shop %u", dwShopID);
		return nullptr;
	}

	pPrivateShop->BindShopPeerHandle(pShopPeer->GetHandle());
	PrivateShopGameSpawn(pPrivateShop);
	return pPrivateShop;
}

LPPRIVATE_SHOP CClientManager::PrivateShopCreate(TPrivateShop* pTable, const std::vector<TPlayerPrivateShopItem>& c_vec_shopItem)
{
	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(pTable->dwOwner);
	if (!pPrivateShop)
	{
		pPrivateShop = CreatePrivateShop(pTable->dwOwner);
		sys_log(0, "Re-creating private shop for owner %u", pTable->dwOwner);
	}

	// Bind data to the newly created shop
	pPrivateShop->SetTable(pTable);
	pPrivateShop->RemoveAllItem();

	for (const auto& c_rShopItem : c_vec_shopItem)
	{
		TItemTable* pItemTable = GetItemTable(c_rShopItem.dwVnum);
		if (!pItemTable)
		{
			sys_err("Cannot find proto for item %u", c_rShopItem.dwVnum);

			// Remove previously added items
			pPrivateShop->RemoveAllItem();
			DeletePrivateShop(pTable->dwOwner);
			sys_err("PRIVATESHOP_DB: create_rolled_back pid=%u reason=missing_proto", pTable->dwOwner);
			return nullptr;
		}

		if (!pPrivateShop->SetItem(&c_rShopItem, pItemTable))
		{
			sys_err("Cannot set item on private shop %u pos %u vnum %u", pTable->dwOwner, c_rShopItem.wPos, c_rShopItem.dwVnum);

			// Remove previously added items
			pPrivateShop->RemoveAllItem();
			DeletePrivateShop(pTable->dwOwner);
			sys_err("PRIVATESHOP_DB: create_rolled_back pid=%u reason=invalid_grid", pTable->dwOwner);
			return nullptr;
		}

		sys_log(0, "PRIVATE_SHOP: Added item %u pos %u vnum %u to private shop %u", c_rShopItem.dwID, c_rShopItem.wPos, c_rShopItem.dwVnum, pTable->dwOwner);
	}

	sys_log(0, "PRIVATE_SHOP: Private shop %u successfully created", pTable->dwOwner);
	return pPrivateShop;
}

void CClientManager::PrivateShopBuild(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPrivateShop* pTable = (TPrivateShop*)c_szData;
	c_szData += sizeof(TPrivateShop);
	pTable->dwLifetimeSeconds = PRIVATE_SHOP_LIFETIME_SECONDS;
	pTable->tPremiumTime = time(0) + pTable->dwLifetimeSeconds;
	sys_log(0, "PRIVATESHOP_DB: lifetime_assigned pid=%u seconds=%u deadline=%u", pTable->dwOwner, pTable->dwLifetimeSeconds, static_cast<unsigned>(pTable->tPremiumTime));

	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_CREATE_RESULT;

	TPacketDGPrivateShopCreateResult subPacket{};
	thecore_memcpy(&subPacket.privateShopTable, pTable, sizeof(subPacket.privateShopTable));

	sys_log(0, "PRIVATESHOP_DB: build_begin owner=%u title=%s vnum=%u page_count=%u channel=%u port=%u item_payload_ptr=%p handle=%u peer=%p",
		pTable->dwOwner, pTable->szTitle, pTable->dwVnum, pTable->bPageCount, pTable->bChannel, pTable->wPort, c_szData, dwHandle, pPeer);

	if (pTable->bPageCount == 0 || pTable->bPageCount > PRIVATE_SHOP_PAGE_MAX_NUM || pTable->dwVnum < 30000 || pTable->dwVnum > 30008)
	{
		subPacket.bSuccess = false;

		sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=invalid_header", pTable->dwOwner);
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

		sys_err("PRIVATESHOP_DB: reason=invalid_build_header owner=%u vnum=%u page_count=%u", pTable->dwOwner, pTable->dwVnum, pTable->bPageCount);
		return;
	}
	// Check if the shop already exists
	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(pTable->dwOwner);
	if (pPrivateShop)
	{
		subPacket.bSuccess = false;

		sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=already_exists", pTable->dwOwner);
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

		sys_log(0, "PRIVATE_SHOP: Private shop %u already created", pTable->dwOwner);
		return;
	}

	auto pCacheSet = GetPrivateShopItemCacheSet(pTable->dwOwner);
	if (pCacheSet && pCacheSet->size())
	{
		subPacket.bSuccess = false;

		sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=item_cache_exists cache_size=%u", pTable->dwOwner, static_cast<unsigned>(pCacheSet->size()));
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

		sys_log(0, "PRIVATE_SHOP: Private shop %u cannot be built as private shop item cache already exists", pTable->dwOwner);
		return;
	}

	// Initialize items
	const WORD wCount = *reinterpret_cast<const WORD*>(c_szData);
	c_szData += sizeof(WORD);
	sys_log(0, "PRIVATESHOP_DB: build_item_count owner=%u count=%u", pTable->dwOwner, wCount);

	if (wCount == 0 || wCount > PRIVATE_SHOP_HOST_ITEM_MAX_NUM)
	{
		subPacket.bSuccess = false;

		sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=invalid_item_count count=%u", pTable->dwOwner, wCount);
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

		sys_err("PRIVATESHOP_DB: reason=invalid_build_item_count owner=%u count=%u", pTable->dwOwner, wCount);
		return;
	}

	bool abUsedPos[PRIVATE_SHOP_HOST_ITEM_MAX_NUM] = {};
	TPlayerPrivateShopItem* pItem = (TPlayerPrivateShopItem*)c_szData;

	std::vector<TPlayerPrivateShopItem> vec_privateShopItem(wCount);

	for (int i = 0; i < wCount; ++i, ++pItem)
	{
		TPlayerPrivateShopItem& rPrivateShopItem = vec_privateShopItem.at(i);
		memcpy(&rPrivateShopItem, pItem, sizeof(TPlayerPrivateShopItem));
		sys_log(0, "PRIVATESHOP_DB: build_item owner=%u idx=%d item_id=%u vnum=%u pos=%u gold=%lld cheque=%u",
			pTable->dwOwner, i, rPrivateShopItem.dwID, rPrivateShopItem.dwVnum, rPrivateShopItem.wPos,
			rPrivateShopItem.TPrice.llGold, rPrivateShopItem.TPrice.dwCheque);

		if (rPrivateShopItem.wPos >= PRIVATE_SHOP_HOST_ITEM_MAX_NUM || abUsedPos[rPrivateShopItem.wPos])
		{
			subPacket.bSuccess = false;

			sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=invalid_pos item=%u pos=%u", pTable->dwOwner, rPrivateShopItem.dwID, rPrivateShopItem.wPos);
			pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
			pPeer->Encode(&bSubHeader, sizeof(BYTE));
			pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

			sys_err("PRIVATESHOP_DB: reason=invalid_build_pos owner=%u item=%u pos=%u", pTable->dwOwner, rPrivateShopItem.dwID, rPrivateShopItem.wPos);
			return;
		}

		abUsedPos[rPrivateShopItem.wPos] = true;
		if (rPrivateShopItem.TPrice.llGold <= 0 || rPrivateShopItem.TPrice.llGold > GOLD_MAX || rPrivateShopItem.TPrice.dwCheque != 0)
		{
			subPacket.bSuccess = false;

			sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=invalid_price item=%u", pTable->dwOwner, rPrivateShopItem.dwID);
			pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
			pPeer->Encode(&bSubHeader, sizeof(BYTE));
			pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

			sys_err("PRIVATESHOP_DB: reason=invalid_build_price owner=%u item=%u vnum=%u gold=%lld cheque=%u",
				pTable->dwOwner, rPrivateShopItem.dwID, rPrivateShopItem.dwVnum, rPrivateShopItem.TPrice.llGold, rPrivateShopItem.TPrice.dwCheque);
			return;
		}
	}
		
	// Create private shop instance
	sys_log(0, "PRIVATESHOP_DB: build_create_instance_begin owner=%u item_count=%u", pTable->dwOwner, wCount);
	pPrivateShop = PrivateShopCreate(pTable, vec_privateShopItem);
	if (!pPrivateShop)
	{
		subPacket.bSuccess = false;

		sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=0 reason=create_instance_failed", pTable->dwOwner);
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

		sys_log(0, "PRIVATE_SHOP: Cannot build private shop for owner pid %u", pTable->dwOwner);
		return;
	}
	sys_log(0, "PRIVATESHOP_DB: build_create_instance_ok owner=%u ptr=%p", pTable->dwOwner, pPrivateShop);

	// Bind peer data to the instance
	sys_log(0, "PRIVATESHOP_DB: build_bind_peer_begin owner=%u shop_peer=%u owner_peer=%u owner_handle=%u",
		pTable->dwOwner, pPeer->GetHandle(), pPeer->GetHandle(), dwHandle);
	pPrivateShop->BindShopPeerHandle(pPeer->GetHandle());
	pPrivateShop->BindOwnerPeerHandle(pPeer->GetHandle());
	pPrivateShop->SetOwnerHandle(dwHandle);
	sys_log(0, "PRIVATESHOP_DB: build_bind_peer_ok owner=%u", pTable->dwOwner);

	// Setup cache
	sys_log(0, "PRIVATESHOP_DB: build_cache_begin owner=%u", pTable->dwOwner);
	PutPrivateShopCache(pTable);
	GetPrivateShopCache(pTable->dwOwner)->Flush();
	CreatePrivateShopItemCacheSet(pPrivateShop->GetOwner());
	sys_log(0, "PRIVATESHOP_DB: build_cache_ok owner=%u", pTable->dwOwner);

	for (auto& c_rShopItem : vec_privateShopItem)
	{
		sys_log(0, "PRIVATESHOP_DB: build_cache_item_begin owner=%u item_id=%u vnum=%u pos=%u", pTable->dwOwner, c_rShopItem.dwID, c_rShopItem.dwVnum, c_rShopItem.wPos);
		// Delete item from the player's cache and remove it from item's table
		if (!DeleteItemCache(c_rShopItem.dwID))
		{
			char szQuery[64];
			snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", GetTablePostfix(), c_rShopItem.dwID);
			CDBManager::instance().AsyncQuery(szQuery);
		}

		// Insert the item to private shop item table
		PutPrivateShopItemCache(&c_rShopItem);
		sys_log(0, "PRIVATESHOP_DB: build_cache_item_ok owner=%u item_id=%u", pTable->dwOwner, c_rShopItem.dwID);
	}

	// Send result back to the game core
	subPacket.bSuccess = true;

	sys_log(0, "PRIVATESHOP_DB: build_send_result owner=%u success=1 handle=%u", pTable->dwOwner, dwHandle);
	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopCreateResult));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopCreateResult));

	sys_log(0, "PRIVATESHOP_DB: build_done owner=%u", pTable->dwOwner);
}

void CClientManager::PrivateShopClose(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const DWORD dwPID = *reinterpret_cast<const DWORD*>(c_szData);
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_DESTROY;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", dwPID);
		return;
	}

	for (const auto& item : pPrivateShop->GetItemContainer())
	{
		if (!item.bAvailable)
		{
			sys_log(0, "PRIVATESHOP_DB: close_wait_reservation pid=%u", dwPID);
			return;
		}
	}
	if (pPrivateShop->GetGold() || pPrivateShop->GetCheque())
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_CLOSE_RESULT_BALANCE_AVAILABLE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		return;
	}

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));

	sys_log(0, "PRIVATE_SHOP: Close request forwarded to game for private shop %u", dwPID);
}

void CClientManager::PrivateShopDelete(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const DWORD dwPID = *reinterpret_cast<const DWORD*>(c_szData);

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		sys_err("Cannot find private shop %u", dwPID);
		return;
	}

	// Delete private shop cache and SQL record
	if (!DeletePrivateShopCache(dwPID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop%s WHERE owner_id=%u", GetTablePostfix(), dwPID);

		CDBManager::instance().AsyncQuery(szQuery);
	}

	// Delete private shop item cache and SQL record
	if (!DeletePrivateShopItemCacheSet(dwPID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop_item%s WHERE owner_id=%u", GetTablePostfix(), dwPID);

		CDBManager::instance().AsyncQuery(szQuery);
	}

	// Delete shop data
	DeletePrivateShop(dwPID);
	sys_log(0, "PRIVATE_SHOP: Deleted private shop with id %u", dwPID);
}

void CClientManager::PrivateShopDespawn(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const DWORD dwPID = *reinterpret_cast<const DWORD*>(c_szData);

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		sys_err("Cannot find private shop %u", dwPID);
		return;
	}

	DeletePrivateShop(dwPID);

	sys_log(0, "PRIVATE_SHOP: Despawning private shop with id %u (Game request)", dwPID);
}

void CClientManager::PrivateShopWithdrawRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const DWORD dwPID = *reinterpret_cast<const DWORD*>(c_szData);
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_WITHDRAW;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", dwPID);
		return;
	}

	if (!pPrivateShop->GetGold() && !pPrivateShop->GetCheque())
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_WITHDRAW_RESULT_NO_BALANCE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		return;
	}

	TPacketDGPrivateShopWithdraw subPacket {};
	subPacket.llGold = pPrivateShop->GetGold();
	subPacket.dwCheque = pPrivateShop->GetCheque();

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopWithdraw));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopWithdraw));

	sys_log(0, "PRIVATE_SHOP: Withdraw request forwarded to game for private shop %u", dwPID);
}

void CClientManager::PrivateShopItemCheckinUpdate(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPlayerPrivateShopItem* pItem = (TPlayerPrivateShopItem*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(pItem->dwOwner);
	if (!pPrivateShop)
	{
		sys_err("Cannot find private shop %u", pItem->dwOwner);
		return;
	}

	TItemTable* pItemTable = GetItemTable(pItem->dwVnum);
	if (!pItemTable)
	{
		sys_err("Cannot find proto for item %u", pItem->dwVnum);
		return;
	}

	if (!pPrivateShop->SetItem(pItem, pItemTable))
	{
		sys_err("Item checkin update failed for private shop %u", pItem->dwVnum);
		return;
	}

	// Delete item from the player's cache and remove it from item's table
	if (!DeleteItemCache(pItem->dwID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", GetTablePostfix(), pItem->dwID);
		CDBManager::instance().AsyncQuery(szQuery);
	}

	// Insert the item to private shop item table
	PutPrivateShopItemCache(pItem);

	// Add the item from the private shop on game core
	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (pShopPeer)
	{
		BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ADD_ITEM;
		DWORD dwShopID = pPrivateShop->GetOwner();

		pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(DWORD) + sizeof(TPlayerPrivateShopItem));
		pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
		pShopPeer->Encode(&dwShopID, sizeof(DWORD));
		pShopPeer->Encode(pItem, sizeof(TPlayerPrivateShopItem));
	}
	else
	{
		sys_err("Cannot find private shop peer %u", pItem->dwOwner);
	}

	sys_log(0, "PRIVATE_SHOP: Item checkin on private shop %u pos %u vnum %u gold %lld won %u", pItem->dwOwner, pItem->wPos, pItem->dwVnum, pItem->TPrice.llGold, pItem->TPrice.dwCheque);
}

void CClientManager::PrivateShopItemCheckoutUpdate(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const WORD wPos = *reinterpret_cast<const WORD*>(c_szData);
	c_szData += sizeof(WORD);

	TPlayerItem* pItem = (TPlayerItem*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(pItem->owner);
	if (!pPrivateShop)
	{
		sys_err("Cannot find private shop %u", pItem->owner);
		return;
	}

	// Remove the item from private shop and private shop item table
	pPrivateShop->RemoveItem(wPos);

	// Save the item to item table
	PutItemCache(pItem);

	// Remove the item from the private shop on game core
	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (pShopPeer)
	{
		BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_REMOVE_ITEM;
		DWORD dwShopID = pPrivateShop->GetOwner();

		pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(DWORD) + sizeof(WORD));
		pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
		pShopPeer->Encode(&dwShopID, sizeof(DWORD));
		pShopPeer->Encode(&wPos, sizeof(WORD));
	}
	else
	{
		sys_err("Cannot find private shop peer %u", pItem->owner);
	}

	sys_log(0, "PRIVATE_SHOP: Item checkout on private shop %u item %u vnum %u pos %u", pItem->owner,  pItem->id, pItem->vnum, wPos);

	// No items left but stash still exists
	if (!pPrivateShop->GetItemCount() && (pPrivateShop->GetGold() || pPrivateShop->GetCheque()))
	{
		sys_log(0, "PRIVATE_SHOP: Despawning item empty private shop %u by item checkout", pItem->owner);
		PrivateShopGameDespawn(pPrivateShop);
	}

	// No items and no stash
	else if (!pPrivateShop->GetItemCount() && !pPrivateShop->GetGold() && !pPrivateShop->GetCheque())
	{
		sys_log(0, "PRIVATE_SHOP: Destroying empty private shop %u by item checkout", pItem->owner);
		PrivateShopGameDespawn(pPrivateShop);
		PrivateShopDestroy(pPrivateShop);
	}
}

void CClientManager::PrivateShopWithdraw(const char* c_szData)
{
	const auto* packet = reinterpret_cast<const TPacketGDPrivateShopWithdraw*>(c_szData);
	const DWORD dwPID = packet->dwPID;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		sys_err("No private shop was found for withrawal for player %d", dwPID);
		return;
	}

	sys_log(0, "PRIVATE_SHOP: Withdrawn private shop %u gold %lld cheque %u", dwPID, pPrivateShop->GetGold(), pPrivateShop->GetCheque());
	if (packet->llGold < 0 || packet->llGold > pPrivateShop->GetGold() || packet->dwCheque > pPrivateShop->GetCheque())
	{
		sys_err("PRIVATESHOP_DB: invalid_withdraw pid=%u gold=%lld", dwPID, packet->llGold);
		return;
	}
	pPrivateShop->Withdraw(packet->llGold, packet->dwCheque);
	sys_log(0, "PRIVATESHOP_DB: withdrawal_confirmed pid=%u gold=%lld remaining=%lld", dwPID, packet->llGold, pPrivateShop->GetGold());

	// Destroy shop if there are no items left in the shop
	if (!pPrivateShop->GetItemCount() && !pPrivateShop->GetGold() && !pPrivateShop->GetCheque())
	{
		sys_log(0, "PRIVATE_SHOP: Destroying empty withdrawn private shop %u", dwPID);
		PrivateShopDestroy(pPrivateShop);
	}
}

void CClientManager::PrivateShopModifyRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	const DWORD dwPID = *reinterpret_cast<const DWORD*>(c_szData);
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_STATE_UPDATE;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop || pPrivateShop->GetState() == STATE_CLOSED)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", dwPID);
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", dwPID);
		return;
	}

	// Update state of the shop
	for (const auto& item : pPrivateShop->GetItemContainer())
	{
		if (!item.bAvailable)
		{
			sys_log(0, "PRIVATESHOP_DB: modify_wait_purchase pid=%u item=%u", dwPID, item.TItem.dwID);
			return;
		}
	}
	BYTE bState = pPrivateShop->GetState();
	if (pPrivateShop->GetPremiumTime() <= time(0) || bState == STATE_RECOVERY)
	{
		sys_log(0, "PRIVATESHOP_DB: reopen_denied_expired pid=%u", dwPID);
		return;
	}
	if (bState == STATE_OPEN)
	{
		pPrivateShop->ChangeState(STATE_MODIFY);
		bState = STATE_MODIFY;

		sys_log(0, "PRIVATE_SHOP: State changed to modify for private shop %u", dwPID);
	}
	else if (bState == STATE_MODIFY)
	{
		pPrivateShop->ChangeState(STATE_OPEN);
		bState = STATE_OPEN;

		sys_log(0, "PRIVATE_SHOP: State changed to open for private shop %u", dwPID);
	}

	//Send the result back to owner (and shop if it is on the same peer)

	TPacketDGPrivateShopStateUpdate subPacket {};
	subPacket.dwPID = dwPID;
	subPacket.bState = bState;

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopStateUpdate));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopStateUpdate));

	// Not needed to be sent twice if both player&shop are on the same peer
	if (pPeer != pShopPeer)
	{
		pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketDGPrivateShopStateUpdate));
		pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
		pShopPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopStateUpdate));
	}
}

void CClientManager::PrivateShopBuyRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketGDPrivateShopBuyRequest* p = (TPacketGDPrivateShopBuyRequest*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_REQUEST;

	if (p->TPrice.llGold <= 0 || p->TPrice.llGold > GOLD_MAX || p->TPrice.dwCheque != 0)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_PRICE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		sys_err("PRIVATESHOP_DB: reason=invalid_buy_price customer=%u shop=%u pos=%u gold=%lld cheque=%u",
			p->dwCustomerPID, p->dwShopID, p->wPos, p->TPrice.llGold, p->TPrice.dwCheque);
		return;
	}

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwShopID);
		return;
	}

	// Player cannot buy an item from the private shop while it is being modified
	if (pPrivateShop->GetState() != STATE_OPEN || pPrivateShop->GetPremiumTime() <= time(0))
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_MODIFY_STATE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	// Check if the item exists 
	TPrivateShopItemInfo* pItemInfo = pPrivateShop->GetItem(p->wPos);
	if (!pItemInfo || !pItemInfo->bAvailable)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_ITEM;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find item on position %u for private shop %u", p->wPos, p->dwShopID);
		return;
	}

	// Check if the price corresponds
	if (pItemInfo->TItem.TPrice.llGold != p->TPrice.llGold || pItemInfo->TItem.TPrice.dwCheque != p->TPrice.dwCheque)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_PRICE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Item price mismatch on private shop %u pos %u game(gold %lld, cheque %u) db(gold %lld, cheque %u)",
			p->dwShopID, p->wPos, p->TPrice.llGold, 
			p->TPrice.dwCheque, pItemInfo->TItem.TPrice.llGold, pItemInfo->TItem.TPrice.dwCheque);
		return;
	}

	// Check player's balance if he can afford the item
	if (p->llGoldBalance < pItemInfo->TItem.TPrice.llGold)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_NO_GOLD;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	p->llGoldBalance -= pItemInfo->TItem.TPrice.llGold;

	if (p->dwChequeBalance < pItemInfo->TItem.TPrice.dwCheque)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_NO_CHEQUE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	p->dwChequeBalance -= pItemInfo->TItem.TPrice.dwCheque;

	// Check if the shop's peer is available
	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwShopID);
		return;
	}

	// Mark the item unavailable in case someone else also wants to buy it while the transaction is in process
	pItemInfo->bAvailable = false;
	static DWORD nextReservation = 0;
	if (++nextReservation == 0)
		++nextReservation;
	pItemInfo->dwReservation = nextReservation;
	pItemInfo->dwBuyerPeer = pPeer->GetHandle();
	pItemInfo->tReservationExpiry = time(0) + 30;
	sys_log(0, "PRIVATESHOP_DB: reserved shop=%u item=%u token=%u peer=%u", p->dwShopID, pItemInfo->TItem.dwID, nextReservation, pPeer->GetHandle());

	// Forward the request to the game
	TPacketDGPrivateShopBuyRequest subPacket {};
	subPacket.dwReservation = pItemInfo->dwReservation;
	subPacket.dwCustomerPID = p->dwCustomerPID;

	subPacket.TRequestedItem = pItemInfo->TItem;

	sys_log(0, "PRIVATE_SHOP: Buy request forwarded to game for private shop %u customer %u item pos %u item %u yang %lld won %u",
		p->dwShopID, p->dwCustomerPID, p->wPos, pItemInfo->TItem.dwVnum,
		p->TPrice.llGold, p->TPrice.dwCheque);

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopBuyRequest));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopBuyRequest));
}

void CClientManager::PrivateShopItemPriceChangeRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketPrivateShopItemPriceChange* p = (TPacketPrivateShopItemPriceChange*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_PRICE_CHANGE;

	if (p->TPrice.llGold <= 0 || p->TPrice.llGold > GOLD_MAX || p->TPrice.dwCheque != 0)
	{
		BYTE bFailSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_PRICE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bFailSubHeader, sizeof(BYTE));
		sys_err("PRIVATESHOP_DB: reason=invalid_price_change shop=%u pos=%u gold=%lld cheque=%u",
			p->dwShopID, p->wPos, p->TPrice.llGold, p->TPrice.dwCheque);
		return;
	}

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwShopID);
		return;
	}

	// Player cannot edit shop's content if its not set to modify state
	if (pPrivateShop->GetState() != STATE_MODIFY)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NOT_MODIFY_STATE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwShopID);
		return;
	}

	// If item price change was successful, forward the packet to owner & shop
	if (pPrivateShop->ChangeItemPrice(p->wPos, p->TPrice.llGold, p->TPrice.dwCheque))
	{
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketPrivateShopItemPriceChange));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(p, sizeof(TPacketPrivateShopItemPriceChange));

		// Not needed to be sent twice if both player&shop are on the same peer
		if (pPeer != pShopPeer)
		{
			pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketPrivateShopItemPriceChange));
			pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
			pShopPeer->Encode(p, sizeof(TPacketPrivateShopItemPriceChange));
		}

		sys_log(0, "PRIVATE_SHOP: Changed item price on pos %u private shop %u -> gold %lld won %u", p->wPos, p->dwShopID, p->TPrice.llGold, p->TPrice.dwCheque);
	}
	else
	{
		BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_ITEM;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot change item price on private shop %u item pos %u", p->dwShopID, p->wPos);
	}
}

void CClientManager::PrivateShopItemMoveRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketPrivateShopItemMove* p = (TPacketPrivateShopItemMove*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_MOVE;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwShopID);
		return;
	}

	// Player cannot edit shop's content if its not set to modify state
	if (pPrivateShop->GetState() != STATE_MODIFY)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NOT_MODIFY_STATE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwShopID);
		return;
	}

	// If item move was successful, forward the packet to owner & shop
	TPrivateShopItemInfo* pItemInfo = pPrivateShop->MoveItem(p->wPos, p->wChangePos);
	if (pItemInfo)
	{
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketPrivateShopItemMove));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		pPeer->Encode(p, sizeof(TPacketPrivateShopItemMove));

		// Not needed to be sent twice if both player&shop are on the same peer
		if (pPeer != pShopPeer)
		{
			pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketPrivateShopItemMove));
			pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
			pShopPeer->Encode(p, sizeof(TPacketPrivateShopItemMove));
		}

		sys_log(0, "PRIVATE_SHOP: Moved item %u from pos %u -> %u on private shop %u", pItemInfo->TItem.dwID, p->wPos, p->wChangePos, p->dwShopID);
	}
	else
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_CANNOT_MOVE_ITEM;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot move item on private shop %u item pos %u", p->dwShopID, p->wPos);
	}
}

void CClientManager::PrivateShopItemCheckinRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketGDPrivateShopItemCheckin* p = (TPacketGDPrivateShopItemCheckin*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_CHECKIN_REQ;

	if (p->TItem.TPrice.llGold <= 0 || p->TItem.TPrice.llGold > GOLD_MAX || p->TItem.TPrice.dwCheque != 0)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_PRICE;
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		sys_err("PRIVATESHOP_DB: reason=invalid_checkin_price shop=%u item=%u vnum=%u gold=%lld cheque=%u",
			p->dwShopID, p->TItem.dwID, p->TItem.dwVnum, p->TItem.TPrice.llGold, p->TItem.TPrice.dwCheque);
		return;
	}

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwShopID);
		return;
	}

	// Player cannot edit shop's content if its not set to modify state
	if (pPrivateShop->GetState() != STATE_MODIFY)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NOT_MODIFY_STATE;
		    
		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwShopID);
		return;
	}

	TItemTable* pItemTable = GetItemTable(p->TItem.dwVnum);
	if (!pItemTable)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_CHECKIN_FALSE_ITEM;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find proto for item %u", p->TItem.dwVnum);
		return;
	}

	bool bFoundPos = false;
	int iFoundPos = -1;
	if (p->iPos > -1 && p->iPos < PRIVATE_SHOP_HOST_ITEM_MAX_NUM)
	{
		// Try to place an item on the position
		CGrid* pGrid = pPrivateShop->GetGridByPosition(p->iPos);
		WORD wGridPos = p->iPos - (p->iPos / PRIVATE_SHOP_PAGE_ITEM_MAX_NUM) * PRIVATE_SHOP_PAGE_ITEM_MAX_NUM;;

		if (pGrid->IsEmpty(wGridPos, 1, pItemTable->bSize))
		{
			bFoundPos = true;
			iFoundPos = p->iPos;
		}
	}

	// If position was not determined or selected position 
	// is invalid, try to find an empty one
	if (!bFoundPos)
	{
		for (BYTE i = 0; i < PRIVATE_SHOP_PAGE_MAX_NUM; ++i)
		{
			CGrid* pGrid = pPrivateShop->GetGridByPage(i);
			iFoundPos = pGrid->FindBlank(1, pItemTable->bSize);

			// Position was found
			if (iFoundPos != -1)
			{
				iFoundPos += i * PRIVATE_SHOP_PAGE_ITEM_MAX_NUM;
				bFoundPos = true;
				break;
			}

		}
	}

	// Proceed with the item check in
	if (!bFoundPos)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_AVAILABLE_SPACE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_log(0, "Cannot find suitable position for item checkin on private shop %u vnum %u", p->dwShopID, p->TItem.dwVnum);
		return;
	}

	p->TItem.wPos = iFoundPos;

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketGDPrivateShopItemCheckin));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(p, sizeof(TPacketGDPrivateShopItemCheckin));

	sys_log(0, "PRIVATE_SHOP: Item check-in request forwarded to game for private shop %u item %u vnum %u pos %u", p->dwShopID, p->TItem.dwID, p->TItem.dwVnum, iFoundPos);
}

void CClientManager::PrivateShopItemCheckoutRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketGDPrivateShopItemCheckout* p = (TPacketGDPrivateShopItemCheckout*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_CHECKOUT_REQ;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwPID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwPID);
		return;
	}

	// Player cannot edit shop's content if its not set to modify state
	if (pPrivateShop->GetState() != STATE_MODIFY && pPrivateShop->GetState() != STATE_RECOVERY)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NOT_MODIFY_STATE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwPID);
		return;
	}

	// Check if the item exists 
	TPrivateShopItemInfo* pItemInfo = pPrivateShop->GetItem(p->wSrcPos);
	if (!pItemInfo || !pItemInfo->bAvailable)
	{
		sys_err("Could not find item info on pos %u shop %u", p->wSrcPos, p->dwPID);

		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_BUY_RESULT_FALSE_ITEM;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_CHECKOUT_REQ;
	TPacketDGPrivateShopItemCheckout subPacket{};
	subPacket.wSrcPos = p->wSrcPos;
	subPacket.TDstPos = p->TDstPos;

	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketDGPrivateShopItemCheckout));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopItemCheckout));

	sys_log(0, "PRIVATE_SHOP: Item check-out request forwarded to game for private shop %u item %u vnum %u pos %u", p->dwPID, p->TItem.dwID, p->TItem.dwVnum, p->wSrcPos);
}

void CClientManager::PrivateShopTitleChangeRequest(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketPrivateShopTitleChange* p = (TPacketPrivateShopTitleChange*)c_szData;
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_TITLE_CHANGE;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NO_SHOP;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop %u", p->dwShopID);
		return;
	}

	// Player cannot edit shop's content if its not set to modify state
	if (pPrivateShop->GetState() != STATE_MODIFY)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_NOT_MODIFY_STATE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));
		return;
	}

	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SHOP_NOT_AVAILABLE;

		pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE));
		pPeer->Encode(&bSubHeader, sizeof(BYTE));

		sys_err("Cannot find private shop peer for private shop %u", p->dwShopID);
		return;
	}

	// Change title on the SQL
	pPrivateShop->ChangeTitle(p->szTitle);

	// Change title on the owner's interface and on the shop
	pPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, dwHandle, sizeof(BYTE) + sizeof(TPacketPrivateShopTitleChange));
	pPeer->Encode(&bSubHeader, sizeof(BYTE));
	pPeer->Encode(p, sizeof(TPacketPrivateShopTitleChange));

	if (pPeer != pShopPeer)
	{
		pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketPrivateShopTitleChange));
		pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
		pShopPeer->Encode(p, sizeof(TPacketPrivateShopTitleChange));
	}

	sys_log(0, "PRIVATE_SHOP: Title changed on private shop %u (%s)", p->dwShopID, p->szTitle);
}

void CClientManager::PrivateShopBuy(CPeer* pPeer, DWORD dwHandle, const char* c_szData)
{
	TPacketGDPrivateShopBuy* p = (TPacketGDPrivateShopBuy*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->TItem.dwOwner);
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %d", p->TItem.dwOwner);
		return;
	}

	TPrivateShopItemInfo* pItemInfo = pPrivateShop->GetItem(p->TItem.wPos);
	if (!pItemInfo)
	{
		sys_err("Could not find item info on pos %u shop %u", p->TItem.wPos, p->TItem.dwOwner);
		return;
	}

	if (pItemInfo->bAvailable || pItemInfo->dwReservation != p->dwReservation ||
		pItemInfo->dwBuyerPeer != pPeer->GetHandle() || pItemInfo->TItem.dwID != p->TItem.dwID)
	{
		sys_err("PRIVATESHOP_DB: buy_stale shop=%u item=%u token=%u", p->TItem.dwOwner, p->TItem.dwID, p->dwReservation);
		return;
	}
	// Item was already removed on game core
	// Send notification to the private shop owner if online
	CPeer* pOwnerPeer = GetPeer(pPrivateShop->GetOwnerPeerHandle());
	if (pOwnerPeer)
	{
		TPacketDGPrivateShopSaleUpdate packet{};
		strlcpy(packet.szCustomerName, p->szCustomerName, sizeof(packet.szCustomerName));
		std::memcpy(&packet.TItem, &p->TItem, sizeof(packet.TItem));

		// Sale Update
		BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SALE_UPDATE;

		pOwnerPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, pPrivateShop->GetOwnerHandle(), sizeof(BYTE) + sizeof(TPacketDGPrivateShopSaleUpdate));
		pOwnerPeer->Encode(&bSubHeader, sizeof(BYTE));
		pOwnerPeer->Encode(&packet, sizeof(TPacketDGPrivateShopSaleUpdate));
	}

	pPrivateShop->UpdateBalance(pItemInfo->TItem.TPrice.llGold, pItemInfo->TItem.TPrice.dwCheque);
	pPrivateShop->RemoveItem(p->TItem.wPos);

	// Remove the item if shop is not spawned on the same core as the item was bought on
	if (pPeer->GetHandle() != pPrivateShop->GetShopPeerHandle())
	{
		CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
		if (pShopPeer)
		{
			BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_REMOVE_ITEM;
			DWORD dwShopID = pPrivateShop->GetOwner();

			pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(DWORD) + sizeof(WORD));
			pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
			pShopPeer->Encode(&dwShopID, sizeof(DWORD));
			pShopPeer->Encode(&p->TItem.wPos, sizeof(WORD));
		}
		else
		{
			sys_err("Cannot find private shop peer %u", p->TItem.dwOwner);
		}
	}

	sys_log(0, "PRIVATE_SHOP: Item bought from private shop %u pos %u gold %lld won %u by pid %u", pPrivateShop->GetOwner(), p->TItem.wPos, p->TItem.TPrice.llGold, p->TItem.TPrice.dwCheque, p->dwCustomer);

	// Keep earnings recoverable through the owner's panel until withdrawal.
	if (!pPrivateShop->GetItemCount())
	{
		sys_log(0, "PRIVATESHOP_DB: sold_out_balance_retained pid=%u gold=%lld", pPrivateShop->GetOwner(), pPrivateShop->GetGold());
		PrivateShopGameDespawn(pPrivateShop);
	}
}

void CClientManager::PrivateShopFailedBuy(const char* c_szData)
{
	TPacketGDPrivateShopFailedBuy* p = (TPacketGDPrivateShopFailedBuy*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %u", p->dwShopID);
		return;
	}

	TPrivateShopItemInfo* pItemInfo = pPrivateShop->GetItem(p->wPos);
	if (!pItemInfo)
	{
		sys_err("Could not find item info on pos %u shop %u", p->wPos, p->dwShopID);
		return;
	}

	if (pItemInfo->bAvailable || pItemInfo->dwReservation != p->dwReservation)
	{
		sys_log(0, "PRIVATESHOP_DB: cancel_stale shop=%u pos=%u token=%u", p->dwShopID, p->wPos, p->dwReservation);
		return;
	}
	pItemInfo->bAvailable = true;
	pItemInfo->tReservationExpiry = 0;

	sys_log(0, "PRIVATE_SHOP: Unlocking item on private shop %u pos %u", p->dwShopID, p->wPos);
}

void CClientManager::PrivateShopItemTransfer(TPlayerItem* pTItem)
{

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(pTItem->owner); // Currently this function is triggered by the shop owner whose id is the same as shop's one
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %d", pTItem->owner);
		return;
	}

	pPrivateShop->RemoveItemByID(pTItem->id);

	if (!DeletePrivateShopItemCache(pTItem->id))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop_item%s WHERE id=%u", GetTablePostfix(), pTItem->id);
		CDBManager::instance().AsyncQuery(szQuery);
	}

	PutItemCache(pTItem);

	sys_log(1, "PRIVATE_SHOP: Safe item transfer for private shop %u item %u", pTItem->owner, pTItem->id);
}

void CClientManager::PrivateShopItemDelete(const char* c_szData)
{
	TPacketGDPrivateShopItemDelete* p = (TPacketGDPrivateShopItemDelete*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %d", p->dwShopID);
		return;
	}

	if (!DeletePrivateShopItemCache(p->dwItemID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop_item%s WHERE id=%u", GetTablePostfix(), p->dwItemID);
		CDBManager::instance().AsyncQuery(szQuery);
	}

	pPrivateShop->RemoveItemByID(p->dwItemID);
	sys_log(0, "PRIVATE_SHOP: Deleting item on private shop %u item %u", p->dwShopID, p->dwItemID);
}

void CClientManager::PrivateShopItemExpire(const char* c_szData)
{
	TPacketGDPrivateShopItemExpire* p = (TPacketGDPrivateShopItemExpire*)c_szData;

	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(p->dwShopID);
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %d", p->dwShopID);
		return;
	}

	pPrivateShop->RemoveItem(p->wPos);

	CPeer* pOwnerPeer = GetPeer(pPrivateShop->GetOwnerPeerHandle());
	if (pOwnerPeer)
	{
		BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_ITEM_EXPIRE;

		pOwnerPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, pPrivateShop->GetOwnerHandle(), sizeof(BYTE) + sizeof(WORD));
		pOwnerPeer->Encode(&bSubHeader, sizeof(BYTE));
		pOwnerPeer->Encode(&p->wPos, sizeof(WORD));
	}

	sys_log(0, "PRIVATE_SHOP: Removing expired item on private shop %u pos %u", p->dwShopID, p->wPos);

	// No items left but stash still exists
	if (!pPrivateShop->GetItemCount() && (pPrivateShop->GetGold() || pPrivateShop->GetCheque()))
	{
		sys_log(0, "PRIVATE_SHOP: Despawning item empty private shop %u by item expire", p->dwShopID);
		PrivateShopGameDespawn(pPrivateShop);
	}

	// No items and no stash
	else if (!pPrivateShop->GetItemCount() && !pPrivateShop->GetGold() && !pPrivateShop->GetCheque())
	{
		sys_log(0, "PRIVATE_SHOP: Destroying empty private shop %u by item expire", p->dwShopID);
		PrivateShopGameDespawn(pPrivateShop);
		PrivateShopDestroy(pPrivateShop);
	}
}

void CClientManager::PrivateShopPremiumTimeUpdate(const char* c_szData)
{
	TPacketGDPrivateShopPremiumTimeUpdate* p = (TPacketGDPrivateShopPremiumTimeUpdate*)c_szData;

	CLoginData* pLoginData = GetLoginDataByAID(p->dwAID);

	if (!pLoginData)
	{
		sys_err("Could not find login data with account id %d", p->dwAID);
		return;
	}

	time_t tNewPremiumTime = p->tPremiumTime + time(0);

	pLoginData->SetPremium(PREMIUM_PRIVATE_SHOP, tNewPremiumTime);

	char szQuery[512];

	for (int i = 1; i <= PLAYER_PER_ACCOUNT; ++i)
	{
		sprintf(szQuery, "SELECT pid%u FROM player_index%s WHERE id=%u", i, GetTablePostfix(), p->dwAID);
		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery));
		MYSQL_RES* pRes = pMsg->Get()->pSQLResult;

		if (!pRes)
		{
			sys_err("Failed to fetch player index %u result for account %u", i, p->dwAID);
			return;
		}

		auto iRow = mysql_num_rows(pRes);
		if (iRow <= 0)
		{
			sys_err("Failed to fetch player index %u columns for account %u", i, p->dwAID);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pRes);
		DWORD dwPID = 0;
		str_to_number(dwPID, row[0]);

		LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
		if (pPrivateShop)
		{
			sys_log(0, "PRIVATESHOP_DB: account_bonus_keeps_shop_deadline pid=%u", dwPID);
		}
	}

	sprintf(szQuery, "UPDATE account SET premium_privateshop_expire = FROM_UNIXTIME(%u) WHERE id = %u LIMIT 1", tNewPremiumTime, p->dwAID);
	sys_log(0, "PREMIUM PRIVATE SHOP CHANGED ACCOUNT %u DURATION %u", p->dwAID, tNewPremiumTime);
	CDBManager::Instance().AsyncQuery(szQuery, SQL_ACCOUNT);


	sys_log(0, "PRIVATE_SHOP: Updating premium time for aid %u -> %u", p->dwAID, tNewPremiumTime);
}

void CClientManager::PrivateShopStartPremiumEvent(DWORD dwPID)
{
	LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwPID);
	if (!pPrivateShop)
	{
		sys_err("Could not find shop with id %d", dwPID);
		return;
	}

	if (pPrivateShop->GetState() == STATE_CLOSED)
	{
		pPrivateShop->BindOwnerPeerHandle(0);
		pPrivateShop->SetOwnerHandle(0);
		sys_log(0, "PRIVATESHOP_DB: logout_closed pid=%u", dwPID);
		return;
	}
	// Logout never grants additional lifetime.
	if (auto cache = GetPrivateShopCache(dwPID))
		cache->Flush();
	sys_log(0, "PRIVATESHOP_DB: offline_deadline_saved pid=%u deadline=%u", dwPID, static_cast<unsigned>(pPrivateShop->GetPremiumTime()));

	// Update owner's peer to null
	pPrivateShop->BindOwnerPeerHandle(0);
	pPrivateShop->SetOwnerHandle(0);

	sys_log(0, "PRIVATESHOP_DB: owner_detached_deadline_unchanged pid=%u", dwPID);
}

void CClientManager::UpdatePrivateShopPremiumEvent()
{
	for (auto& entry : m_map_privateShop)
	{
		for (auto& item : entry.second->GetItemContainer())
		{
			if (item.bAvailable || item.tReservationExpiry > time(0))
				continue;
			CPeer* peer = GetPeer(item.dwBuyerPeer);
			if (!peer)
			{
				item.bAvailable = true;
				sys_log(0, "PRIVATESHOP_DB: reservation_peer_gone shop=%u item=%u", entry.first, item.TItem.dwID);
				continue;
			}
			TPacketGDPrivateShopFailedBuy packet{};
			packet.dwShopID = entry.first;
			packet.wPos = item.TItem.wPos;
			packet.dwReservation = item.dwReservation;
			BYTE header = PRIVATE_SHOP_DG_SUBHEADER_CANCEL_BUY;
			peer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(header) + sizeof(packet));
			peer->Encode(&header, sizeof(header));
			peer->Encode(&packet, sizeof(packet));
			item.tReservationExpiry = time(0) + 30;
			sys_log(0, "PRIVATESHOP_DB: reservation_timeout_cancel shop=%u item=%u token=%u", entry.first, item.TItem.dwID, item.dwReservation);
		}
	}
	std::vector<DWORD> expired;
	for (const auto& entry : m_map_privateShop)
		if (entry.second->GetState() != STATE_CLOSED && PrivateShopDeadlinePassed(entry.second->GetPremiumTime(), time(0)))
			expired.push_back(entry.first);
	for (DWORD pid : expired)
	{
		LPPRIVATE_SHOP shop = GetPrivateShop(pid);
		if (!shop) continue;
		if (shop->GetState() != STATE_RECOVERY)
		{
			shop->ChangeState(STATE_RECOVERY);
			BYTE header = PRIVATE_SHOP_DG_SUBHEADER_STATE_UPDATE;
			TPacketDGPrivateShopStateUpdate packet{};
			packet.dwPID = pid;
			packet.bState = STATE_RECOVERY;
			CPeer* owner = GetPeer(shop->GetOwnerPeerHandle());
			CPeer* host = GetPeer(shop->GetShopPeerHandle());
			for (CPeer* peer : {owner, host == owner ? nullptr : host})
			{
				if (!peer) continue;
				peer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, peer == owner ? shop->GetOwnerHandle() : 0, sizeof(header) + sizeof(packet));
				peer->Encode(&header, sizeof(header));
				peer->Encode(&packet, sizeof(packet));
			}
			UpdatePrivateShopItemCacheSet(pid);
			sys_log(0, "PRIVATESHOP_DB: expired_recovery_retained pid=%u items=%u gold=%lld", pid, shop->GetItemCount(), shop->GetGold());
		}
		bool reserved = false;
		for (const auto& item : shop->GetItemContainer())
			reserved = reserved || !item.bAvailable;
		if (reserved)
		{
			continue;
		}
		if (!shop->GetItemCount() && !shop->GetGold() && !shop->GetCheque())
			PrivateShopDestroy(shop);
	}
}

// Called upon last item checkout / stash withdrawal
void CClientManager::PrivateShopDestroy(LPPRIVATE_SHOP pPrivateShop)
{
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_CLOSE;

	CPeer* pOwnerPeer = GetPeer(pPrivateShop->GetOwnerPeerHandle());
	DWORD dwPID = pPrivateShop->GetOwner();

	// Shop instance is already destroyed on the game core
	if (pOwnerPeer)
	{
		pOwnerPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, pPrivateShop->GetOwnerHandle(), sizeof(BYTE));
		pOwnerPeer->Encode(&bSubHeader, sizeof(BYTE));
	}

	// Delete private shop cache and SQL record
	if (GetPrivateShopCache(dwPID))
	{
		DeletePrivateShopCache(dwPID);
	}
	else
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop%s WHERE owner_id=%u", GetTablePostfix(), dwPID);

		CDBManager::instance().AsyncQuery(szQuery);
	}

	// Delete private shop item cache and SQL record
	if (GetPrivateShopItemCacheSet(dwPID))
	{
		DeletePrivateShopItemCacheSet(dwPID);
	}
	else
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop_item%s WHERE owner_id=%u", GetTablePostfix(), dwPID);

		CDBManager::instance().AsyncQuery(szQuery);
	}

	DWORD dwShopID = pPrivateShop->GetOwner();

	// @note: Shouldn't happen as we already despawn the private shop by PrivateShopGameDespawn
	{
		if (GetPrivateShop(dwPID))
			DeletePrivateShop(dwShopID);
	}

	sys_log(0, "PRIVATE_SHOP: Destroyed private shop %u", dwShopID);
}

// Called upon last item checkout / last item sale
void CClientManager::PrivateShopGameDespawn(LPPRIVATE_SHOP pPrivateShop)
{
	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_STATE_UPDATE;

	CPeer* pOwnerPeer = GetPeer(pPrivateShop->GetOwnerPeerHandle());
	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());

	TPacketDGPrivateShopStateUpdate subPacket {};
	subPacket.dwPID = pPrivateShop->GetOwner();;
	subPacket.bState = STATE_CLOSED;

	pPrivateShop->ChangeState(STATE_CLOSED);

	if (pOwnerPeer)
	{
		pOwnerPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, pPrivateShop->GetOwnerHandle(), sizeof(BYTE) + sizeof(TPacketDGPrivateShopStateUpdate));
		pOwnerPeer->Encode(&bSubHeader, sizeof(BYTE));
		pOwnerPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopStateUpdate));

		sys_log(0, "PRIVATE_SHOP: Sending despawn update to owner %u", subPacket.dwPID);
	}
	// Despawn only changes visibility. Final destruction belongs to the caller.
	UpdatePrivateShopItemCacheSet(subPacket.dwPID);
	sys_log(0, "PRIVATESHOP_DB: despawn_retained pid=%u items=%u gold=%lld", subPacket.dwPID, pPrivateShop->GetItemCount(), pPrivateShop->GetGold());

	if (pShopPeer && pShopPeer != pOwnerPeer)
	{
		pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPacketDGPrivateShopStateUpdate));
		pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
		pShopPeer->Encode(&subPacket, sizeof(TPacketDGPrivateShopStateUpdate));
	}
}

void CClientManager::PrivateShopGameSpawn(LPPRIVATE_SHOP pPrivateShop)
{
	if (pPrivateShop->GetState() != STATE_CLOSED && pPrivateShop->GetPremiumTime() <= time(0))
	{
		pPrivateShop->ChangeState(STATE_RECOVERY);
		sys_log(0, "PRIVATESHOP_DB: spawn_expired_recovery pid=%u", pPrivateShop->GetOwner());
	}
	if (pPrivateShop->GetState() == STATE_CLOSED)
	{
		sys_log(0, "PRIVATESHOP_DB: spawn_skip_closed pid=%u", pPrivateShop->GetOwner());
		return;
	}
	CPeer* pShopPeer = GetPeer(pPrivateShop->GetShopPeerHandle());
	if (!pShopPeer)
	{
		sys_err("Cannot find peer for private shop %u", pPrivateShop->GetOwner());
		return;
	}

	if (!pPrivateShop->GetItemCount())
	{
		sys_log(0, "PRIVATE_SHOP: Skipping spawning of empty private shop %u", pPrivateShop->GetOwner());
		return;
	}

	BYTE bSubHeader = PRIVATE_SHOP_DG_SUBHEADER_SPAWN;
	WORD wCount = pPrivateShop->GetItemCount();

	pShopPeer->EncodeHeader(HEADER_DG_PRIVATE_SHOP, 0, sizeof(BYTE) + sizeof(TPrivateShop) + sizeof(WORD) + wCount * sizeof(TPlayerPrivateShopItem));
	pShopPeer->Encode(&bSubHeader, sizeof(BYTE));
	pShopPeer->Encode(&pPrivateShop->GetTable(), sizeof(TPrivateShop));
	pShopPeer->Encode(&wCount, sizeof(WORD));

	auto& itemContainer = pPrivateShop->GetItemContainer();
	for (const auto& c_rShopItem : itemContainer)
		pShopPeer->Encode(&c_rShopItem.TItem, sizeof(TPlayerPrivateShopItem));

	sys_log(0, "PRIVATE_SHOP: Spawning private shop %u premium_time %u", pPrivateShop->GetOwner(), pPrivateShop->GetPremiumTime());
}

bool CClientManager::PrivateShopFetchData(DWORD dwShopID, TPrivateShop& rTable, std::vector<TPlayerPrivateShopItem>& c_vec_shopItem)
{
	// Private Shop Information
	{
		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(GetPrivateShopQuery(dwShopID)));
		MYSQL_RES* pRes = pMsg->Get()->pSQLResult;

		if (!CreatePrivateShopTableFromRes(pRes, rTable))
			return false;
	}

	// Items
	{
		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(GetPrivateShopItemQuery(dwShopID)));
		MYSQL_RES* pRes = pMsg->Get()->pSQLResult;

		CreatePrivateShopItemTableFromRes(pRes, &c_vec_shopItem, dwShopID);
	}

	return true;
}

void CClientManager::PrivateShopPeerSpawn(CPeer* pPeer)
{
	size_t sCount = 0;

	if (m_map_privateShop.size())
	{
		sys_log(0, "RESPAWNING UNAVAILABLE PRIVATE SHOPS (CHANNEL %u PORT %u)", pPeer->GetChannel(), pPeer->GetListenPort());

		for (auto it = m_map_privateShop.begin(); it != m_map_privateShop.end(); )
		{
			LPPRIVATE_SHOP pPrivateShop = it->second.get();

			if (pPrivateShop->GetState() == STATE_RECOVERY &&
				pPrivateShop->GetTable().wPort == pPeer->GetListenPort() &&
				pPrivateShop->GetTable().bChannel == pPeer->GetChannel() &&
				!GetPeer(pPrivateShop->GetShopPeerHandle()))
			{
				pPrivateShop->BindShopPeerHandle(pPeer->GetHandle());
				PrivateShopGameSpawn(pPrivateShop);
				sys_log(0, "PRIVATESHOP_DB: recovery_host_rebound pid=%u", pPrivateShop->GetOwner());
			}

			if (pPrivateShop->GetState() != STATE_UNAVAILABLE)
			{
				++it;
				continue;
			}

			bool bDeleteShop = true;

			if (pPrivateShop->GetTable().wPort == pPeer->GetListenPort() && pPrivateShop->GetTable().bChannel == pPeer->GetChannel())
			{
				for (const auto& kv : m_map_pkLoginData)
				{
					CLoginData* pLoginData = kv.second;
					if (pPrivateShop->GetOwner() == pLoginData->GetLastPlayerID() && pLoginData->IsPlay())
					{
						bDeleteShop = false;

						if (pPrivateShop->GetItemCount())
						{
							pPrivateShop->ChangeState(pPrivateShop->GetPremiumTime() <= time(0) ? STATE_RECOVERY : STATE_OPEN);
							pPrivateShop->BindShopPeerHandle(pPeer->GetHandle());
							PrivateShopGameSpawn(pPrivateShop);
						}
						++sCount;
					}
				}
			}

//Darklovers_Fix_Offline_Shop
			if (bDeleteShop)
			{
				it = m_map_privateShop.erase(it);
			}
			else
			{
				++it;
			}
//Darklovers_Fix_Offline_Shop
		}
	}

	{
		std::vector<DWORD> vec_suitablePrivateShops;

		char szQuery[256 + 1] {};

		snprintf(szQuery, sizeof(szQuery),
			"SELECT owner_id "
			"FROM private_shop%s "
			"WHERE premium_time > %u "
			"AND state+0 > %u AND channel=%u AND port=%u"
			, GetTablePostfix(), time(0), STATE_CLOSED, pPeer->GetChannel(), pPeer->GetListenPort());

		sys_log(0, "RESPAWNING PRIVATE SHOPS (CHANNEL %u PORT %u)", pPeer->GetChannel(), pPeer->GetListenPort());

		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery));
		MYSQL_RES* pRes = pMsg->Get()->pSQLResult;

		if (!pRes)
		{
			sys_log(0, "FAILED TO FETCH#1 PRIVATE SHOPS (CHANNEL %u PORT %u)", pPeer->GetChannel(), pPeer->GetListenPort());
			return;
		}

		auto iRow = mysql_num_rows(pRes);
		if (iRow <= 0)
		{
			sys_log(0, "NO PRIVATE SHOPS FOUND (CHANNEL %u PORT %u)", pPeer->GetChannel(), pPeer->GetListenPort());
			return;
		}

		vec_suitablePrivateShops.resize(iRow);

		for (int i = 0; i < static_cast<int>(iRow); ++i)
		{
			MYSQL_ROW row = mysql_fetch_row(pRes);
			DWORD& rShopID = vec_suitablePrivateShops.at(i);

			str_to_number(rShopID, row[0]);
		}

		for (const auto& dwShopID : vec_suitablePrivateShops)
		{
			LPPRIVATE_SHOP pPrivateShop = GetPrivateShop(dwShopID);
			if (pPrivateShop && pPrivateShop->GetState() != STATE_UNAVAILABLE)
				continue;

			PrivateShopSpawn(dwShopID);
			PrivateShopStartPremiumEvent(dwShopID);
			++sCount;
		}
	}

	sys_log(0, "SPAWNED %u PRIVATE SHOPS (CHANNEL %u PORT %u)", sCount, pPeer->GetChannel(), pPeer->GetListenPort());
}

CPeer* CClientManager::GetPrivateShopPeer(BYTE bChannel, WORD wListenPort)
{
	for (const auto& pPeer : m_peerList)
	{
		if (pPeer->GetChannel() == bChannel && pPeer->GetListenPort() == wListenPort)
			return pPeer;
	}

	return nullptr;
}

TItemTable* CClientManager::GetItemTable(DWORD dwVnum)
{
	auto it = m_map_itemTableByVnum.find(dwVnum);
	if (it != m_map_itemTableByVnum.end())
		return it->second;

	for (const auto& pItemTable : m_vec_itemVnumRange)
	{
		if ((pItemTable->dwVnum < dwVnum) &&
			dwVnum < (pItemTable->dwVnum + pItemTable->dwVnumRange))
		{
			return pItemTable;
		}
	}

	return nullptr;
}
