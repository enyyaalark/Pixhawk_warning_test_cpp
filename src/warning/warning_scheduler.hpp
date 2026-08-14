#pragma once
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include "warning_event.hpp"
namespace warning {
int severity_rank(Severity severity) noexcept;
class WarningScheduler {
public:
    explicit WarningScheduler(double repeat_interval_s = 5.0, double cooldown_s = 1.0);
    void submit(const WarningEvent& event, double now_s);
    std::optional<WarningEvent> next(double now_s);
    std::optional<WarningEvent> highest_active() const;
    std::size_t active_count() const noexcept { return _active.size(); }
private:
    struct State { WarningEvent event; double last_announced{-1e30}; };
    double _repeat_interval_s, _cooldown_s, _last_output_s{-1e30};
    std::unordered_map<std::string, State> _active;
    std::deque<WarningEvent> _transitions;
};
}
