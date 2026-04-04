#ifndef __INC_ITEM_MANAGER__
#define __INC_ITEM_MANAGER__

#ifdef M2_USE_POOL
#include "pool.h"
#endif

// special_item_group.txt���� �����ϴ� �Ӽ� �׷�
// type attr�� ������ �� �ִ�.
// �� �Ӽ� �׷��� �̿��� �� �ִ� ���� special_item_group.txt���� Special type���� ���ǵ� �׷쿡 ���� UNIQUE ITEM�̴�.
class CSpecialAttrGroup
{
public:
	CSpecialAttrGroup(DWORD vnum)
		: m_dwVnum(vnum)
	{}
	struct CSpecialAttrInfo
	{
		CSpecialAttrInfo (DWORD _apply_type, DWORD _apply_value)
			: apply_type(_apply_type), apply_value(_apply_value)
		{}
		DWORD apply_type;
		DWORD apply_value;

	};
	DWORD m_dwVnum;
	std::string	m_stEffectFileName;
	std::vector<CSpecialAttrInfo> m_vecAttrs;
};

class CSpecialItemGroup
{
	public:
		enum EGiveType
		{
			NONE,
			GOLD,
			EXP,
			MOB,
			SLOW,
			DRAIN_HP,
			POISON,
			MOB_GROUP,
		};

		// QUEST Ÿ���� ����Ʈ ��ũ��Ʈ���� vnum.sig_use�� ����� �� �ִ� �׷��̴�.
		//		��, �� �׷쿡 ���� ���ؼ��� ITEM ��ü�� TYPE�� QUEST���� �Ѵ�.
		// SPECIAL Ÿ���� idx, item_vnum, attr_vnum�� �Է��Ѵ�. attr_vnum�� ���� CSpecialAttrGroup�� Vnum�̴�.
		//		�� �׷쿡 ����ִ� �������� ���� ������ �� ����.
		enum ESIGType { NORMAL, PCT, QUEST, SPECIAL };

		struct CSpecialItemInfo
		{
			DWORD vnum;
			int count;
			int rare;

			CSpecialItemInfo(DWORD _vnum, int _count, int _rare)
				: vnum(_vnum), count(_count), rare(_rare)
				{}
		};

		CSpecialItemGroup(DWORD vnum, BYTE type=0)
			: m_dwVnum(vnum), m_bType(type)
			{}

		void AddItem(DWORD vnum, int count, int prob, int rare)
		{
			if (!prob)
				return;
			if (!m_vecProbs.empty())
				prob += m_vecProbs.back();
			m_vecProbs.push_back(prob);
			m_vecItems.push_back(CSpecialItemInfo(vnum, count, rare));
		}

		bool IsEmpty() const
		{
			return m_vecProbs.empty();
		}

		// Type Multi, �� m_bType == PCT �� ���,
		// Ȯ���� ���ذ��� �ʰ�, ���������� ����Ͽ� �������� �����Ѵ�.
		// ���� ���� ���� �������� ������ �� �ִ�.
		// by rtsummit
		int GetMultiIndex(std::vector <int> &idx_vec) const
		{
			idx_vec.clear();
			if (m_bType == PCT)
			{
				int count = 0;
				if (number(1,100) <= m_vecProbs[0])
				{
					idx_vec.push_back(0);
					count++;
				}
				for (uint i = 1; i < m_vecProbs.size(); i++)
				{
					if (number(1,100) <= m_vecProbs[i] - m_vecProbs[i-1])
					{
						idx_vec.push_back(i);
						count++;
					}
				}
				return count;
			}
			else
			{
				idx_vec.push_back(GetOneIndex());
				return 1;
			}
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			itertype(m_vecProbs) it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}

		int GetVnum(int idx) const
		{
			return m_vecItems[idx].vnum;
		}

		int GetCount(int idx) const
		{
			return m_vecItems[idx].count;
		}

		int GetRarePct(int idx) const
		{
			return m_vecItems[idx].rare;
		}

		bool Contains(DWORD dwVnum) const
		{
			for (DWORD i = 0; i < m_vecItems.size(); i++)
			{
				if (m_vecItems[i].vnum == dwVnum)
					return true;
			}
			return false;
		}
		
