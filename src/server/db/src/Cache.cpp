
#include "stdafx.h"
#include "Cache.h"

#include "QID.h"
#include "ClientManager.h"
#include "Main.h"

extern CPacketInfo g_item_info;
extern int g_iPlayerCacheFlushSeconds;
extern int g_iItemCacheFlushSeconds;
extern int g_test_server;
// MYSHOP_PRICE_LIST
extern int g_iItemPriceListTableCacheFlushSeconds;
// END_OF_MYSHOP_PRICE_LIST
//
extern int g_item_count;
const int auctionMinFlushSec = 1800;

#ifdef WJ_PREMIUM_PRIVATE_SHOP
CPrivateShopCache::CPrivateShopCache()
{
	m_expireTime = MIN(1800, g_iItemCacheFlushSeconds);
}

CPrivateShopCache::~CPrivateShopCache()
{
}

void CPrivateShopCache::Delete()
{
	if (m_data.dwVnum == 0)
		return;

	if (g_test_server)
		sys_log(0, "PRIVATESHOP_DB: cache_delete owner=%u", m_data.dwOwner);

	m_data.dwVnum = 0;
	m_bNeedQuery = true;
	m_lastUpdateTime = time(0);
	OnFlush();
}

void CPrivateShopCache::OnFlush()
{
	char szQuery[1024];

	if (m_data.dwVnum == 0)
	{
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop%s WHERE owner_id=%u", GetTablePostfix(), m_data.dwOwner);
		CDBManager::instance().ReturnQuery(szQuery, QID_PRIVATE_SHOP_DELETE, 0, NULL);

		if (g_test_server)
			sys_log(0, "PRIVATESHOP_DB: cache_flush_delete owner=%u query=%s", m_data.dwOwner, szQuery);
	}
	else
	{
		char szEscapedTitle[TITLE_MAX_LEN * 2 + 1] { 0 };
		char szEscapedOwnerName[CHARACTER_NAME_MAX_LEN * 2 + 1] { 0 };
		CDBManager::instance().EscapeString(szEscapedTitle, m_data.szTitle, strlen(m_data.szTitle));
		CDBManager::instance().EscapeString(szEscapedOwnerName, m_data.szOwnerName, strlen(m_data.szOwnerName));

		snprintf(szQuery, sizeof(szQuery),
			"REPLACE INTO private_shop%s SET "
			"owner_id=%u, owner_name='%s', state=%u, title='%s', title_type=%u, "
			"vnum=%u, x=%ld, y=%ld, map_index=%ld, channel=%u, port=%u, "
			"gold=%lld, cheque=%u, page_count=%u, premium_time=%u, lifetime_seconds=%u",
			GetTablePostfix(), m_data.dwOwner, szEscapedOwnerName, m_data.bState, szEscapedTitle,
			m_data.bTitleType, m_data.dwVnum, m_data.lX, m_data.lY, m_data.lMapIndex,
			m_data.bChannel, m_data.wPort, m_data.llGold, m_data.dwCheque,
			m_data.bPageCount, m_data.tPremiumTime, m_data.dwLifetimeSeconds);

		CDBManager::instance().ReturnQuery(szQuery, QID_PRIVATE_SHOP_SAVE, 0, NULL);
		m_bNeedQuery = false;
	}
}

CPrivateShopItemCache::CPrivateShopItemCache()
{
	m_expireTime = MIN(1800, g_iItemCacheFlushSeconds);
}

CPrivateShopItemCache::~CPrivateShopItemCache()
{
}

void CPrivateShopItemCache::Delete()
{
	if (m_data.dwVnum == 0)
		return;

	if (g_test_server)
		sys_log(0, "PRIVATESHOP_DB: item_cache_delete owner=%u item=%u", m_data.dwOwner, m_data.dwID);

	m_data.dwVnum = 0;
	m_bNeedQuery = true;
	m_lastUpdateTime = time(0);
	OnFlush();
}

