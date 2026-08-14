#include <gtest/gtest.h>
#include "warning/warning_scheduler.hpp"
using namespace warning;
static WarningEvent event(std::string code,Severity s,bool active=true){return {std::move(code),s,"message",{},active};}
TEST(WarningSchedulerTest,PrioritizesAndDeduplicates){WarningScheduler q(5,0);q.submit(event("LOW",Severity::CAUTION),0);q.submit(event("HIGH",Severity::CRITICAL),0);q.submit(event("HIGH",Severity::CRITICAL),0);EXPECT_EQ(q.active_count(),2);EXPECT_EQ(q.next(0)->code,"HIGH");EXPECT_EQ(q.next(0)->code,"LOW");EXPECT_FALSE(q.next(0));}
TEST(WarningSchedulerTest,RepeatsAndClears){WarningScheduler q(5,0);q.submit(event("A",Severity::WARNING),0);ASSERT_TRUE(q.next(0));EXPECT_FALSE(q.next(4));ASSERT_TRUE(q.next(5));q.submit(event("A",Severity::WARNING,false),6);auto clear=q.next(6);ASSERT_TRUE(clear);EXPECT_FALSE(clear->active);EXPECT_EQ(q.active_count(),0);}
TEST(WarningSchedulerTest,CooldownApplies){WarningScheduler q(5,1);q.submit(event("A",Severity::WARNING),0);ASSERT_TRUE(q.next(0));q.submit(event("B",Severity::CRITICAL),.1);EXPECT_FALSE(q.next(.5));EXPECT_EQ(q.next(1)->code,"B");}
