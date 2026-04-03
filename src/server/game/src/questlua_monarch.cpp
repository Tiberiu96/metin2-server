#include "stdafx.h"

#include "questlua.h"
#include "questmanager.h"
#include "monarch.h"
#include "desc_client.h"
#include "start_position.h"
#include "config.h"
#include "mob_manager.h"
#include "castle.h"
#include "dev_log.h"
#include "char.h"
#include "char_manager.h"
#include "utils.h"
#include "p2p.h"
#include "guild.h"
#include "sectree_manager.h"

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

ACMD(do_monarch_mob);

namespace quest
{
	EVENTINFO(monarch_powerup_event_info)
	{
		int EmpireIndex;

		monarch_powerup_event_info() 
		: EmpireIndex( 0 )
		{
		}
	};

	// NOTE: Copied from SPacketGGMonarchTransfer for event data 
	EVENTINFO(monarch_transfer2_event_info)
	{
		BYTE	bHeader;
		DWORD	dwTargetPID;
		long	x;
		long	y;

		monarch_transfer2_event_info() 
		: bHeader( 0 )
		, dwTargetPID( 0 )
		, x( 0 )
		, y( 0 )
		{
		}
	};

	EVENTFUNC(monarch_powerup_event)
	{
		monarch_powerup_event_info * info =  dynamic_cast<monarch_powerup_event_info*>(event->info);

		if ( info == NULL )
		{
			sys_err( "monarch_powerup_event> <Factor> Null pointer" );
			return 0;
		}

		CMonarch::instance().PowerUp(info->EmpireIndex, false);
		return 0;
	}

	EVENTINFO(monarch_defenseup_event_info)
	{
		int EmpireIndex;

		monarch_defenseup_event_info() 
		: EmpireIndex( 0 )
		{
		}
	};

	EVENTFUNC(monarch_defenseup_event)
	{
		monarch_powerup_event_info * info =  dynamic_cast<monarch_powerup_event_info*>(event->info);

		if ( info == NULL )
		{
			sys_err( "monarch_defenseup_event> <Factor> Null pointer" );
			return 0;
		}

		CMonarch::instance().DefenseUp(info->EmpireIndex, false);
		return 0;
	}
	