void CPrivateShopItemCache::OnFlush()
{
	if (m_data.dwVnum == 0)
	{
		char szQuery[128 + 1];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM private_shop_item%s WHERE id=%u", GetTablePostfix(), m_data.dwID);
		CDBManager::instance().ReturnQuery(szQuery, QID_PRIVATE_SHOP_ITEM_DELETE, 0, NULL);

		if (g_test_server)
			sys_log(0, "PRIVATESHOP_DB: item_cache_flush_delete owner=%u item=%u query=%s", m_data.dwOwner, m_data.dwID, szQuery);
	}
	else
	{
		long alSockets[ITEM_SOCKET_MAX_NUM];
		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
		bool isSocket = false, isAttr = false;

		memset(&alSockets, 0, sizeof(long) * ITEM_SOCKET_MAX_NUM);
		memset(&aAttr, 0, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM);

		TPlayerPrivateShopItem* p = &m_data;

		if (memcmp(alSockets, p->alSockets, sizeof(long) * ITEM_SOCKET_MAX_NUM))
			isSocket = true;

		if (memcmp(aAttr, p->aAttr, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM))
			isAttr = true;

		char szColumns[QUERY_MAX_LEN];
		char szValues[QUERY_MAX_LEN];

		int iLen = snprintf(szColumns, sizeof(szColumns), "id, owner_id, pos, count, vnum, gold, cheque, checkin");
		int iValueLen = snprintf(szValues, sizeof(szValues), "%u, %u, %u, %u, %u, %lld, %u, %u",
			p->dwID, p->dwOwner, p->wPos, p->dwCount, p->dwVnum, p->TPrice.llGold, p->TPrice.dwCheque, p->tCheckin);

		if (isSocket)
		{
			iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", socket0, socket1, socket2");
			iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %lu, %lu, %lu", p->alSockets[0], p->alSockets[1], p->alSockets[2]);
		}

		if (isAttr)
		{
			iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen,
				", attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3"
				", attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6");

			iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
				", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
				p->aAttr[0].bType, p->aAttr[0].sValue,
				p->aAttr[1].bType, p->aAttr[1].sValue,
				p->aAttr[2].bType, p->aAttr[2].sValue,
				p->aAttr[3].bType, p->aAttr[3].sValue,
				p->aAttr[4].bType, p->aAttr[4].sValue,
				p->aAttr[5].bType, p->aAttr[5].sValue,
				p->aAttr[6].bType, p->aAttr[6].sValue);
		}

		char szItemQuery[32768];
		snprintf(szItemQuery, sizeof(szItemQuery), "REPLACE INTO private_shop_item%s (%s) VALUES(%s)", GetTablePostfix(), szColumns, szValues);

		if (g_test_server)
			sys_log(0, "PRIVATESHOP_DB: item_cache_flush_replace query=%s", szItemQuery);

		CDBManager::instance().ReturnQuery(szItemQuery, QID_PRIVATE_SHOP_ITEM_SAVE, 0, NULL);
		m_bNeedQuery = false;
	}
}
#endif
#ifdef __AUCTION__
#include "AuctionManager.h"
#endif
CItemCache::CItemCache()
{
	m_expireTime = MIN(1800, g_iItemCacheFlushSeconds);
}

CItemCache::~CItemCache()
{
}

// �̰� �̻��ѵ�...
// Delete�� ������, Cache�� �����ؾ� �ϴ°� �ƴѰ�???
// �ٵ� Cache�� �����ϴ� �κ��� ����.
// �� ã�� �ǰ�?
// �̷��� �س�����, ��� �ð��� �� ������ �������� ��� ����...
// �̹� ����� �������ε�... Ȯ�λ��??????
// fixme
// by rtsummit
void CItemCache::Delete()
{
	if (m_data.vnum == 0)
		return;

	//char szQuery[QUERY_MAX_LEN];
	//szQuery[QUERY_MAX_LEN] = '\0';
	if (g_test_server)
		sys_log(0, "ItemCache::Delete : DELETE %u", m_data.id);

	m_data.vnum = 0;
	m_bNeedQuery = true;
	m_lastUpdateTime = time(0);
	OnFlush();
	
	//m_bNeedQuery = false;
	//m_lastUpdateTime = time(0) - m_expireTime; // �ٷ� Ÿ�Ӿƿ� �ǵ��� ����.
}

