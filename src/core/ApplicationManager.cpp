#include "core/ApplicationManager.hpp"
#include "core/Logger.hpp"
#include <stdexcept>
namespace Pine {
void ApplicationManager::registerApp(std::unique_ptr<Application> app){if(!app||app->id().empty())throw std::invalid_argument("Invalid application");auto id=app->id();if(apps_.contains(id))throw std::invalid_argument("Duplicate application: "+id);apps_.emplace(id,std::move(app));}
bool ApplicationManager::launch(const std::string& id){auto it=apps_.find(id);if(it==apps_.end())return false;if(current_==it->second.get())return true;if(current_)current_->onClose();current_=it->second.get();current_->onOpen();Logger::instance().info("APP","Launched "+id);return true;}
void ApplicationManager::close(){if(current_){Logger::instance().info("APP","Closed "+current_->id());current_->onClose();current_=nullptr;}}
std::string ApplicationManager::currentId()const{return current_?current_->id():"home";}
std::vector<Application*> ApplicationManager::applications()const{std::vector<Application*> out;out.reserve(apps_.size());for(auto&[id,app]:apps_)out.push_back(app.get());return out;}
}
