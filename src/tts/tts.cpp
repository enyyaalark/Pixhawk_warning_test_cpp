#include "tts.hpp"
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace tts {
ConsoleAudioOutput::ConsoleAudioOutput(std::function<void(const std::string&)>s):_sink(s?std::move(s):[](const std::string&v){std::cout<<"VOICE: "<<v<<'\n';}){}
bool ConsoleAudioOutput::speak(const warning::WarningEvent&e){_sink(e.message);return true;}
WavAudioOutput::WavAudioOutput(std::filesystem::path d,std::string p):_dir(std::move(d)),_player(std::move(p)){}
bool WavAudioOutput::speak(const warning::WarningEvent&e){stop();auto path=_dir/(e.code+(e.active?"_ACTIVE.wav":"_CLEAR.wav"));if(!std::filesystem::exists(path))return false;pid_t p=fork();if(p==0){execlp(_player.c_str(),_player.c_str(),"-q",path.c_str(),nullptr);_exit(127);}if(p<0)return false;_pid=p;return true;}
void WavAudioOutput::stop(){if(_pid>0){kill(_pid,SIGTERM);waitpid(_pid,nullptr,WNOHANG);_pid=-1;}}
}
