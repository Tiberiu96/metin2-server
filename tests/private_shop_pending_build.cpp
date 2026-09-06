#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <vector>
using BYTE = uint8_t;
using DWORD = uint32_t;
enum { HEADER_GD_ITEM_SAVE = 1, HEADER_GD_PRIVATE_SHOP, PRIVATE_SHOP_GD_SUBHEADER_LOGOUT, QUICKSLOT_TYPE_ITEM, CHAT_TYPE_INFO };
#define sys_log(...) ((void)0)
#define sys_err(...) ((void)0)
#define LC_TEXT(s) (s)
#define LC_TEXT_LANG(s, lang) (s)
struct TPlayerItem { DWORD id = 0, vnum = 50200; };
struct TPlayerPrivateShopItem { DWORD dwID = 0; };
struct TPrivateShop { DWORD dwVnum = 30000; BYTE bTitleType = 0, bPageCount = 1; };
static DWORD GetPrivateShopBundleVnum(DWORD, BYTE, BYTE) { return 50200; }
struct Desc { DWORD handle = 10; DWORD GetHandle() { return handle; } };
struct Character {
    DWORD pid = 1; Desc desc; int refunded = 0, loadedItems = 0;
    DWORD GetPlayerID() { return pid; }
    Desc* GetDesc() { return &desc; }
    void ClosePrivateShopPanel(bool) {}
    void SyncQuickslot(int, int, int) {}
    void AutoGiveItem(DWORD) { ++refunded; }
    void ChatPacket(int, const char*) {}
    void SetPrivateShopTable(const TPrivateShop&) {}
    void SetPrivateShopItem(const TPlayerPrivateShopItem&) { ++loadedItems; }
};
using LPCHARACTER = Character*;
struct Item {
    DWORD id; LPCHARACTER owner; bool skip = true, locked = true;
    LPCHARACTER GetOwner() { return owner; }
    void SetSkipSave(bool value) { skip = value; }
    void RemoveFromCharacter() { owner = nullptr; }
    void Lock(bool value) { locked = value; }
    int GetCell() { return 0; }
};
using LPITEM = Item*;
struct ITEM_MANAGER {
    std::unordered_map<DWORD, std::unique_ptr<Item>> items;
    static ITEM_MANAGER& Instance() { static ITEM_MANAGER instance; return instance; }
    LPITEM Find(DWORD id) { auto it = items.find(id); return it == items.end() ? nullptr : it->second.get(); }
    void Destroy(LPITEM item) { assert(item->skip && !item->owner); items.erase(item->id); }
};
#define M2_DESTROY_ITEM(item) ITEM_MANAGER::Instance().Destroy(item)
struct CHARACTER_MANAGER {
    LPCHARACTER owner = nullptr;
    static CHARACTER_MANAGER& Instance() { static CHARACTER_MANAGER instance; return instance; }
    LPCHARACTER FindByPID(DWORD) { return owner; }
};
struct LogManager {
    static LogManager& Instance() { static LogManager instance; return instance; }
    void CharLog(LPCHARACTER, int, const char*, const char*) {}
};
struct DB {
    int refunds = 0, logout = 0; DWORD logoutHandle = 0;
    void DBPacket(int header, DWORD, const void*, size_t) { assert(header == HEADER_GD_ITEM_SAVE); ++refunds; }
    void DBPacketHeader(int, DWORD handle, size_t) { ++logout; logoutHandle = handle; }
    void Packet(const void*, size_t) {}
};
static DB database;
static DB* db_clientdesc = &database;
struct CPrivateShopManager {
    struct PendingBuild { std::vector<TPlayerPrivateShopItem> items; TPlayerItem bundle{}; DWORD ownerHandle = 0; bool disconnected = false; };
    std::unordered_map<DWORD, PendingBuild> m_pendingBuilds;
    int spawned = 0;
    void SetPendingBuildBundle(DWORD, const TPlayerItem&);
    void PreservePendingBuild(LPCHARACTER);
    void BuildPrivateShopResult(DWORD, TPrivateShop*, bool);
    void DeletePrivateShop(DWORD) {}
    void SpawnPrivateShop(TPrivateShop*, const std::vector<TPlayerPrivateShopItem>& items) { for (const auto& item : items) assert(!ITEM_MANAGER::Instance().Find(item.dwID)); ++spawned; }
    void Prepare(Character& owner) {
        PendingBuild pending; pending.ownerHandle = owner.desc.handle; pending.items.push_back({25});
        m_pendingBuilds.emplace(owner.pid, pending);
        ITEM_MANAGER::Instance().items.emplace(25, std::make_unique<Item>(Item{25, &owner}));
        CHARACTER_MANAGER::Instance().owner = &owner;
        database = {};
    }
};
#include "private_shop_game_functions.inc"
int main() {
    TPrivateShop table;
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.PreservePendingBuild(&owner);
        assert(!ITEM_MANAGER::Instance().Find(25));
        CHARACTER_MANAGER::Instance().owner = nullptr;
        m.BuildPrivateShopResult(1, &table, true);
        assert(m.spawned == 1 && database.logout == 1 && database.logoutHandle == 10 && m.m_pendingBuilds.empty());
        m.BuildPrivateShopResult(1, &table, true);
        assert(m.spawned == 1);
    }
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.SetPendingBuildBundle(1, TPlayerItem{99}); m.PreservePendingBuild(&owner);
        CHARACTER_MANAGER::Instance().owner = nullptr;
        m.BuildPrivateShopResult(1, &table, false);
        assert(!m.spawned && database.refunds == 1 && m.m_pendingBuilds.empty());
    }
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.BuildPrivateShopResult(1, &table, true);
        assert(m.spawned == 1 && owner.loadedItems == 1 && !database.logout && !ITEM_MANAGER::Instance().Find(25));
    }
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.SetPendingBuildBundle(1, TPlayerItem{99});
        m.BuildPrivateShopResult(1, &table, false);
        auto original = ITEM_MANAGER::Instance().Find(25);
        assert(original && !original->locked && !original->skip && owner.refunded == 1 && !m.spawned);
        ITEM_MANAGER::Instance().items.clear();
    }
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.BuildPrivateShopResult(1, &table, false);
        assert(owner.refunded == 0 && database.refunds == 0 && !m.spawned);
        ITEM_MANAGER::Instance().items.clear();
    }
    {
        CPrivateShopManager m; Character owner; m.Prepare(owner);
        m.PreservePendingBuild(&owner);
        CHARACTER_MANAGER::Instance().owner = nullptr;
        m.BuildPrivateShopResult(1, &table, false);
        assert(database.refunds == 0 && !m.spawned && m.m_pendingBuilds.empty());
    }
    std::puts("PASS: pending creation disconnect, offline success/failure, bundle refund, duplicate response, online success/failure");
}
