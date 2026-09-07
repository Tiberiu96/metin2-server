#pragma once
#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"

static const char* GetPrivateShopQuery(DWORD dwOwner)
{
	static char szQuery[512 + 1]{};
	snprintf(szQuery, sizeof(szQuery),
		"SELECT owner_id, owner_name, state+0, title, title_type, vnum, x, y, map_index, channel, port, gold, cheque, page_count, premium_time, lifetime_seconds "
		"FROM private_shop%s "
		"WHERE owner_id = %u ",
		GetTablePostfix(), dwOwner);

	return szQuery;
}

static const char* GetPrivateShopItemQuery(DWORD dwOwner)
{
	static char szQuery[QUERY_MAX_LEN] {};
	snprintf(szQuery, sizeof(szQuery),
		"SELECT id, pos, count, vnum, gold, cheque, checkin, "
		"socket0, "
		"socket1, "
		"socket2, "
		"attrtype0, attrvalue0, "
		"attrtype1, attrvalue1, "
		"attrtype2, attrvalue2, "
		"attrtype3, attrvalue3, "
		"attrtype4, attrvalue4, "
		"attrtype5, attrvalue5, "
		"attrtype6, attrvalue6 "
		"FROM private_shop_item%s WHERE owner_id = %d",
		GetTablePostfix(), dwOwner);

	return szQuery;
}

inline bool CreatePrivateShopTableFromRes(MYSQL_RES* pRes, TPrivateShop& rTable)
{
	if (!pRes)
		return false;

	auto iRow = mysql_num_rows(pRes);
	if (iRow <= 0)
		return false;

	MYSQL_ROW row = mysql_fetch_row(pRes);
	int col = 0;

	str_to_number(rTable.dwOwner, row[col++]);
	strlcpy(rTable.szOwnerName, row[col++], sizeof(rTable.szOwnerName));
	str_to_number(rTable.bState, row[col++]);
	strlcpy(rTable.szTitle, row[col++], sizeof(rTable.szTitle));
	str_to_number(rTable.bTitleType, row[col++]);
	str_to_number(rTable.dwVnum, row[col++]);
	str_to_number(rTable.lX, row[col++]);
	str_to_number(rTable.lY, row[col++]);
	str_to_number(rTable.lMapIndex, row[col++]);
	str_to_number(rTable.bChannel, row[col++]);
	str_to_number(rTable.wPort, row[col++]);
	str_to_number(rTable.llGold, row[col++]);
	str_to_number(rTable.dwCheque, row[col++]);
	str_to_number(rTable.bPageCount, row[col++]);
	str_to_number(rTable.tPremiumTime, row[col++]);
	str_to_number(rTable.dwLifetimeSeconds, row[col++]);

	return true;
}

inline bool CreatePrivateShopItemTableFromRes(MYSQL_RES* pRes, std::vector<TPlayerPrivateShopItem>* pVec, DWORD dwPID)
{
	if (!pRes)
	{
		pVec->clear();
		return false;
	}

	auto iRow = mysql_num_rows(pRes);
	if (iRow <= 0)
	{
		pVec->clear();
		return false;
	}

	pVec->resize(iRow);

	for (int i = 0; i < static_cast<int>(iRow); ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(pRes);
		TPlayerPrivateShopItem& item = pVec->at(i);

		int cur = 0;

		str_to_number(item.dwID, row[cur++]);
		str_to_number(item.wPos, row[cur++]);
		str_to_number(item.dwCount, row[cur++]);
		str_to_number(item.dwVnum, row[cur++]);
		str_to_number(item.TPrice.llGold, row[cur++]);
		str_to_number(item.TPrice.dwCheque, row[cur++]);
		str_to_number(item.tCheckin, row[cur++]);
		str_to_number(item.alSockets[0], row[cur++]);
		str_to_number(item.alSockets[1], row[cur++]);
		str_to_number(item.alSockets[2], row[cur++]);

		for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++)
		{
			str_to_number(item.aAttr[j].bType, row[cur++]);
			str_to_number(item.aAttr[j].sValue, row[cur++]);
		}

		item.dwOwner = dwPID;
	}

	return true;
}
