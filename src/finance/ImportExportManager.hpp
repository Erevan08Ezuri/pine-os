#pragma once
#include "finance/FinanceModels.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace Pine {class FileService;namespace Finance {
class FinanceService;
struct CsvMapping{std::size_t date{},description{1},amount{2};std::optional<std::size_t>category;bool firstRowHeader{true};};
struct ImportPreviewRow{std::string date,merchant,category,providerId;std::int64_t amount{};bool duplicate{},valid{};std::string error;};
class ImportExportManager{public:ImportExportManager(FinanceService&,FileService&);std::vector<ImportPreviewRow>previewCsv(const std::filesystem::path&,const std::string&accountId,const CsvMapping&,std::size_t maxRows=1000)const;ImportResult importCsv(const std::filesystem::path&,const std::string&accountId,const CsvMapping&,bool confirmed);std::vector<ImportPreviewRow>previewOfx(const std::filesystem::path&,const std::string&accountId,std::size_t maxRows=1000)const;ImportResult importOfx(const std::filesystem::path&,const std::string&accountId,bool confirmed);std::filesystem::path exportTransactionsCsv(const std::filesystem::path&)const;std::filesystem::path exportEverythingJson(const std::filesystem::path&)const;std::filesystem::path createBackup(const std::filesystem::path&)const;bool restoreBackup(const std::filesystem::path&,bool confirmed);private:FinanceService&service_;FileService&files_;};}}
