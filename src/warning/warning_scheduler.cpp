#include "warning_scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace warning {
int severity_rank(Severity s) noexcept { return static_cast<int>(s); }
WarningScheduler::WarningScheduler(double repeat, double cooldown):_repeat_interval_s(repeat),_cooldown_s(cooldown){if(!std::isfinite(repeat)||repeat<=0||!std::isfinite(cooldown)||cooldown<0)throw std::invalid_argument("scheduler intervals are invalid");}
void WarningScheduler::submit(const WarningEvent& e,double now){if(!std::isfinite(now)||e.code.empty())throw std::invalid_argument("invalid warning event");if(e.active){auto it=_active.find(e.code);if(it==_active.end()){_active.emplace(e.code,State{e});_transitions.push_back(e);}else it->second.event=e;}else if(_active.erase(e.code)>0)_transitions.push_back(e);}
std::optional<WarningEvent> WarningScheduler::next(double now){if(!std::isfinite(now))throw std::invalid_argument("invalid scheduler time");if(now-_last_output_s<_cooldown_s)return std::nullopt;if(!_transitions.empty()){auto best=std::max_element(_transitions.begin(),_transitions.end(),[](const auto&a,const auto&b){return severity_rank(a.severity)<severity_rank(b.severity);});auto result=*best;_transitions.erase(best);_last_output_s=now;if(result.active)_active[result.code].last_announced=now;return result;}State*best=nullptr;for(auto&[_,s]:_active){if(now-s.last_announced<_repeat_interval_s)continue;if(!best||severity_rank(s.event.severity)>severity_rank(best->event.severity))best=&s;}if(!best)return std::nullopt;best->last_announced=now;_last_output_s=now;return best->event;}
std::optional<WarningEvent> WarningScheduler::highest_active()const{const State*best=nullptr;for(const auto&[_,s]:_active)if(!best||severity_rank(s.event.severity)>severity_rank(best->event.severity))best=&s;return best?std::optional(best->event):std::nullopt;}
}