		// Group�� Type�� Special�� ��쿡
		// dwVnum�� ��Ī�Ǵ� AttrVnum�� return���ش�.
		DWORD GetAttrVnum(DWORD dwVnum) const
		{
			if (CSpecialItemGroup::SPECIAL != m_bType)
				return 0;
			for (itertype(m_vecItems) it = m_vecItems.begin(); it != m_vecItems.end(); it++)
			{
				if (it->vnum == dwVnum)
				{
					return it->count;
				}
			}
			return 0;
		}

		DWORD m_dwVnum;
		BYTE	m_bType;
		std::vector<int> m_vecProbs;
		std::vector<CSpecialItemInfo> m_vecItems; // vnum, count
};

class CMobItemGroup
{
	public:
		struct SMobItemGroupInfo
		{
			DWORD dwItemVnum;
			int iCount;
			int iRarePct;

			SMobItemGroupInfo(DWORD dwItemVnum, int iCount, int iRarePct)
				: dwItemVnum(dwItemVnum),
			iCount(iCount),
			iRarePct(iRarePct)
			{
			}
		};

		CMobItemGroup(DWORD dwMobVnum, int iKillDrop, const std::string& r_stName)
			:
			m_dwMobVnum(dwMobVnum),
		m_iKillDrop(iKillDrop),
		m_stName(r_stName)
		{
		}

		int GetKillPerDrop() const
		{
			return m_iKillDrop;
		}

		void AddItem(DWORD dwItemVnum, int iCount, int iPartPct, int iRarePct)
		{
			if (!m_vecProbs.empty())
				iPartPct += m_vecProbs.back();
			m_vecProbs.push_back(iPartPct);
			m_vecItems.push_back(SMobItemGroupInfo(dwItemVnum, iCount, iRarePct));
		}

		// MOB_DROP_ITEM_BUG_FIX
		bool IsEmpty() const
		{
			return m_vecProbs.empty();
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			itertype(m_vecProbs) it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}
		// END_OF_MOB_DROP_ITEM_BUG_FIX

		const SMobItemGroupInfo& GetOne() const
		{
			return m_vecItems[GetOneIndex()];
		}

	private:
		DWORD m_dwMobVnum;
		int m_iKillDrop;
		std::string m_stName;
		std::vector<int> m_vecProbs;
		std::vector<SMobItemGroupInfo> m_vecItems;
};

class CDropItemGroup
{
	struct SDropItemGroupInfo
	{
		DWORD	dwVnum;
		DWORD	dwPct;
		int	iCount;

