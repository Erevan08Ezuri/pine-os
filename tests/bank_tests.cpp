#include "finance/BankClient.hpp"
#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include "platform/Platform.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Pine;
void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
int main(int argc,char**argv){
  try{
    const auto root=std::filesystem::temp_directory_path()/"pine-bank-tests";
    std::filesystem::remove_all(root);std::filesystem::create_directories(root);
    Finance::BankClient client(root);
    require(!client.configured(),"missing config must not be usable");
    for(const auto*url:{"http://localhost:8765","https://user:pass@host","https://host/?token=x","https://host/#fragment"}){
      bool rejected=false;try{client.configure(url,std::string(40,'x'));}catch(...){rejected=true;}
      require(rejected,"unsafe server URL accepted");
    }
    client.configure("https://example.invalid/",std::string(40,'x'));
    require(client.configured(),"valid HTTPS config rejected");
    std::ifstream input(root/"bank-connection.json");nlohmann::json saved;input>>saved;
    require(saved.at("url")=="https://example.invalid","trailing slash not normalized");
    std::filesystem::remove(root/"bank-connection.json");
    require(SDL_Init(SDL_INIT_VIDEO),"SDL unavailable");
    const auto fonts=std::filesystem::path(PINE_TEST_ROOT)/"assets/fonts";
    require(initializeFonts(fonts/"Inter-Regular.ttf",fonts/"Inter-SemiBold.ttf"),"fonts unavailable");
    auto*surface=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565);
    auto*renderer=SDL_CreateSoftwareRenderer(surface);require(renderer,"renderer unavailable");
    {
      Configuration config(root);config.load();Shell shell(nullptr,renderer,config,createPlatform("desktop"));
      shell.acceptanceSkipBoot();require(shell.acceptanceLaunch("finance"),"Finance unavailable");
      shell.acceptanceFinanceView("banks");shell.render();
      if(argc>1){
        std::ifstream fixture(argv[1]);nlohmann::json data;fixture>>data;
        shell.acceptanceBankPreview(data.at("snapshot"));shell.render();
        require(shell.captureFrame(root/"bank-balances.bmp"),"balance capture failed");
        shell.acceptanceBankPreview(data.at("snapshot"),data.at("link"));shell.render();
        require(shell.captureFrame(root/"bank-qr.bmp"),"QR capture failed");
      }
      require(shell.finance().accounts().empty(),"bank preview changed manual ledger");
      require(shell.acceptanceFinanceLock("4826"),"Finance lock failed");shell.render();
      require(shell.captureFrame(root/"bank-locked.bmp"),"lock capture failed");
    }
    shutdownFonts();SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);SDL_Quit();
    std::cout<<"Bank client validation, rendering and ledger isolation passed\n";return 0;
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