	//
	// "monarch_" lua functions
	//
	int takemonarchmoney(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		
		int nMoney = (int)lua_tonumber(L,1);
		int nPID = ch->GetPlayerID();
		int nEmpire = ch->GetEmpire();
		nMoney = nMoney * 10000;
		
		sys_log(0 ,"[MONARCH] Take Money Empire(%d) pid(%d) Money(%d)", ch->GetEmpire(), ch->GetPlayerID(), nMoney);


		db_clientdesc->DBPacketHeader(HEADER_GD_TAKE_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) * 3);
		db_clientdesc->Packet(&nEmpire, sizeof(int)); 
		db_clientdesc->Packet(&nPID, sizeof(int));
		db_clientdesc->Packet(&nMoney, sizeof(int));
		return 1;
	}

	int	 is_guild_master(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
	
		if (ch->GetGuild()	)
		{
			TGuildMember * pMember = ch->GetGuild()->GetMember(ch->GetPlayerID());

			if (pMember)
			{
				if (pMember->grade <= 4)
				{
					lua_pushnumber(L ,1);
					return 1;
				}
			}

		}
		lua_pushnumber(L ,0);

		return 1;
	}

	int monarch_bless(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		
		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}
	
		int HealPrice = quest::CQuestManager::instance().GetEventFlag("MonarchHealGold");
		if (HealPrice == 0)
			HealPrice = 2000000;	// 200만

		if (CMonarch::instance().HealMyEmpire(ch, HealPrice))
		{
			char szNotice[256];
			snprintf(szNotice, sizeof(szNotice),
					LC_TEXT("When the Blessing of the Emperor is used %s the HP and SP are restored again."), EMPIRE_NAME(ch->GetEmpire()));
			SendNoticeMap(szNotice, ch->GetMapIndex(), false);

			ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("The Emperor Blessing is activated.", ch->GetLanguage()));
		}

		return 1;
	}

	// 군주의 사자후 퀘스트 함수 
	int monarch_powerup(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		//군주 체크 
		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}

		//군주 국고 검사
		int	money_need = 5000000;	// 500만
		if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, money_need);
			return 0;
		}

		if (!CMonarch::instance().CheckPowerUpCT(ch->GetEmpire()))
		{
			int	next_sec = CMonarch::instance().GetPowerUpCT(ch->GetEmpire()) / passes_per_sec;
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("After %d seconds you can use the Emperors Blessing.", ch->GetLanguage()), next_sec);
			return 0;
		}

		//군주의 사자후 적용	
		CMonarch::instance().PowerUp(ch->GetEmpire(), true); 
	
		//군주의 사자후 적용시간 	
		int g_nMonarchPowerUpCT = 60 * 3;
		
		monarch_powerup_event_info* info = AllocEventInfo<monarch_powerup_event_info>();

		info->EmpireIndex = ch->GetEmpire();
		
		event_create(quest::monarch_powerup_event, info, PASSES_PER_SEC(g_nMonarchPowerUpCT));
		
		CMonarch::instance().SendtoDBDecMoney(5000000, ch->GetEmpire(), ch);
		
		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), LC_TEXT("Due to Emperor Sa-Za-Hu, the player %s will receive an attack power increase of 10 %% for 3 minutes in this area."), EMPIRE_NAME(ch->GetEmpire()));
		
		SendNoticeMap(szNotice, ch->GetMapIndex(), false);

		return 1;		
	}
	// 군주의 금강권 퀘스트 함수 
	int monarch_defenseup(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		
		if (!ch)
			return 0;
	
		
		//군주 체크	
		
		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}
	
		int	money_need = 5000000;	// 500만
		if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, money_need);
			return 0;
		}
	
		if (!CMonarch::instance().CheckDefenseUpCT(ch->GetEmpire()))
		{
			int	next_sec = CMonarch::instance().GetDefenseUpCT(ch->GetEmpire()) / passes_per_sec;
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("After %d seconds you can use the Emperors Blessing.", ch->GetLanguage()), next_sec);
			return 0;
		}	
		
		//군주의 금강권 적용	
		CMonarch::instance().DefenseUp(ch->GetEmpire(), true); 

		//군주의 금강권 적용 시간
		int g_nMonarchDefenseUpCT = 60 * 3;
		
		monarch_defenseup_event_info* info = AllocEventInfo<monarch_defenseup_event_info>();
		
		info->EmpireIndex = ch->GetEmpire();
		
		event_create(quest::monarch_defenseup_event, info, PASSES_PER_SEC(g_nMonarchDefenseUpCT));
		
		CMonarch::instance().SendtoDBDecMoney(5000000, ch->GetEmpire(), ch);
		
		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), LC_TEXT("By the Emperor Geum-Gang-Gwon the player %s gets 10 %% more armour for 3 minutes."), EMPIRE_NAME(ch->GetEmpire()));

		SendNoticeMap(szNotice, ch->GetMapIndex(), false);

		return 1;
	}

	int is_monarch(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;
		lua_pushnumber(L, ch->IsMonarch());
		return 1;
	}

	int spawn_mob(lua_State * L)
	{
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		DWORD mob_vnum = (DWORD)lua_tonumber(L, 1);
	
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		const CMob * pkMob = NULL;
		
		if (!(pkMob = CMobManager::Instance().Get(mob_vnum)))
			if (pkMob == NULL)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "No such mob by that vnum");
				return 0;
			}

		if (false == ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}

		DWORD dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		LPCHARACTER mob = NULL;

		{
			long x = ch->GetX();
			long y = ch->GetY();
#if 0
			if (11505 == mob_vnum)	// 황금두꺼비
			{
				//군주 국고 검사
				if (!CMonarch::instance().IsMoneyOk(CASTLE_FROG_PRICE, ch->GetEmpire()))
				{
					int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, CASTLE_FROG_PRICE);
					return 0;
				}

				mob = castle_spawn_frog(ch->GetEmpire());

				if (mob)
				{
					// 국고감소
					CMonarch::instance().SendtoDBDecMoney(CASTLE_FROG_PRICE, ch->GetEmpire(), ch);
					castle_save();
				}
			}
			else
