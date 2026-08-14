#include <gtest/gtest.h>
#include "tts/tts.hpp"
TEST(TtsTest,ConsoleBackendReceivesMessage){std::string got;tts::ConsoleAudioOutput out([&](const std::string&s){got=s;});warning::WarningEvent e{"TEST",warning::Severity::INFO,"System test",{},true};EXPECT_TRUE(out.speak(e));EXPECT_EQ(got,"System test");}
TEST(TtsTest,MissingWavFailsSafely){tts::WavAudioOutput out("/definitely/missing");warning::WarningEvent e{"TEST",warning::Severity::INFO,"",{},true};EXPECT_FALSE(out.speak(e));}
