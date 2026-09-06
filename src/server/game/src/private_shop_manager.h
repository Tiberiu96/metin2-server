#pragma once
#include <unordered_map>
#include <unordered_set>
#include "../../common/tables.h"
#include "packet.h"
#include "private_shop.h"
#include <memory>

class CItem;
class TEMP_BUFFER;
class CPrivateShopManager : public singleton<CPrivateShopManager>
{
	public:

		CPrivateShopManager() : m_dwVIDCount(0) {}
		~CPrivateShopManager() = default;

		void				Destroy();

		LPPRIVATE_SHOP		CreatePrivateShop(DWORD dwPID);
		LPPRIVATE_SHOP		GetPrivateShop(DWORD dwPID);
		LPPRIVATE_SHOP		GetPrivateShopByOwnerName(const char* c_szOwnerName);
		LPPRIVATE_SHOP		GetPrivateShopByVID(DWORD dwVID);
		bool				DeletePrivateShop(DWORD dwPID);
		DWORD				AllocVID() { return ++m_dwVIDCount; }

		bool				BuildPrivateShop(LPCHARACTER pShopOwner, const char* c_szTitle, DWORD dwPolyVnum, BYTE bTitleType, BYTE bPageCount, const std::vector<TPrivateShopItem>& c_vec_shopItem);
		void				BuildPrivateShopResult(DWORD dwPID, TPrivateShop* pPrivateShopTable, bool bSuccess);
		void PreservePendingBuild(LPCHARACTER owner);
		void SetPendingBuildBundle(DWORD pid, const TPlayerItem& bundle);
		void				ClosePrivateShop(LPCHARACTER pShopOwner);

		void				SpawnPrivateShop(TPrivateShop* pTab, const std::vector<TPlayerPrivateShopItem>& c_vec_shopItem);
		void				DespawnPrivateShop(DWORD dwPID);

		bool				StopShopping(LPCHARACTER pShopViewer);

		void				ItemCheckin(LPCHARACTER pOwner, const TPlayerPrivateShopItem* c_pShopItem);
		void				ItemCheckout(LPCHARACTER pOwner, WORD wSrcPos, TItemPos TDstPos);

		bool				ItemTransaction(LPCHARACTER pCustomer, TPlayerPrivateShopItem* c_pShopItem, DWORD dwReservation);
		void				SendItemTransaction(const TPlayerPrivateShopItem* c_pShopItem, LPCHARACTER pCustomer, DWORD dwReservation);
		void				SendItemTransactionFailedResult(DWORD dwShopID, WORD wPos, DWORD dwReservation);
		void				SendItemTransfer(const TPlayerItem* c_pItem);
		void				SendItemExpire(LPITEM pItem);

		void				AddSearchItem(LPITEM pItem);
		void				RemoveSearchItem(LPITEM pItem);
		void				SearchItem(LPDESC pDesc, TPrivateShopSearchFilter& rFilter, bool bUseFilter, DWORD dwCustomerPID = 0);//Darklovers_Fix_Offline_Shop

		typedef std::unordered_map<DWORD, std::unique_ptr<CPrivateShop> >	TPrivateShopMap;
		typedef std::unordered_map<DWORD, LPPRIVATE_SHOP>					TPrivateShopVIDMap;
		typedef std::unordered_map<DWORD, TItemPrice>						TMarketPriceMap;

		typedef std::unordered_set<LPITEM>					TItemList;
		typedef std::unordered_map<BYTE, TItemList>			TSubTypeItemMap;
		typedef std::unordered_map<BYTE, TSubTypeItemMap>	TTypeItemMap;

	private:
		struct PendingBuild
		{
			std::vector<TPlayerPrivateShopItem> items;
			TPlayerItem bundle{};
			DWORD ownerHandle = 0;
			bool disconnected = false;
		};
		std::unordered_map<DWORD, PendingBuild> m_pendingBuilds;
		DWORD				m_dwVIDCount;
		TPrivateShopMap		m_map_privateShop;
		TPrivateShopVIDMap	m_map_privateShopVID;
		TTypeItemMap		m_map_searchItem;
};

