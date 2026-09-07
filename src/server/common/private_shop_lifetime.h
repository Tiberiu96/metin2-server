#pragma once

// Production duration for newly created shops: 48 hours.
constexpr unsigned PRIVATE_SHOP_LIFETIME_SECONDS = 48 * 60 * 60;

inline bool PrivateShopDeadlinePassed(long long deadline, long long now)
{
    return deadline <= now;
}
