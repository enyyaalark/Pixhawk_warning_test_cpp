#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include <cmath>
#include <exception>
#include <iostream>
#include <memory>

#include "pfd_live_model.hpp"
#include "pfd_replay_model.hpp"
#include "telemetry/telemetry_replay.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pixhawk_pfd"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Qt 5 PFD using live MAVLink UDP or normalized replay"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption replay_option(
        QStringList{QStringLiteral("r"), QStringLiteral("replay")},
        QStringLiteral("Normalized telemetry JSONL recording"),
        QStringLiteral("path"));
    QCommandLineOption speed_option(
        QStringList{QStringLiteral("s"), QStringLiteral("speed")},
        QStringLiteral("Replay speed multiplier"),
        QStringLiteral("factor"),
        QStringLiteral("1.0"));
    parser.addOption(replay_option);
    parser.addOption(speed_option);
    QCommandLineOption udp_port_option(
        QStringList{QStringLiteral("u"), QStringLiteral("udp-port")},
        QStringLiteral("Receive live MAVLink UDP (default: 14445)"),
        QStringLiteral("port"), QStringLiteral("14445"));
    QCommandLineOption udp_bind_option(
        QStringList{QStringLiteral("udp-bind")},
        QStringLiteral("Live UDP bind address"), QStringLiteral("address"),
        QStringLiteral("127.0.0.1"));
    parser.addOption(udp_port_option);
    parser.addOption(udp_bind_option);
    parser.process(app);

    try {
        QQmlApplicationEngine engine;
        std::unique_ptr<PfdReplayModel> replay_model;
        std::unique_ptr<PfdLiveModel> live_model;
        QObject* model = nullptr;

        if (parser.isSet(replay_option)) {
            bool speed_ok = false;
            const double speed = parser.value(speed_option).toDouble(&speed_ok);
            if (!speed_ok || !std::isfinite(speed) || speed <= 0.0) {
                std::cerr << "Invalid --speed: expected a finite positive number\n";
                return 2;
            }
            telemetry::TelemetryReplay replay(parser.value(replay_option).toStdString());
            replay_model = std::make_unique<PfdReplayModel>(replay.samples(), speed);
            model = replay_model.get();
        } else {
            bool port_ok = false;
            const int port = parser.value(udp_port_option).toInt(&port_ok);
            if (!port_ok || port < 1 || port > 65535) {
                std::cerr << "Invalid --udp-port: expected 1..65535\n";
                return 2;
            }
            live_model = std::make_unique<PfdLiveModel>(
                static_cast<std::uint16_t>(port),
                parser.value(udp_bind_option).toStdString());
            model = live_model.get();
        }

        engine.rootContext()->setContextProperty(QStringLiteral("pfdModel"), model);
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }

        if (replay_model) {
            QTimer::singleShot(0, replay_model.get(), &PfdReplayModel::start);
        } else {
            QTimer::singleShot(0, live_model.get(), &PfdLiveModel::start);
        }
        return app.exec();
    } catch (const std::exception& error) {
        std::cerr << "Could not start PFD: " << error.what() << '\n';
        return 1;
    }
}
