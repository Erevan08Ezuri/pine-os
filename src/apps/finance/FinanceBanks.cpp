#include "ui/Shell.hpp"
#include "finance/BankClient.hpp"
#include <algorithm>
#include <chrono>

namespace Pine {
void Shell::beginBankRequest(const std::string&method,const std::string&path){
  if(bankJob_.valid())return;
  bankError_.clear();const auto root=config_.dataRoot();
  bankJob_=std::async(std::launch::async,[root,method,path]{return Finance::BankClient(root).request(method,path);});
}
void Shell::updateBanks(){
  if(bankJob_.valid()&&bankJob_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
    try{auto result=bankJob_.get();if(result.contains("qr")){bankLink_=std::move(result);}
      else if(result.contains("items")){bankSnapshot_=std::move(result);
        if(!bankLink_.empty())for(const auto&s:bankSnapshot_.value("sessions",nlohmann::json::array()))
          if(s.at("id")==bankLink_.at("session_id")&&s.at("state")!="waiting"){
            bankError_=s.at("state")=="connected"?"BANK CONNECTED - SYNCING":"LINK EXPIRED - TRY AGAIN";bankLink_=nlohmann::json::object();break;}}
      else bankLastPoll_=0;
    }catch(...){bankError_="CONNECTION FAILED - CHECK SERVER, TOKEN, WIFI AND TIME";}
  }
}
void Shell::renderFinanceBanks(){
  renderFinanceHeader("LINKED BANKS");
  const bool busy=bankJob_.valid();const bool configured=bankConfigured_;
  if(button({40,225,200,52},"SERVER SETUP")&&!busy){
    openTextPrompt("BANK SERVER HTTPS URL","https://",[this](const std::string&url){
      openTextPrompt("PRIVATE DEVICE TOKEN","",[this,url](const std::string&token){
        try{Finance::BankClient(config_.dataRoot()).configure(url,token);bankSnapshot_=nlohmann::json::object();
          bankLink_=nlohmann::json::object();bankLastPoll_=0;bankConfigured_=true;toast("BANK SERVER SAVED");}
        catch(...){toast("USE HTTPS AND A VALID DEVICE TOKEN");}
      },InputType::Password,256);
    },InputType::Text,240);
  }
  if(button({255,225,200,52},busy?"WORKING":"CONNECT BANK",true)&&!busy&&configured)beginBankRequest("POST","/v1/banks/link");
  if(button({470,225,210,52},"SYNC")&&!busy&&configured)beginBankRequest("POST","/v1/banks/sync");
  if(!configured){sublabel(45,310,"SET UP YOUR PRIVATE BANK SERVER FIRST");sublabel(45,345,"THEN SCAN A QR CODE WITH YOUR PHONE");return;}
  const auto now=SDL_GetTicks();
  if(!busy&&(bankLastPoll_==0||now-bankLastPoll_>10000)){bankLastPoll_=now;beginBankRequest("GET","/v1/banks");}
  if(!bankError_.empty())sublabel(45,290,bankError_);
  if(!bankLink_.empty()){
    sublabel(60,340,"SCAN WITH YOUR PHONE; SELECT CAPITAL ONE OR CHASE");
    const auto rows=bankLink_.value("qr",std::vector<std::string>{});
    if(!rows.empty()&&rows.size()<=177&&std::all_of(rows.begin(),rows.end(),[&](const auto&r){return r.size()==rows.size();})){
      const int cell=static_cast<int>(480/rows.size());const float side=static_cast<float>(rows.size()*cell);
      const float left=(720-side)/2;SDL_SetRenderDrawColor(renderer_,255,255,255,255);
      SDL_FRect box{left,395,side,side};SDL_RenderFillRect(renderer_,&box);SDL_SetRenderDrawColor(renderer_,0,0,0,255);
      for(std::size_t y=0;y<rows.size();++y)for(std::size_t x=0;x<rows.size();++x)if(rows[y][x]=='1'){
        SDL_FRect pixel{left+static_cast<float>(x*cell),395+static_cast<float>(y*cell),static_cast<float>(cell),static_cast<float>(cell)};
        SDL_RenderFillRect(renderer_,&pixel);
      }
    }
    sublabel(100,910,"FINISH SIGN-IN ON YOUR PHONE, THEN RETURN HERE.");
    if(button({200,965,320,55},"HIDE QR CODE"))bankLink_=nlohmann::json::object();
    return;
  }
  sublabel(45,325,bankSnapshot_.value("environment",std::string{})=="production"?
    "BANK DATA / READ ONLY / USD":"SANDBOX / TEST DATA ONLY / USD");
  sublabel(45,355,"SEPARATE FROM MANUAL TOTALS. BALANCES MAY BE DELAYED.");
  auto items=bankSnapshot_.value("items",nlohmann::json::array());float y=405-financeScroll_;
  auto clip=[](std::string s,std::size_t n){return s.size()>n?s.substr(0,n-3)+"...":s;};
  const SDL_Rect content{35,390,655,685};SDL_FlushRenderer(renderer_);SDL_SetRenderClipRect(renderer_,&content);
  for(const auto&item:items){
    const auto id=item.at("id").get<std::string>();panel({40,y,640,155});
    label(60,y+12,clip(item.value("institution",std::string("Bank")),28),2.7f,Theme::Gold);
    const auto synced=item.value("synced_at",std::string{});
    sublabel(60,y+48,"RETRIEVED: "+(synced.empty()?"WAITING":synced.substr(0,19)+" UTC"));
    const auto error=item.value("error",std::string{});if(!error.empty())sublabel(60,y+75,clip(error,48));
    if(y+100>=390&&y+145<1075){
      if(button({60,y+100,260,42},"RECONNECT")&&!busy)beginBankRequest("POST","/v1/banks/"+id+"/reconnect");
      if(button({350,y+100,300,42},"DISCONNECT")&&!busy)openTextPrompt("TYPE REMOVE TO DISCONNECT","",[this,id](const std::string&v){if(v=="REMOVE")beginBankRequest("DELETE","/v1/banks/"+id);},InputType::Text,6);
    }y+=170;
    for(const auto&account:item.at("accounts")){
      panel({40,y,640,90});label(60,y+12,clip(account.value("name",std::string("Account")),28),2.2f);
      sublabel(60,y+49,account.value("type",std::string{})+" / "+account.value("mask",std::string{}));
      label(450,y+42,account.at("current_minor").is_null()?"UNAVAILABLE":financeAmount(account.at("current_minor").get<std::int64_t>(),"USD"),2.2f);y+=103;
    }
    for(const auto&t:item.at("transactions")){
      label(60,y,clip(t.value("name",std::string{}),30),2.1f);
      sublabel(60,y+30,t.value("date",std::string{})+(t.value("pending",false)?" / PENDING":" / POSTED"));
      label(460,y+12,financeAmount(-t.at("amount_minor").get<std::int64_t>(),"USD",true),2.0f);y+=75;
    }y+=25;
  }
  if(items.empty())sublabel(60,440,"CONNECT CAPITAL ONE, THEN REPEAT FOR CHASE");
  financeScroll_=std::clamp(financeScroll_,0.0f,std::max(0.0f,y+financeScroll_-1050));
  SDL_FlushRenderer(renderer_);SDL_SetRenderClipRect(renderer_,nullptr);SDL_FlushRenderer(renderer_);
}
}
