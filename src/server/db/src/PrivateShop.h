#pragma once

class CPeer;
class CGrid;

typedef struct SPrivateShopItemInfo
{
	TPlayerPrivateShopItem TItem;
	TItemTable* pItemProto;

	bool	bAvailable;
	DWORD dwReservation;
	DWORD dwBuyerPeer;
	time_t tReservationExpiry;
} TPrivateShopItemInfo;

class CPrivateShop
{
	public:
		CPrivateShop();
		virtual ~CPrivateShop();
		void						SetTable(TPrivateShop* pTable) { m_table = *pTable; }
		TPrivateShop&				GetTable() { return m_table; }

		void						SetOwner(DWORD dwOwner) { m_table.dwOwner = dwOwner; }
		DWORD						GetOwner() { return m_table.dwOwner; }

		void						BindShopPeerHandle(DWORD dwHandle) { m_dwShopPeerHandle = dwHandle; }
		DWORD						GetShopPeerHandle() { return m_dwShopPeerHandle; }

		void						BindOwnerPeerHandle(DWORD dwHandle) { m_dwOwnerPeerHandle = dwHandle; }
		DWORD						GetOwnerPeerHandle() { return m_dwOwnerPeerHandle; }

		WORD						GetPort() { return m_table.wPort; }
		BYTE						GetChannel() { return m_table.bChannel; }

		void						SetOwnerHandle(DWORD dwHandle) { m_dwOwnerHandle = dwHandle; }
		DWORD						GetOwnerHandle() { return m_dwOwnerHandle; }

		void						SetState(BYTE bState) { m_table.bState = bState; }
		void						ChangeState(BYTE bState);
		BYTE						GetState() { return m_table.bState; }

		void						ChangeGold(long long llGold) { m_table.llGold += llGold; }
		long long					GetGold() { return m_table.llGold; }
		void						ChangeCheque(DWORD dwCheque) { m_table.dwCheque += dwCheque; }
		DWORD						GetCheque() { return m_table.dwCheque; }

		void						SetPremiumTime(time_t tPremiumTime) { m_table.tPremiumTime = tPremiumTime; }
		void						UpdatePremiumTime(time_t tPremiumTime);
		time_t						GetPremiumTime() { return m_table.tPremiumTime; }

		void						ChangeTitle(const char* c_szTitle);

		TPrivateShopItemInfo*				SetItem(const TPlayerPrivateShopItem* pItem, TItemTable* pItemProto);
		void								RemoveItem(WORD wPos);
		void								RemoveItemByID(DWORD dwID);
		void								RemoveAllItem();

		std::vector<TPrivateShopItemInfo>&	GetItemContainer() { return m_vec_privateShopItem; }
		TPrivateShopItemInfo*				GetItem(WORD wPos);
		WORD								GetItemCount();

		CGrid*						GetGridByPosition(WORD wPos);
		CGrid*						GetGridByPage(BYTE bPage);

		bool						Buy(WORD wPos);
		void						UpdateBalance(long long llGold, DWORD dwCheque);
		void						Withdraw(long long gold, DWORD cheque);
		bool						ChangeItemPrice(WORD wPos, long long llGold, DWORD dwCheque);
		TPrivateShopItemInfo*		MoveItem(WORD wPos, WORD wChangePos);

	private:
		DWORD						m_dwShopPeerHandle;
		DWORD						m_dwOwnerPeerHandle;
		DWORD						m_dwOwnerHandle;
		TPrivateShop				m_table;

		std::vector<TPrivateShopItemInfo> m_vec_privateShopItem;
		CGrid* m_pGrid[PRIVATE_SHOP_PAGE_MAX_NUM];
};

typedef CPrivateShop* LPPRIVATE_SHOP;