void CItemCache::OnFlush()
{
	if (m_data.vnum == 0) // vnum�� 0�̸� �����϶�� ǥ�õ� ���̴�.
	{
		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", GetTablePostfix(), m_data.id);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_DESTROY, 0, NULL);

		if (g_test_server)
			sys_log(0, "ItemCache::Flush : DELETE %u %s", m_data.id, szQuery);
	}
	else
	{
		long alSockets[ITEM_SOCKET_MAX_NUM];
		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
		bool isSocket = false, isAttr = false;

		memset(&alSockets, 0, sizeof(long) * ITEM_SOCKET_MAX_NUM);
		memset(&aAttr, 0, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM);

		TPlayerItem * p = &m_data;

		if (memcmp(alSockets, p->alSockets, sizeof(long) * ITEM_SOCKET_MAX_NUM))
			isSocket = true;

		if (memcmp(aAttr, p->aAttr, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM))
			isAttr = true;

		char szColumns[QUERY_MAX_LEN];
		char szValues[QUERY_MAX_LEN];
		char szUpdate[QUERY_MAX_LEN];

		int iLen = snprintf(szColumns, sizeof(szColumns), "id, owner_id, window, pos, count, vnum");

		int iValueLen = snprintf(szValues, sizeof(szValues), "%u, %u, %d, %d, %u, %u",
				p->id, p->owner, p->window, p->pos, p->count, p->vnum);

		int iUpdateLen = snprintf(szUpdate, sizeof(szUpdate), "owner_id=%u, window=%d, pos=%d, count=%u, vnum=%u",
				p->owner, p->window, p->pos, p->count, p->vnum);

		if (isSocket)
		{
			iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", socket0, socket1, socket2");
			iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
					", %lu, %lu, %lu", p->alSockets[0], p->alSockets[1], p->alSockets[2]);
			iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
					", socket0=%lu, socket1=%lu, socket2=%lu", p->alSockets[0], p->alSockets[1], p->alSockets[2]);
		}

		if (isAttr)
		{
			iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, 
					", attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3"
					", attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6");

			iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
					", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
					p->aAttr[0].bType, p->aAttr[0].sValue,
					p->aAttr[1].bType, p->aAttr[1].sValue,
					p->aAttr[2].bType, p->aAttr[2].sValue,
					p->aAttr[3].bType, p->aAttr[3].sValue,
					p->aAttr[4].bType, p->aAttr[4].sValue,
					p->aAttr[5].bType, p->aAttr[5].sValue,
					p->aAttr[6].bType, p->aAttr[6].sValue);

			iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
					", attrtype0=%d, attrvalue0=%d"
					", attrtype1=%d, attrvalue1=%d"
					", attrtype2=%d, attrvalue2=%d"
					", attrtype3=%d, attrvalue3=%d"
					", attrtype4=%d, attrvalue4=%d"
					", attrtype5=%d, attrvalue5=%d"
					", attrtype6=%d, attrvalue6=%d",
					p->aAttr[0].bType, p->aAttr[0].sValue,
					p->aAttr[1].bType, p->aAttr[1].sValue,
					p->aAttr[2].bType, p->aAttr[2].sValue,
					p->aAttr[3].bType, p->aAttr[3].sValue,
					p->aAttr[4].bType, p->aAttr[4].sValue,
					p->aAttr[5].bType, p->aAttr[5].sValue,
					p->aAttr[6].bType, p->aAttr[6].sValue);
		}

		char szItemQuery[QUERY_MAX_LEN + QUERY_MAX_LEN];
		snprintf(szItemQuery, sizeof(szItemQuery), "REPLACE INTO item%s (%s) VALUES(%s)", GetTablePostfix(), szColumns, szValues);

		if (g_test_server)	
			sys_log(0, "ItemCache::Flush :REPLACE  (%s)", szItemQuery);

		CDBManager::instance().ReturnQuery(szItemQuery, QID_ITEM_SAVE, 0, NULL);

		//g_item_info.Add(p->vnum);
		++g_item_count;
	}

	m_bNeedQuery = false;
}

