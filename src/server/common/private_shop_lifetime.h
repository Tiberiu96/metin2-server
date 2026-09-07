#pragma once

// Test duration. Set to 48 * 60 * 60 for production; existing deadlines are retained.
constexpr unsigned PRIVATE_SHOP_LIFETIME_SECONDS = 120;

inline bool PrivateShopDeadlinePassed(long long deadline, long long now)
{
    return deadline <= now;
}
