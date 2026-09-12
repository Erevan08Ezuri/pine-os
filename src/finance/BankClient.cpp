#include "finance/BankClient.hpp"
#include "core/AtomicFile.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#ifdef PINE_TAB5
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#elif defined(PINE_BANK_CURL)
#include <curl/curl.h>
#endif

namespace Pine::Finance {
namespace {
constexpr std::size_t maxResponse=512*1024;
void validate(const std::string&url,const std::string&token){
  if(!url.starts_with("https://")||url.size()<9||url.size()>240||
     url.find_first_of("?#@\\ \r\n\t")!=std::string::npos)
    throw std::invalid_argument("USE AN HTTPS SERVER URL");
  if(token.size()<32||token.size()>256||!std::all_of(token.begin(),token.end(),[](unsigned char c){return c>=33&&c<=126;}))
    throw std::invalid_argument("INVALID DEVICE TOKEN");
}
#ifdef PINE_TAB5
esp_err_t receive(esp_http_client_event_t*event){
  if(event->event_id==HTTP_EVENT_ON_DATA){
    auto&body=*static_cast<std::string*>(event->user_data);
    if(body.size()+event->data_len>maxResponse)return ESP_FAIL;
    body.append(static_cast<const char*>(event->data),event->data_len);
  }
  return ESP_OK;
}
#elif defined(PINE_BANK_CURL)
std::size_t receive(char*data,std::size_t size,std::size_t count,void*context){
  auto&body=*static_cast<std::string*>(context);const auto bytes=size*count;
  if(bytes>maxResponse-body.size())return 0;
  body.append(data,bytes);return bytes;
}
#endif
}
bool BankClient::configured()const{try{std::ifstream in(path_);nlohmann::json j;in>>j;validate(j.at("url"),j.at("token"));return true;}catch(...){return false;}}
void BankClient::configure(const std::string&url,const std::string&token)const{
  auto base=url;while(base.ends_with('/'))base.pop_back();validate(base,token);
  writeAtomicFile(path_,nlohmann::json{{"url",base},{"token",token}}.dump());
#ifndef PINE_TAB5
  std::filesystem::permissions(path_,std::filesystem::perms::owner_read|std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace);
#endif
}
nlohmann::json BankClient::request(const std::string&method,const std::string&path)const{
  std::ifstream in(path_);nlohmann::json settings;
  if(!in)throw std::runtime_error("CONFIGURE BANK SERVER FIRST");
  in>>settings;const auto base=settings.at("url").get<std::string>(),token=settings.at("token").get<std::string>();
  validate(base,token);
  if(!path.starts_with("/v1/banks")||path.find_first_of("?#\r\n ")!=std::string::npos)
    throw std::invalid_argument("INVALID BANK REQUEST");
  const auto url=base+path;const auto auth="Bearer "+token;std::string body;int status=0;
#ifdef PINE_TAB5
  esp_http_client_config_t config{};config.url=url.c_str();config.timeout_ms=35000;
  config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;
  config.event_handler=receive;config.user_data=&body;
  auto client=esp_http_client_init(&config);if(!client)throw std::runtime_error("BANK CLIENT UNAVAILABLE");
  esp_http_client_set_header(client,"Authorization",auth.c_str());
  esp_http_client_set_method(client,method=="POST"?HTTP_METHOD_POST:method=="DELETE"?HTTP_METHOD_DELETE:HTTP_METHOD_GET);
  auto result=esp_http_client_perform(client);status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);
  if(result!=ESP_OK)throw std::runtime_error("BANK SERVER UNREACHABLE / CHECK WIFI AND TIME");
#elif defined(PINE_BANK_CURL)
  static const bool initialized=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
  if(!initialized)throw std::runtime_error("BANK CLIENT UNAVAILABLE");
  auto*client=curl_easy_init();if(!client)throw std::runtime_error("BANK CLIENT UNAVAILABLE");
  auto*headers=curl_slist_append(nullptr,("Authorization: "+auth).c_str());
  curl_easy_setopt(client,CURLOPT_URL,url.c_str());curl_easy_setopt(client,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(client,CURLOPT_CUSTOMREQUEST,method.c_str());curl_easy_setopt(client,CURLOPT_TIMEOUT,35L);
  curl_easy_setopt(client,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(client,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(client,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(client,CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(client,CURLOPT_WRITEDATA,&body);
  auto result=curl_easy_perform(client);long code=0;curl_easy_getinfo(client,CURLINFO_RESPONSE_CODE,&code);status=static_cast<int>(code);
  curl_slist_free_all(headers);curl_easy_cleanup(client);
  if(result!=CURLE_OK)throw std::runtime_error("BANK SERVER UNREACHABLE / CHECK HTTPS");
#else
  (void)url;(void)auth;(void)method;(void)status;
  throw std::runtime_error("BUILD DESKTOP WITH LIBCURL FOR BANK SYNC");
#endif
  if(status==401)throw std::runtime_error("BANK DEVICE TOKEN REJECTED");
  if(status<200||status>=300)throw std::runtime_error("BANK SERVER REQUEST FAILED ("+std::to_string(status)+")");
  return nlohmann::json::parse(body);
}
}