//
// CPlayerTableCache
//
CPlayerTableCache::CPlayerTableCache()
{
	m_expireTime = MIN(1800, g_iPlayerCacheFlushSeconds);
}

CPlayerTableCache::~CPlayerTableCache()
{
}

void CPlayerTableCache::OnFlush()
{
	if (g_test_server)
		sys_log(0, "PlayerTableCache::Flush : %s", m_data.name);

	char szQuery[QUERY_MAX_LEN];
	CreatePlayerSaveQuery(szQuery, sizeof(szQuery), &m_data);
	CDBManager::instance().ReturnQuery(szQuery, QID_PLAYER_SAVE, 0, NULL);
}

// MYSHOP_PRICE_LIST
//
// CItemPriceListTableCache class implementation
//

const int CItemPriceListTableCache::s_nMinFlushSec = 1800;

CItemPriceListTableCache::CItemPriceListTableCache()
{
	m_expireTime = MIN(s_nMinFlushSec, g_iItemPriceListTableCacheFlushSeconds);
}

void CItemPriceListTableCache::UpdateList(const TItemPriceListTable* pUpdateList)
{
	//
	// �̹� ĳ�̵� �����۰� �ߺ��� �������� ã�� �ߺ����� �ʴ� ���� ������ tmpvec �� �ִ´�.
	//

	std::vector<TItemPriceInfo> tmpvec;

	for (uint idx = 0; idx < m_data.byCount; ++idx)
	{
		const TItemPriceInfo* pos = pUpdateList->aPriceInfo;
		for (; pos != pUpdateList->aPriceInfo + pUpdateList->byCount && m_data.aPriceInfo[idx].dwVnum != pos->dwVnum; ++pos)
			;

		if (pos == pUpdateList->aPriceInfo + pUpdateList->byCount)
			tmpvec.push_back(m_data.aPriceInfo[idx]);
	}

	//
	// pUpdateList �� m_data �� �����ϰ� ���� ������ tmpvec �� �տ��� ���� ���� ��ŭ �����Ѵ�.
	// 

	if (pUpdateList->byCount > SHOP_PRICELIST_MAX_NUM)
	{
		sys_err("Count overflow!");
		return;
	}

	m_data.byCount = pUpdateList->byCount;

	thecore_memcpy(m_data.aPriceInfo, pUpdateList->aPriceInfo, sizeof(TItemPriceInfo) * pUpdateList->byCount);

	int nDeletedNum;	// ������ ���������� ����

	if (pUpdateList->byCount < SHOP_PRICELIST_MAX_NUM)
	{
		size_t sizeAddOldDataSize = SHOP_PRICELIST_MAX_NUM - pUpdateList->byCount;

		if (tmpvec.size() < sizeAddOldDataSize)
			sizeAddOldDataSize = tmpvec.size();

		if (sizeAddOldDataSize > 0)
			thecore_memcpy(m_data.aPriceInfo + pUpdateList->byCount, &tmpvec[0], sizeof(TItemPriceInfo) * sizeAddOldDataSize);
		m_data.byCount += sizeAddOldDataSize;

		nDeletedNum = tmpvec.size() - sizeAddOldDataSize;
	}
	else
		nDeletedNum = tmpvec.size();

	m_bNeedQuery = true;

	sys_log(0, 
			"ItemPriceListTableCache::UpdateList : OwnerID[%u] Update [%u] Items, Delete [%u] Items, Total [%u] Items", 
			m_data.dwOwnerID, pUpdateList->byCount, nDeletedNum, m_data.byCount);
}

