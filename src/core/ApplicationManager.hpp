#pragma once
#include "core/Application.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
namespace Pine { class ApplicationManager { public: void registerApp(std::unique_ptr<Application> app); bool launch(const std::string& id); void close(); void home(){close();} Application* current()const{return current_;} std::string currentId()const; std::vector<Application*> applications()const; private:std::unordered_map<std::string,std::unique_ptr<Application>> apps_;Application* current_{nullptr};}; }