		SDropItemGroupInfo(DWORD dwVnum, DWORD dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CDropItemGroup(DWORD dwVnum, DWORD dwMobVnum, const std::string& r_stName)
		:
		m_dwVnum(dwVnum),
	m_dwMobVnum(dwMobVnum),
	m_stName(r_stName)
	{
	}

	const std::vector<SDropItemGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(DWORD dwItemVnum, DWORD dwPct, int iCount)
	{
		m_vec_items.push_back(SDropItemGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	DWORD m_dwVnum;
	DWORD m_dwMobVnum;
	std::string m_stName;
	std::vector<SDropItemGroupInfo> m_vec_items;
};

class CLevelItemGroup
{
	struct SLevelItemGroupInfo
	{
		DWORD dwVNum;
		DWORD dwPct;
		int iCount;

		SLevelItemGroupInfo(DWORD dwVnum, DWORD dwPct, int iCount)
			: dwVNum(dwVnum), dwPct(dwPct), iCount(iCount)
		{ }
	};

	private :
		DWORD m_dwLevelLimit;
		std::string m_stName;
		std::vector<SLevelItemGroupInfo> m_vec_items;

	public :
		CLevelItemGroup(DWORD dwLevelLimit)
			: m_dwLevelLimit(dwLevelLimit)
		{}

		DWORD GetLevelLimit() { return m_dwLevelLimit; }

		void AddItem(DWORD dwItemVnum, DWORD dwPct, int iCount)
		{
			m_vec_items.push_back(SLevelItemGroupInfo(dwItemVnum, dwPct, iCount));
		}

		const std::vector<SLevelItemGroupInfo> & GetVector()
		{
			return m_vec_items;
		}
};

class CBuyerThiefGlovesItemGroup
{
	struct SThiefGroupInfo
	{
		DWORD	dwVnum;
		DWORD	dwPct;
		int	iCount;

		SThiefGroupInfo(DWORD dwVnum, DWORD dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CBuyerThiefGlovesItemGroup(DWORD dwVnum, DWORD dwMobVnum, const std::string& r_stName)
		:
		m_dwVnum(dwVnum),
	m_dwMobVnum(dwMobVnum),
	m_stName(r_stName)
	{
	}

	const std::vector<SThiefGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(DWORD dwItemVnum, DWORD dwPct, int iCount)
	{
		m_vec_items.push_back(SThiefGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	DWORD m_dwVnum;
	DWORD m_dwMobVnum;
	std::string m_stName;
	std::vector<SThiefGroupInfo> m_vec_items;
};

class ITEM;

class ITEM_MANAGER : public singleton<ITEM_MANAGER>
{
	public:
		ITEM_MANAGER();
		virtual ~ITEM_MANAGER();

		bool                    Initialize(TItemTable * table, int size);
		void			Destroy();
		void			Update();	// �� �������� �θ���.
		void			GracefulShutdown();

		DWORD			GetNewID();
		bool			SetMaxItemID(TItemIDRangeTable range); // �ִ� ���� ���̵� ����
		bool			SetMaxSpareItemID(TItemIDRangeTable range);

		// DelayedSave: ��� ��ƾ ������ ������ �ؾ� �� ���� ���� �ϸ� ����
		// ������ �ʹ� �������Ƿ� "������ �Ѵ�" ��� ǥ�ø� �صΰ� ���
		// (��: 1 frame) �Ŀ� �����Ų��.
		void			DelayedSave(LPITEM item);
		void			FlushDelayedSave(LPITEM item); // Delayed ����Ʈ�� �ִٸ� ����� �����Ѵ�. ���� ó���� ��� ��.
		void			SaveSingleItem(LPITEM item);

		LPITEM                  CreateItem(DWORD vnum, DWORD count = 1, DWORD dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
#ifndef DEBUG_ALLOC
		void DestroyItem(LPITEM item);
#else
		void DestroyItem(LPITEM item, const char* file, size_t line);
#endif
		void			RemoveItem(LPITEM item, const char * c_pszReason=NULL); // ����ڷ� ���� �������� ����

		LPITEM			Find(DWORD id);
		LPITEM                  FindByVID(DWORD vid);
		TItemTable *            GetTable(DWORD vnum);
		bool			GetVnum(const char * c_pszName, DWORD & r_dwVnum);
		bool			GetVnumByOriginalName(const char * c_pszName, DWORD & r_dwVnum);

		bool			LoadItemNames();
		const char *		GetLocaleName(DWORD dwVnum, const char * lang);

		bool			GetDropPct(LPCHARACTER pkChr, LPCHARACTER pkKiller, OUT int& iDeltaPercent, OUT int& iRandRange);
		bool			CreateDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item);

		bool			ReadCommonDropItemFile(const char * c_pszFileName);
		bool			ReadEtcDropItemFile(const char * c_pszFileName);
		bool			ReadDropItemGroup(const char * c_pszFileName);
		bool			ReadMonsterDropItemGroup(const char * c_pszFileName);
		bool			ReadSpecialDropItemFile(const char * c_pszFileName);
		
		// convert name -> vnum special_item_group.txt
		bool			ConvSpecialDropItemFile();
		// convert name -> vnum special_item_group.txt

		DWORD			GetRefineFromVnum(DWORD dwVnum);

		static void		CopyAllAttrTo(LPITEM pkOldItem, LPITEM pkNewItem);		// pkNewItem���� ��� �Ӽ��� ���� ������ ����ϴ� �Լ�.


		const CSpecialItemGroup* GetSpecialItemGroup(DWORD dwVnum);
		const CSpecialAttrGroup* GetSpecialAttrGroup(DWORD dwVnum);

		const std::vector<TItemTable> & GetTable() { return m_vec_prototype; }

		// CHECK_UNIQUE_GROUP
		int			GetSpecialGroupFromItem(DWORD dwVnum) const { itertype(m_ItemToSpecialGroup) it = m_ItemToSpecialGroup.find(dwVnum); return (it == m_ItemToSpecialGroup.end()) ? 0 : it->second; }
		// END_OF_CHECK_UNIQUE_GROUP

	protected:
		int                     RealNumber(DWORD vnum);
		void			CreateQuestDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item, int iDeltaPercent, int iRandRange);

	protected:
		typedef std::map<DWORD, LPITEM> ITEM_VID_MAP;		

		std::vector<TItemTable>		m_vec_prototype;
		std::vector<TItemTable*> m_vec_item_vnum_range_info;
		std::map<DWORD, DWORD>		m_map_ItemRefineFrom;
		int				m_iTopOfTable;

		ITEM_VID_MAP			m_VIDMap;			///< m_dwVIDCount �� �������� �������� �����Ѵ�.
		DWORD				m_dwVIDCount;			///< �̳༮ VID�� �ƴ϶� �׳� ���μ��� ���� ����ũ ��ȣ��.
		DWORD				m_dwCurrentID;
		TItemIDRangeTable	m_ItemIDRange;
		TItemIDRangeTable	m_ItemIDSpareRange;

		std::unordered_set<LPITEM> m_set_pkItemForDelayedSave;
		std::map<DWORD, LPITEM>		m_map_pkItemByID;
		std::map<DWORD, DWORD>		m_map_dwEtcItemDropProb;
		std::map<DWORD, CDropItemGroup*> m_map_pkDropItemGroup;
		std::map<DWORD, CSpecialItemGroup*> m_map_pkSpecialItemGroup;
		std::map<DWORD, CSpecialItemGroup*> m_map_pkQuestItemGroup;
		std::map<DWORD, CSpecialAttrGroup*> m_map_pkSpecialAttrGroup;
		std::map<DWORD, CMobItemGroup*> m_map_pkMobItemGroup;
		std::map<DWORD, CLevelItemGroup*> m_map_pkLevelItemGroup;
		std::map<DWORD, CBuyerThiefGlovesItemGroup*> m_map_pkGloveItemGroup;
		std::map<std::string, std::map<DWORD, std::string>> m_mapItemNamesByLang;

		// CHECK_UNIQUE_GROUP
		std::map<DWORD, int>		m_ItemToSpecialGroup;
		// END_OF_CHECK_UNIQUE_GROUP
	
	private:
		// ���Ͽ��� ���� ĳ�� �����۰� ������, ��ȯ ������ ĳ�� �������� ����ٰ� �Ͽ�,
		// �������� �����ۿ� ��ȯ ���� �÷��׸� ������ ���ο� �����۵��� �����,
		// ���ο� ������ �뿪�� �Ҵ��Ͽ���.
		// ������ ���ο� �����۵� �������� �����۰� ���� ȿ���� �����ϴµ�,
		// ������, Ŭ���, vnum ������� �Ǿ��־�
		// ���ο� vnum�� �˴� ������ ���� �� �ھƾ��ϴ� ��Ÿ��� ��Ȳ�� �´�Ҵ�.
		// �׷��� �� vnum�� �������̸�, �������� ���ư� ���� �������� ������ vnum���� �ٲ㼭 ���� �ϰ�,
		// ������ ���� ���� vnum���� �ٲ��ֵ��� �Ѵ�.
		// �̸� ���� �������� vnum�� ���ο� vnum�� ��������ִ� ���� ����.
		typedef std::map <DWORD, DWORD> TMapDW2DW;
		TMapDW2DW	m_map_new_to_ori;

	public:	
		DWORD	GetMaskVnum(DWORD dwVnum);
		std::map<DWORD, TItemTable>  m_map_vid;
		std::map<DWORD, TItemTable>&  GetVIDMap() { return m_map_vid; }
		std::vector<TItemTable>& GetVecProto() { return m_vec_prototype; }	
		
		const static int MAX_NORM_ATTR_NUM = 5;
		const static int MAX_RARE_ATTR_NUM = 2;
		bool ReadItemVnumMaskTable(const char * c_pszFileName);
	private:
#ifdef M2_USE_POOL
		ObjectPool<CItem> pool_;
#endif
};

#ifndef DEBUG_ALLOC
#define M2_DESTROY_ITEM(ptr) ITEM_MANAGER::instance().DestroyItem(ptr)
#else
#define M2_DESTROY_ITEM(ptr) ITEM_MANAGER::instance().DestroyItem(ptr, __FILE__, __LINE__)
#endif

#endif