void CItemPriceListTableCache::OnFlush()
{
	char szQuery[QUERY_MAX_LEN];

	//
	// �� ĳ���� �����ڿ� ���� ������ DB �� ����� ������ ���������� ��� �����Ѵ�.
	//

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM myshop_pricelist%s WHERE owner_id = %u", GetTablePostfix(), m_data.dwOwnerID);
	CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_DESTROY, 0, NULL);

	//
	// ĳ���� ������ ��� DB �� ����.
	//

	for (int idx = 0; idx < m_data.byCount; ++idx)
	{
		snprintf(szQuery, sizeof(szQuery),
				// "INSERT INTO myshop_pricelist%s(owner_id, item_vnum, price) VALUES(%u, %u, %u)", 
				"REPLACE myshop_pricelist%s (owner_id, item_vnum, price) VALUES(%u, %u, %u)", 
				GetTablePostfix(), m_data.dwOwnerID, m_data.aPriceInfo[idx].dwVnum, m_data.aPriceInfo[idx].dwPrice);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_SAVE, 0, NULL);
	}

	sys_log(0, "ItemPriceListTableCache::Flush : OwnerID[%u] Update [%u]Items", m_data.dwOwnerID, m_data.byCount);
	
	m_bNeedQuery = false;
}
// END_OF_MYSHOP_PRICE_LIST
#ifdef __AUCTION__
CAuctionItemInfoCache::CAuctionItemInfoCache()
{
	m_expireTime = MIN (auctionMinFlushSec, g_iItemCacheFlushSeconds);
}

CAuctionItemInfoCache::~CAuctionItemInfoCache()
{

}

void CAuctionItemInfoCache::Delete()
{
	if (m_data.item_num == 0)
		return;

	if (g_test_server)
		sys_log(0, "CAuctionItemInfoCache::Delete : DELETE %u", m_data.item_id);

	m_data.item_num = 0;
	m_bNeedQuery = true;
	m_lastUpdateTime = time(0);
	OnFlush();
	delete this;
}

void CAuctionItemInfoCache::OnFlush()
{
	char szQuery[QUERY_MAX_LEN];
	
	if (m_data.item_num == 0)
	{
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM auction where item_id = %d", m_data.item_id);
		CDBManager::instance().AsyncQuery(szQuery);
	}
	else
	{
		snprintf(szQuery, sizeof(szQuery), "REPLACE INTO auction VALUES (%u, %d, %d, %u, \"%s\", %u, %u, %u, %u)", 
			m_data.item_num, m_data.offer_price, m_data.price, m_data.offer_id, m_data.shown_name, (DWORD)m_data.empire, (DWORD)m_data.expired_time, 
			m_data.item_id, m_data.bidder_id);

		CDBManager::instance().AsyncQuery(szQuery);
	}
}

CSaleItemInfoCache::CSaleItemInfoCache()
{
	m_expireTime = MIN (auctionMinFlushSec, g_iItemCacheFlushSeconds);
}

CSaleItemInfoCache::~CSaleItemInfoCache()
{
}

void CSaleItemInfoCache::Delete()
{
}

void CSaleItemInfoCache::OnFlush()
{
	char szQuery[QUERY_MAX_LEN];
	
	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO sale VALUES (%u, %d, %d, %u, \"%s\", %u, %u, %u, %u)", 
		m_data.item_num, m_data.offer_price, m_data.price, m_data.offer_id, m_data.shown_name, (DWORD)m_data.empire, (DWORD)m_data.expired_time,
		m_data.item_id, m_data.wisher_id);
	
	CDBManager::instance().AsyncQuery(szQuery);
}

CWishItemInfoCache::CWishItemInfoCache()
{
	m_expireTime = MIN (auctionMinFlushSec, g_iItemCacheFlushSeconds);
}

CWishItemInfoCache::~CWishItemInfoCache()
{
}

void CWishItemInfoCache::Delete()
{
}

void CWishItemInfoCache::OnFlush()
{
	char szQuery[QUERY_MAX_LEN];
	
	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO wish VALUES (%u, %d, %d, %u, \"%s\", %u, %d)", 
		m_data.item_num, m_data.offer_price, m_data.price, m_data.offer_id, m_data.shown_name, (DWORD)m_data.empire, (DWORD)m_data.expired_time);
	
	CDBManager::instance().AsyncQuery(szQuery);
}
#endif
