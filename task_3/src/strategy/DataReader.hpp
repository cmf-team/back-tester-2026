#pragma once
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Trade
{
    uint64_t timestamp;
    bool is_sell;
    double price;
    double amount;
};

struct BookLevel
{
    double price = 0.0;
    double amount = 0.0;
};

struct BookSnapshot
{
    uint64_t timestamp = 0;
    std::vector<BookLevel> asks;
    std::vector<BookLevel> bids;

    double best_ask() const { return asks.empty() ? 0.0 : asks[0].price; }
    double best_bid() const { return bids.empty() ? 0.0 : bids[0].price; }
    double mid() const { return (best_ask() + best_bid()) / 2.0; }
    double spread() const { return best_ask() - best_bid(); }
};

inline double safe_stod(const std::string& s)
{
    if (s.empty())
        return 0.0;
    try
    {
        return std::stod(s);
    }
    catch (...)
    {
        return 0.0;
    }
}
inline uint64_t safe_stoull(const std::string& s)
{
    if (s.empty())
        return 0;
    try
    {
        return std::stoull(s);
    }
    catch (...)
    {
        return 0;
    }
}

// trades.csv: ,local_timestamp,side,price,amount
inline std::vector<Trade> read_trades(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open trades: " + path);
    std::vector<Trade> result;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string idx, ts, side, price, amount;
        std::getline(ss, idx, ',');
        std::getline(ss, ts, ',');
        std::getline(ss, side, ',');
        std::getline(ss, price, ',');
        std::getline(ss, amount, ',');
        if (ts.empty() || price.empty())
            continue;
        Trade t;
        t.timestamp = safe_stoull(ts);
        t.is_sell = (side == "sell");
        t.price = safe_stod(price);
        t.amount = safe_stod(amount);
        result.push_back(t);
    }
    return result;
}

// lob.csv: ,local_timestamp,asks[0].price,asks[0].amount,bids[0].price,bids[0].amount,...
inline std::vector<BookSnapshot> read_lob(const std::string& path, int depth = 25)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open lob: " + path);
    std::vector<BookSnapshot> result;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;
        std::vector<std::string> vals;
        std::stringstream ss(line);
        std::string v;
        while (std::getline(ss, v, ','))
            vals.push_back(v);
        if (vals.size() < 2)
            continue;
        BookSnapshot snap;
        snap.timestamp = safe_stoull(vals[1]);
        snap.asks.resize(depth);
        snap.bids.resize(depth);
        for (int i = 0; i < depth; ++i)
        {
            int base = 2 + i * 4;
            if (base + 3 >= (int)vals.size())
                break;
            snap.asks[i].price = safe_stod(vals[base]);
            snap.asks[i].amount = safe_stod(vals[base + 1]);
            snap.bids[i].price = safe_stod(vals[base + 2]);
            snap.bids[i].amount = safe_stod(vals[base + 3]);
        }
        result.push_back(snap);
    }
    return result;
}
