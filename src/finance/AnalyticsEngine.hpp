#pragma once
#include "finance/FinanceModels.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace Pine::Finance {class FinanceService;struct CashFlow{std::int64_t income{},expenses{},net{};};struct RecurringSuggestion{std::string merchant;std::int64_t typicalAmount{};int occurrences{};};class AnalyticsEngine{public:explicit AnalyticsEngine(const FinanceService&s):service_(s){}CashFlow cashFlow(const std::string&from,const std::string&to)const;std::int64_t averageDailySpending(const std::string&from,const std::string&to)const;std::vector<Transaction>largestExpenses(const std::string&from,const std::string&to,std::size_t limit=5)const;std::vector<RecurringSuggestion>detectRecurring()const;static std::string normalizeMerchant(const std::string&);private:const FinanceService&service_;};}
