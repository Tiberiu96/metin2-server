#include "stdafx.h"
#include "private_shop_util.h"
#include "packet.h"
#include "item.h"
#include "char.h"
#include "private_shop.h"
#include "private_shop_manager.h"

bool CanBuildPrivateShop(LPCHARACTER ch)
{
	switch (ch->GetMapIndex())
	{
	case 1:
	case 21:
	case 41:
		return true;

	default:
		return false;
	}
}

DWORD GetPrivateShopBundleVnum(DWORD dwPolyVnum, BYTE bTitleType, BYTE bPageCount)
{
	const bool bPremiumBuild = (dwPolyVnum != 30000 || bTitleType != 0 || bPageCount == PRIVATE_SHOP_PAGE_MAX_NUM);
	return bPremiumBuild ? PRIVATE_SHOP_PREMIUM_BUNDLE_VNUM : PRIVATE_SHOP_BUNDLE_VNUM;
}

bool CheckTradeWindows(LPCHARACTER ch)
{
	// Default windows
	if (
		ch->GetExchange() ||
		ch->IsOpenSafebox() ||
		ch->GetMyShop() ||
		ch->GetShopOwner() ||
		ch->IsCubeOpen() ||
		ch->IsRefineThroughGuild()
		)
		return false;

	return true;
}

int GetEmptyInventory(LPCHARACTER pChar, LPITEM pItem /* = nullptr */)
{
	int iPos = -1;

	if (pItem->IsDragonSoul())
	{
		iPos = pChar->GetEmptyDragonSoulInventory(pItem);
	}
	else
	{
		iPos = pChar->GetEmptyInventory(pItem->GetSize());
	}

	return iPos;
}

void CopyItemData(LPITEM pItem, TPlayerItem& rTargetTable)
{
	rTargetTable.id = pItem->GetID();

	if (pItem->GetOwner())
	{
		rTargetTable.owner = pItem->GetOwner()->GetPlayerID();
	}
	else if (pItem->GetPrivateShop())
	{
		rTargetTable.owner = pItem->GetPrivateShop()->GetID();
	}
	else
	{
		rTargetTable.owner = 0;
		sys_err("Could not find owner of the item %u", pItem->GetID());
	}

	rTargetTable.vnum = pItem->GetVnum();
	rTargetTable.count = pItem->GetCount();
	rTargetTable.pos = pItem->GetCell();
	rTargetTable.window = pItem->GetWindow();
	memcpy(rTargetTable.alSockets, pItem->GetSockets(), sizeof(rTargetTable.alSockets));
	memcpy(rTargetTable.aAttr, pItem->GetAttributes(), sizeof(rTargetTable.aAttr));
}

/*		LPITEM -> TPlayerPrivateShopItem	*/
void CopyItemData(LPITEM pItem, TPlayerPrivateShopItem& rTargetTable)
{
	rTargetTable.dwID = pItem->GetID();

	if (pItem->GetOwner())
	{
		rTargetTable.dwOwner = pItem->GetOwner()->GetPlayerID();
	}
	else if (pItem->GetPrivateShop())
	{
		rTargetTable.dwOwner = pItem->GetPrivateShop()->GetID();
	}
	else
	{
		rTargetTable.dwOwner = 0;
		sys_err("Could not find owner of the item %u", pItem->GetID());
	}

	rTargetTable.dwVnum = pItem->GetVnum();
	rTargetTable.dwCount = pItem->GetCount();
	rTargetTable.wPos = pItem->GetCell();
	rTargetTable.TPrice.llGold = pItem->GetGoldPrice();
	rTargetTable.TPrice.dwCheque = pItem->GetChequePrice();
	rTargetTable.tCheckin = pItem->GetCheckinTime();
	memcpy(rTargetTable.alSockets, pItem->GetSockets(), sizeof(rTargetTable.alSockets));
	memcpy(rTargetTable.aAttr, pItem->GetAttributes(), sizeof(rTargetTable.aAttr));
}

/*		LPITEM -> TPrivateShopItemData	*/
void CopyItemData(LPITEM pItem, TPrivateShopItemData& rTargetTable)
{
	rTargetTable.dwVnum = pItem->GetVnum();
	rTargetTable.dwCount = pItem->GetCount();
	rTargetTable.wPos = pItem->GetCell();
	rTargetTable.TPrice.llGold = pItem->GetGoldPrice();
	rTargetTable.TPrice.dwCheque = pItem->GetChequePrice();
	rTargetTable.tCheckin = pItem->GetCheckinTime();
	memcpy(rTargetTable.alSockets, pItem->GetSockets(), sizeof(rTargetTable.alSockets));
	memcpy(rTargetTable.aAttr, pItem->GetAttributes(), sizeof(rTargetTable.aAttr));
}

/*		LPITEM -> TPrivateShopSearchData	*/
void CopyItemData(LPITEM pItem, TPrivateShopSearchData& rTargetTable)
{
	rTargetTable.dwShopID = pItem->GetPrivateShop()->GetID();
	strlcpy(rTargetTable.szOwnerName, pItem->GetPrivateShop()->GetOwnerName().c_str(), sizeof(rTargetTable.szOwnerName));
	rTargetTable.dwVnum = pItem->GetVnum();
	rTargetTable.dwCount = pItem->GetCount();
	rTargetTable.wPos = pItem->GetCell();
	rTargetTable.TPrice.llGold = pItem->GetGoldPrice();
	rTargetTable.TPrice.dwCheque = pItem->GetChequePrice();
	memcpy(rTargetTable.alSockets, pItem->GetSockets(), sizeof(rTargetTable.alSockets));
	memcpy(rTargetTable.aAttr, pItem->GetAttributes(), sizeof(rTargetTable.aAttr));
	rTargetTable.tCheckin = pItem->GetCheckinTime();
}

/*		TPlayerPrivateShopItem -> LPITEM	*/
void CopyItemData(const TPlayerPrivateShopItem& rSourceTable, LPITEM pItem, LPCHARACTER pOwner /* = nullptr */)
{
	pItem->SetGoldPrice(rSourceTable.TPrice.llGold);
	pItem->SetChequePrice(rSourceTable.TPrice.dwCheque);
	pItem->SetSockets(rSourceTable.alSockets);
	pItem->SetAttributes(rSourceTable.aAttr);
	pItem->SetCell(pOwner, rSourceTable.wPos);
	pItem->SetCheckinTime(rSourceTable.tCheckin);
}

/*		TPlayerPrivateShopItem -> TPrivateShopItemData		*/
void CopyItemData(const TPlayerPrivateShopItem& rSourceTable, TPrivateShopItemData& rTargetTable)
{
	rTargetTable.dwVnum = rSourceTable.dwVnum;
	rTargetTable.dwCount = rSourceTable.dwCount;
	rTargetTable.wPos = rSourceTable.wPos;
	rTargetTable.TPrice.llGold = rSourceTable.TPrice.llGold;
	rTargetTable.TPrice.dwCheque = rSourceTable.TPrice.dwCheque;
	rTargetTable.tCheckin = rSourceTable.tCheckin;
	thecore_memcpy(rTargetTable.alSockets, rSourceTable.alSockets, sizeof(rTargetTable.alSockets));
	thecore_memcpy(rTargetTable.aAttr, rSourceTable.aAttr, sizeof(rTargetTable.aAttr));
}

