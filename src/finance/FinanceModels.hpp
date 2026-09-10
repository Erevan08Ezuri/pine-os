#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Pine::Finance {
enum class AccountType{Checking,Savings,Cash,CreditCard,Loan,Investment,OtherAsset,OtherLiability};
enum class TransactionType{Expense,Income,Transfer,Refund,Adjustment};
enum class TransactionStatus{Pending,Posted};
struct Account{std::string id,name,type,institution,currency{"USD"},color{"gold"},syncProvider{"manual"},syncStatus{"Manual"};std::int64_t currentBalance{},availableBalance{},createdAt{},updatedAt{},lastSyncTime{},creditLimit{},statementBalance{},minimumPayment{},aprBasisPoints{};std::string paymentDueDate;bool archived{};};
struct Transaction{std::string id,accountId,type,currency{"USD"},merchant,originalDescription,description,category,date,status{"Posted"},notes,transferId,recurringId,providerTransactionId;std::int64_t amount{},createdAt{},updatedAt{};};
struct Category{std::string id,name,icon;bool builtIn{},hidden{};};
struct Budget{std::string id,category,month;std::int64_t limit{};bool rollover{};int warningPercent{80};};
struct Bill{std::string id,name,accountId,category,frequency,nextDueDate,notes;std::int64_t amount{};bool autopay{},reminderEnabled{true};int reminderDays{3};};
struct Subscription{std::string id,name,frequency,accountId,nextCharge,category;std::int64_t price{};bool active{true};};
struct Goal{std::string id,name,targetDate,linkedAccountId;std::int64_t targetAmount{},currentAmount{},createdAt{};bool completed{};};
struct BalanceSnapshot{std::string accountId;std::int64_t balance{},timestamp{};};
struct FinanceSettings{std::string defaultCurrency{"USD"},autoLock{"5 minutes"},weekStart{"Monday"};bool hideBalances{},requireAuthentication{},notificationPreviews{true},notificationsEnabled{true};std::int64_t totalMonthlyBudget{};int budgetWarningPercent{80},dashboardMask{511};};
struct DashboardSummary{std::int64_t assets{},liabilities{},netWorth{},incomeMonth{},spendingMonth{},budgetLimit{},budgetRemaining{},subscriptionMonthly{};};
struct CategorySpend{std::string category;std::int64_t amount{};};
struct ImportResult{std::size_t imported{},duplicates{},invalid{};std::vector<std::string>errors;};
}
