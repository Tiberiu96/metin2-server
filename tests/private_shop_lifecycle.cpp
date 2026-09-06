// Runs production DB lifecycle functions with deterministic peers, cache and clock.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
static time_t now = 1000;
static time_t test_time(int) { return now; }
#define time test_time
#define sys_log(...) ((void)0)
#define sys_err(...) ((void)0)
static void strlcpy(char* dst, const char* src, size_t count) { std::snprintf(dst, count, "%s", src); }
enum { STATE_UNAVAILABLE, STATE_CLOSED, STATE_OPEN, STATE_MODIFY };
enum { HEADER_DG_PRIVATE_SHOP, PRIVATE_SHOP_DG_SUBHEADER_STATE_UPDATE,
    PRIVATE_SHOP_DG_SUBHEADER_CANCEL_BUY, PRIVATE_SHOP_DG_SUBHEADER_SALE_UPDATE,
    PRIVATE_SHOP_DG_SUBHEADER_REMOVE_ITEM };
struct Price { long long llGold = 0; DWORD dwCheque = 0; };
struct TPlayerPrivateShopItem { DWORD dwID = 1, dwOwner = 1, dwVnum = 1; WORD wPos = 0; Price TPrice; };
struct Item { TPlayerPrivateShopItem TItem; bool bAvailable = true; DWORD dwReservation = 0, dwBuyerPeer = 0; time_t tReservationExpiry = 0; };
using TPrivateShopItemInfo = Item;
struct TPacketGDPrivateShopFailedBuy { DWORD dwReservation, dwShopID; WORD wPos; };
struct TPacketGDPrivateShopBuy { DWORD dwReservation; TPlayerPrivateShopItem TItem; DWORD dwCustomer; char szCustomerName[32]; };
struct TPacketGDPrivateShopWithdraw { DWORD dwPID; long long llGold; DWORD dwCheque; };
struct TPacketDGPrivateShopStateUpdate { DWORD dwPID; BYTE bState; };
struct TPacketDGPrivateShopSaleUpdate { char szCustomerName[32]; TPlayerPrivateShopItem TItem; };
struct CPeer {
    DWORD id = 2; int packets = 0;
    DWORD GetHandle() { return id; }
    void EncodeHeader(int, DWORD, size_t) { ++packets; }
    void Encode(const void*, size_t) {}
};
struct Cache { int flushes = 0; void Flush() { ++flushes; } };
struct CPrivateShop {
    DWORD pid = 1, ownerPeer = 0, shopPeer = 2, ownerHandle = 0;
    BYTE state = STATE_OPEN;
    long long gold = 0; DWORD cheque = 0;
    time_t premium = 0, persistedPremium = 0;
    std::vector<Item> items;
    DWORD GetOwner() { return pid; }
    DWORD GetOwnerPeerHandle() { return ownerPeer; }
    DWORD GetOwnerHandle() { return ownerHandle; }
    DWORD GetShopPeerHandle() { return shopPeer; }
    void BindOwnerPeerHandle(DWORD peer) { ownerPeer = peer; }
    void SetOwnerHandle(DWORD handle) { ownerHandle = handle; }
    void ChangeState(BYTE value) { state = value; }
    BYTE GetState() { return state; }
    time_t GetPremiumTime() { return premium; }
    void UpdatePremiumTime(time_t value) { persistedPremium = premium = value; }
    long long GetGold() { return gold; }
    DWORD GetCheque() { return cheque; }
    WORD GetItemCount() { return static_cast<WORD>(items.size()); }
    std::vector<Item>& GetItemContainer() { return items; }
    Item* GetItem(WORD pos) { for (auto& item : items) if (item.TItem.wPos == pos) return &item; return nullptr; }
    void UpdateBalance(long long value, DWORD won) { gold += value; cheque += won; }
    void Withdraw(long long value, DWORD won) { gold -= value; cheque -= won; }
    void RemoveItem(WORD pos) { items.erase(std::remove_if(items.begin(), items.end(), [pos](const Item& i) { return i.TItem.wPos == pos; }), items.end()); }
};
using LPPRIVATE_SHOP = CPrivateShop*;
struct CClientManager {
    std::unordered_map<DWORD, std::unique_ptr<CPrivateShop>> m_map_privateShop;
    std::list<LPPRIVATE_SHOP> m_list_privateShopPremium;
    CPeer peer; Cache cache; int destroyed = 0, itemFlushes = 0;
    LPPRIVATE_SHOP Add(DWORD pid = 1) { auto p = std::make_unique<CPrivateShop>(); p->pid = pid; auto ptr = p.get(); m_map_privateShop.emplace(pid, std::move(p)); return ptr; }
    LPPRIVATE_SHOP GetPrivateShop(DWORD pid) { auto it = m_map_privateShop.find(pid); return it == m_map_privateShop.end() ? nullptr : it->second.get(); }
    CPeer* GetPeer(DWORD id) { return id == peer.id ? &peer : nullptr; }
    Cache* GetPrivateShopCache(DWORD) { return &cache; }
    void UpdatePrivateShopItemCacheSet(DWORD) { ++itemFlushes; }
    void PrivateShopDestroy(LPPRIVATE_SHOP p) { assert(!p->GetItemCount() && !p->gold && !p->cheque); ++destroyed; DeletePrivateShop(p->pid); }
    bool DeletePrivateShop(DWORD);
    void PrivateShopGameDespawn(LPPRIVATE_SHOP);
    void PrivateShopStartPremiumEvent(DWORD);
    void PrivateShopEndPremiumEvent(DWORD);
    bool IsPrivateShopPremiumEvent(DWORD);
    void UpdatePrivateShopPremiumEvent();
    void PrivateShopFailedBuy(const char*);
    void PrivateShopBuy(CPeer*, DWORD, const char*);
    void PrivateShopWithdraw(const char*);
};
#include "private_shop_functions.inc"

