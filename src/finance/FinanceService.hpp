#pragma once
#include "finance/FinanceDatabase.hpp"
#include "finance/FinanceModels.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Pine::Finance {
class FinanceService{
public:
  explicit FinanceService(const std::filesystem::path&dataRoot);
  Account createAccount(std::string name,std::string type,std::string institution,std::int64_t startingBalance,std::string currency="USD");
  bool updateAccount(const Account&);bool archiveAccount(const std::string&,bool=true);std::optional<Account>account(const std::string&)const;std::vector<Account>accounts(bool includeArchived=false)const;
  Transaction addTransaction(std::string accountId,std::string type,std::int64_t amount,std::string merchant,std::string category,std::string date,std::string notes="",std::string status="Posted");
  std::pair<Transaction,Transaction>transfer(const std::string&from,const std::string&to,std::int64_t amount,std::string date,std::string notes="");
  bool deleteTransaction(const std::string&);Transaction reconcile(const std::string&accountId,std::int64_t actualBalance,std::string date);
  std::vector<Transaction>transactions(std::string search="",std::string accountId="",std::string category="",std::string type="",std::string status="",std::string fromDate="",std::string toDate="",std::optional<std::int64_t>minAmount={},std::optional<std::int64_t>maxAmount={},std::size_t limit=100,std::size_t offset=0)const;
  Category createCategory(std::string name,std::string icon);bool renameCategory(const std::string&id,const std::string&name);bool hideCategory(const std::string&id,bool);std::vector<Category>categories(bool includeHidden=false)const;
  Budget setBudget(std::string category,std::string month,std::int64_t limit,bool rollover=false,int warning=80);bool deleteBudget(const std::string&);std::vector<Budget>budgets(std::string month="")const;std::int64_t budgetSpent(const Budget&)const;std::int64_t budgetAvailable(const Budget&)const;
  Bill addBill(std::string name,std::int64_t amount,std::string accountId,std::string category,std::string due,std::string frequency,bool autopay=false,bool reminder=true,int reminderDays=3,std::string notes="");bool deleteBill(const std::string&);std::vector<Bill>bills()const;
  Subscription addSubscription(std::string name,std::int64_t price,std::string frequency,std::string accountId,std::string nextCharge,std::string category);bool deleteSubscription(const std::string&);std::vector<Subscription>subscriptions(bool activeOnly=true)const;
  Goal addGoal(std::string name,std::int64_t target,std::string date="",std::string linkedAccount="");bool contribute(const std::string&,std::int64_t);bool deleteGoal(const std::string&);std::vector<Goal>goals(bool includeCompleted=false)const;
  DashboardSummary dashboard(std::string month="")const;std::vector<CategorySpend>spendingByCategory(std::string from,std::string to,std::size_t limit=20)const;
  std::vector<BalanceSnapshot>balanceHistory(const std::string&accountId,std::size_t limit=100)const;
  FinanceSettings settings()const;void saveSettings(const FinanceSettings&);void eraseAll();
  FinanceDatabase&database(){return db_;}const FinanceDatabase&database()const{return db_;}
  static std::string today();static std::string currentMonth();
private:void seedCategories();void snapshotIfNeeded(const std::string&,std::int64_t);static std::string id();FinanceDatabase db_;
};
}