#endif
			{
				mob = CHARACTER_MANAGER::instance().SpawnMob(mob_vnum, ch->GetMapIndex(), x, y, 0, pkMob->m_table.bType == CHAR_TYPE_STONE, -1);
			}

			if (mob)
			{
				//if (bAggressive)
				//	mob->SetAggressive();

				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, (DWORD) mob->GetVID());
				}
			}
		}

		return 1;
	}

	int spawn_guard(lua_State * L)
	{
		// mob_level(0-2), region(0~3)
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("invalid argument");
			return 0;
		}

		DWORD	group_vnum		= (DWORD)lua_tonumber(L,1);
		int		region_index	= (int)lua_tonumber(L, 2);
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;

		/*-----
		const CMob * pkMob = NULL;
		if (!(pkMob = CMobManager::Instance().Get(mob_vnum)))

		if (pkMob == NULL)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "No such mob by that vnum");
			return 0;
		}
		-----*/
		
		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}

		if (false==castle_is_my_castle(ch->GetEmpire(), ch->GetMapIndex()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You are only able to use this function while you are at the castle.", ch->GetLanguage()));
			return 0;
		}

		//DWORD 			dwQuestIdx 	= CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		LPCHARACTER		guard_leader = NULL;
		{
			int	money_need = castle_cost_of_hiring_guard(group_vnum);

			//군주 국고 검사
			if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
			{
				int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, money_need);
				return 0;
			}
			guard_leader = castle_spawn_guard(ch->GetEmpire(), group_vnum, region_index);

			if (guard_leader)
			{
				// 국고감소
				CMonarch::instance().SendtoDBDecMoney(money_need, ch->GetEmpire(), ch);
				castle_save();
			}
		}

		return 1;
	}

	int frog_to_empire_money(lua_State * L)
	{
		LPCHARACTER ch	= CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (NULL==ch)
			return false;

		if (!ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT_LANG("You do not have the emperor qualification.", ch->GetLanguage()));
				sys_err("No Monarch pid %d ", ch->GetPlayerID());
				return 0;
			}
		}

		if (castle_frog_to_empire_money(ch))
		{
			int empire_money = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("TEST: The Gold Bar has been paid back to your kingdom's safe.", ch->GetLanguage()));
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("TEST: In your kingdom's safe there are: %d", ch->GetLanguage()), empire_money);
			castle_save();
			return 1;
		}
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("TEST: You cannot pay the Gold Bar back into your kingdom's safe.", ch->GetLanguage()));
			return 0;
		}
	}

	int monarch_warp(lua_State * L)
	{
		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		std::string name = lua_tostring(L, 1);
		
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;


		
		if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("This function can only be used by the emperor.", ch->GetLanguage()));
			return 0;
		}

		//군주 쿨타임 검사
		if (!ch->IsMCOK(CHARACTER::MI_WARP))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Cooldown time for approximately %d seconds", ch->GetLanguage()), ch->GetMCLTime(CHARACTER::MI_WARP));	
			return 0;
		}


		//군주 몹 소환 비용 
		const int WarpPrice = 10000;

		//군주 국고 검사 
		if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, WarpPrice);
			return 0;	
		}

		int x, y;

		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name.c_str());

		if (!tch)
		{
			CCI * pkCCI = P2P_MANAGER::instance().Find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot be warped to an unknown player.", ch->GetLanguage()));
					return 0;
				}
				if (pkCCI->bChannel != g_bChannel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Adding player %d into the channel. (Present channel %d)", ch->GetLanguage()), pkCCI->bChannel, g_bChannel);
					return 0;
				}
	
				if (!IsMonarchWarpZone(pkCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));	
					return 0;
				}

				PIXEL_POSITION pos;

				if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos))
					ch->ChatPacket(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
				else
				{
					//ch->ChatPacket(CHAT_TYPE_INFO, "You warp to (%d, %d)", pos.x, pos.y);
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Warp to player %s.", ch->GetLanguage()), name.c_str());
					ch->WarpSet(pos.x, pos.y);

					//군주 돈 삭감	
					CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

					//쿨타임 초기화 
					ch->SetMC(CHARACTER::MI_WARP);
				}

			}
			else if (NULL == CHARACTER_MANAGER::instance().FindPC(name.c_str()))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "There is no one by that name");
			}

			return 0;
		}
		else
		{
			if (tch->GetEmpire() != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot be warped to an unknown player.", ch->GetLanguage()));
				return 0;
			}

			if (!IsMonarchWarpZone(tch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));
				return 0;
			}

			x = tch->GetX();
			y = tch->GetY();
		}

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Warp to player %s.", ch->GetLanguage()), name.c_str());
		ch->WarpSet(x,y);
		ch->Stop();

		//군주 돈 삭감	
		CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

		//쿨타임 초기화 
		ch->SetMC(CHARACTER::MI_WARP); 

		return 0;
	}

	int empire_info(lua_State * L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (NULL == ch)
			return false;

		TMonarchInfo * p = CMonarch::instance().GetMonarch();

		if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))	
		{
			ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT_LANG("My information about the emperor", ch->GetLanguage()));
		
			for (int n = 1; n < 4; ++n)
			{
				if (n == ch->GetEmpire())
					ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT_LANG("[%sMonarch] : %s Yang owned %lld", ch->GetLanguage()), EMPIRE_NAME(n), p->name[n], p->money[n]);
				else
					ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT_LANG("[%sMonarch] : %s", ch->GetLanguage()), EMPIRE_NAME(n), p->name[n]);
			}
		}
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT_LANG("Information about the emperor", ch->GetLanguage()));

			for (int n = 1; n < 4; ++n)
				ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT_LANG("[%sMonarch] : %s", ch->GetLanguage()), EMPIRE_NAME(n), p->name[n]);
		}

		return 0;
	}

	int monarch_transfer(lua_State * L)
	{
		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		std::string name = lua_tostring(L, 1);
		
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("This function can only be used by the emperor.", ch->GetLanguage()));
			return 0;
		}

		// 군주 쿨타임 검사
		if (!ch->IsMCOK(CHARACTER::MI_TRANSFER))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Cooldown time for approximately %d seconds", ch->GetLanguage()), ch->GetMCLTime(CHARACTER::MI_TRANSFER));	
			return 0;
		}

		// 군주 워프 비용 
		const int WarpPrice = 10000;

		// 군주 국고 검사 
		if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, WarpPrice);
			return 0;	
		}

		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name.c_str());

		if (!tch)
		{
			CCI * pkCCI = P2P_MANAGER::instance().Find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit players from another kingdom.", ch->GetLanguage()));
					return 0;
				}

				if (pkCCI->bChannel != g_bChannel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("The player %s is on channel %d at the moment. (Your channel: %d)", ch->GetLanguage()), name.c_str(), pkCCI->bChannel, g_bChannel);
					return 0;
				}

				if (!IsMonarchWarpZone(pkCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));	
					return 0;
				}
				if (!IsMonarchWarpZone(ch->GetMapIndex()))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot summon to that area.", ch->GetLanguage()));
					return 0;
				}

				TPacketGGTransfer pgg;

				pgg.bHeader = HEADER_GG_TRANSFER;
				strlcpy(pgg.szName, name.c_str(), sizeof(pgg.szName));
				pgg.lX = ch->GetX();
				pgg.lY = ch->GetY();

				P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You have recruited %s players.", ch->GetLanguage()), name.c_str());

				// 군주 돈 삭감
				CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

				// 쿨타임 초기화 
				ch->SetMC(CHARACTER::MI_TRANSFER);
			}
			else
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("There is no user with this name.", ch->GetLanguage()));
			}

			return 0;
		}

		if (ch == tch)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit yourself.", ch->GetLanguage()));
			return 0;
		}

		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit players from another kingdom.", ch->GetLanguage()));
			return 0;
		}

		if (!IsMonarchWarpZone(tch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));
			return 0;
		}
		if (!IsMonarchWarpZone(ch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot summon to that area.", ch->GetLanguage()));
			return 0;
		}
		tch->WarpSet(ch->GetX(), ch->GetY(), ch->GetMapIndex());

		// 군주 돈 삭감	
		CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
		// 쿨타임 초기화 
		ch->SetMC(CHARACTER::MI_TRANSFER);
		return 0;
	}

	int monarch_notice(lua_State * L)
	{
		if (!lua_isstring(L, 1))
			return 0;

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch == NULL)
			return 0;
		
		if (ch->IsMonarch() == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("This function can only be used by the emperor.", ch->GetLanguage()));
			return 0;
		}

		std::string strNotice = lua_tostring(L, 1);

		if (strNotice.length() > 0)
			SendMonarchNotice(ch->GetEmpire(), strNotice.c_str());

		return 0;
	}

	int monarch_mob(lua_State * L)
	{
		if (!lua_isstring(L, 1))
			return 0;

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch == NULL)
			return 0;

		char vnum[256];
		strlcpy(vnum, lua_tostring(L, 1), sizeof(vnum));
		do_monarch_mob(ch, vnum, 0, 0);
		return 0;
	}

	EVENTFUNC(monarch_transfer2_event)
	{
		monarch_transfer2_event_info* info = dynamic_cast<monarch_transfer2_event_info*>(event->info);

		if ( info == NULL )
		{
			sys_err( "monarch_transfer2_event> <Factor> Null pointer" );
			return 0;
		}

		LPCHARACTER pTargetChar = CHARACTER_MANAGER::instance().FindByPID(info->dwTargetPID);

		if (pTargetChar != NULL)
		{
			unsigned int qIndex = quest::CQuestManager::instance().GetQuestIndexByName("monarch_transfer");

			if (qIndex != 0)
			{
				pTargetChar->SetQuestFlag("monarch_transfer.x", info->x);
				pTargetChar->SetQuestFlag("monarch_transfer.y", info->y);
				quest::CQuestManager::instance().Letter(pTargetChar->GetPlayerID(), qIndex, 0);
			}
		}

		return 0;
	}

	int monarch_transfer2(lua_State* L)
	{
		if (lua_isstring(L, 1) == false) return 0;
		
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (ch == NULL) return false;

		if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()) == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("This function can only be used by the emperor.", ch->GetLanguage()));
			return 0;
		}

		if (ch->IsMCOK(CHARACTER::MI_TRANSFER) == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Cooldown time for approximately %d seconds", ch->GetLanguage()), ch->GetMCLTime(CHARACTER::MI_TRANSFER));
			return 0;
		}
		
		const int ciTransferCost = 10000;

		if (CMonarch::instance().IsMoneyOk(ciTransferCost, ch->GetEmpire()) == false)
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Not enough money in the treasury. Current: %u Required: %u", ch->GetLanguage()), NationMoney, ciTransferCost);
			return 0;
		}

		std::string strTargetName = lua_tostring(L, 1);

		LPCHARACTER pTargetChar = CHARACTER_MANAGER::instance().FindPC(strTargetName.c_str());

		if (pTargetChar == NULL)
		{
			CCI* pCCI = P2P_MANAGER::instance().Find(strTargetName.c_str());

			if (pCCI != NULL)
			{
				if (pCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit players from another kingdom.", ch->GetLanguage()));
					return 0;
				}

				if (pCCI->bChannel != g_bChannel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("%s is connected on channel %d. (Current channel: %d)", ch->GetLanguage()),
						   strTargetName.c_str(), pCCI->bChannel, g_bChannel);
					return 0;
				}

				if (!IsMonarchWarpZone(pCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));
					return 0;
				}
				if (!IsMonarchWarpZone(ch->GetMapIndex()))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot summon to that area.", ch->GetLanguage()));
					return 0;
				}

				TPacketMonarchGGTransfer packet;
				packet.bHeader = HEADER_GG_MONARCH_TRANSFER;
				packet.dwTargetPID = pCCI->dwPID;
				packet.x = ch->GetX();
				packet.y = ch->GetY();

				P2P_MANAGER::instance().Send(&packet, sizeof(TPacketMonarchGGTransfer));
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Summon request sent.", ch->GetLanguage()));

				CMonarch::instance().SendtoDBDecMoney(ciTransferCost, ch->GetEmpire(), ch);
				ch->SetMC(CHARACTER::MI_TRANSFER);
			}
			else
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("There is no user with this name.", ch->GetLanguage()));
				return 0;
			}
		}
		else
		{
			if (pTargetChar == ch)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit yourself.", ch->GetLanguage()));
				return 0;
			}

			if (pTargetChar->GetEmpire() != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("You cannot recruit players from another kingdom.", ch->GetLanguage()));
				return 0;
			}

			if (DISTANCE_APPROX(pTargetChar->GetX() - ch->GetX(), pTargetChar->GetY() - ch->GetY()) <= 5000)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("%s is nearby.", ch->GetLanguage()), pTargetChar->GetName());
				return 0;
			}

			if (!IsMonarchWarpZone(pTargetChar->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot move to that area.", ch->GetLanguage()));
				return 0;
			}
			if (!IsMonarchWarpZone(ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT_LANG("Cannot summon to that area.", ch->GetLanguage()));
				return 0;
			}

			monarch_transfer2_event_info* info = AllocEventInfo<monarch_transfer2_event_info>();

			info->bHeader = HEADER_GG_MONARCH_TRANSFER;
			info->dwTargetPID = pTargetChar->GetPlayerID();
			info->x = ch->GetX();
			info->y = ch->GetY();

			event_create(monarch_transfer2_event, info, 1);

			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_LANG("Summon request sent.", ch->GetLanguage()));

			CMonarch::instance().SendtoDBDecMoney(ciTransferCost, ch->GetEmpire(), ch);
			ch->SetMC(CHARACTER::MI_TRANSFER);
			return 0;
		}

		return 0;
	}

	void RegisterMonarchFunctionTable()
	{
		luaL_reg Monarch_functions[] = 
		{
			{ "takemonarchmoney",		takemonarchmoney	},
			{ "isguildmaster",			is_guild_master		},
			{ "ismonarch",				is_monarch 			},
			{ "monarchbless",			monarch_bless		},
			{ "monarchpowerup",			monarch_powerup		},
			{ "monarchdefenseup",		monarch_defenseup	},
			{ "spawnmob",				spawn_mob			},
			{ "spawnguard",				spawn_guard			},
//			{ "frog_to_empire_money",	frog_to_empire_money		},
			{ "warp",					monarch_warp 		},
			{ "transfer",				monarch_transfer	},
			{ "transfer2",				monarch_transfer2	},
			{ "info",					empire_info 		},	
			{ "notice",					monarch_notice		},
			{ "monarch_mob",			monarch_mob			},

			{ NULL,						NULL				}
		};
		
		CQuestManager::instance().AddLuaFunctionTable("oh", Monarch_functions);
	}

}