int main() {
    {
        CClientManager m; auto shop = m.Add(); shop->gold = 100;
        m.PrivateShopStartPremiumEvent(1);
        assert(shop->premium == now + 300 && shop->persistedPremium == shop->premium && m.cache.flushes == 1);
        m.PrivateShopGameDespawn(shop);
        assert(m.GetPrivateShop(1) == shop && shop->gold == 100 && shop->state == STATE_CLOSED);
        assert(!m.IsPrivateShopPremiumEvent(1) && m.destroyed == 0);
    }
    {
        CClientManager m; auto shop = m.Add(); shop->gold = 500; shop->items.push_back({});
        m.PrivateShopStartPremiumEvent(1); now += 301;
        m.UpdatePrivateShopPremiumEvent();
        assert(m.GetPrivateShop(1) == shop && shop->gold == 500 && shop->items.size() == 1);
        assert(shop->state == STATE_CLOSED && !m.IsPrivateShopPremiumEvent(1));
    }
    {
        CClientManager m; auto shop = m.Add(); Item item;
        item.bAvailable = false; item.dwReservation = 17; item.dwBuyerPeer = 2;
        item.TItem.TPrice.llGold = 90; shop->items.push_back(item);
        m.UpdatePrivateShopPremiumEvent();
        assert(!shop->items[0].bAvailable && m.peer.packets == 1);
        TPacketGDPrivateShopFailedBuy cancel{16, 1, 0};
        m.PrivateShopFailedBuy(reinterpret_cast<const char*>(&cancel));
        assert(!shop->items[0].bAvailable);
        TPacketGDPrivateShopBuy buy{}; buy.dwReservation = 17; buy.TItem = item.TItem;
        m.PrivateShopBuy(&m.peer, 0, reinterpret_cast<const char*>(&buy));
        assert(shop->GetItemCount() == 0 && shop->gold == 90 && shop->state == STATE_CLOSED);
        cancel.dwReservation = 17;
        m.PrivateShopFailedBuy(reinterpret_cast<const char*>(&cancel));
        assert(shop->gold == 90 && m.destroyed == 0);
    }
    {
        CClientManager m; auto shop = m.Add(); Item item;
        item.bAvailable = false; item.dwReservation = 9; item.dwBuyerPeer = 2;
        shop->items.push_back(item);
        m.PrivateShopStartPremiumEvent(1); now += 301;
        m.UpdatePrivateShopPremiumEvent();
        assert(shop->state == STATE_OPEN && !shop->items[0].bAvailable);
        TPacketGDPrivateShopFailedBuy cancel{9, 1, 0};
        m.PrivateShopFailedBuy(reinterpret_cast<const char*>(&cancel));
        m.UpdatePrivateShopPremiumEvent();
        assert(shop->state == STATE_CLOSED && shop->items[0].bAvailable);
    }
    {
        CClientManager m; auto shop = m.Add(); shop->gold = 5000000000LL;
        TPacketGDPrivateShopWithdraw withdrawal{1, 1999999999, 0};
        m.PrivateShopWithdraw(reinterpret_cast<const char*>(&withdrawal));
        assert(shop->gold == 3000000001LL && m.destroyed == 0);
        withdrawal.llGold = shop->gold;
        m.PrivateShopWithdraw(reinterpret_cast<const char*>(&withdrawal));
        assert(!m.GetPrivateShop(1) && m.destroyed == 1);
    }
    {
        CClientManager m; auto shop = m.Add();
        m.PrivateShopStartPremiumEvent(1); now += 301;
        m.UpdatePrivateShopPremiumEvent();
        assert(!m.GetPrivateShop(1) && m.destroyed == 1 && m.m_list_privateShopPremium.empty());
    }
    std::puts("PASS: offline deadline, non-owning despawn, retained expiry, reservation ordering, sold-out earnings, partial withdrawal, empty cleanup");
}
