#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include "warning/warning_event.hpp"
namespace tts {
class AlertAudioOutput { public: virtual ~AlertAudioOutput()=default; virtual bool speak(const warning::WarningEvent&)=0; virtual void stop()=0; };
class ConsoleAudioOutput final:public AlertAudioOutput { public: explicit ConsoleAudioOutput(std::function<void(const std::string&)> sink={}); bool speak(const warning::WarningEvent&)override; void stop()override{} private:std::function<void(const std::string&)> _sink; };
class WavAudioOutput final:public AlertAudioOutput { public: explicit WavAudioOutput(std::filesystem::path dir,std::string player="aplay"); ~WavAudioOutput()override{stop();} bool speak(const warning::WarningEvent&)override; void stop()override; private:std::filesystem::path _dir;std::string _player;int _pid{-1}; };
}
