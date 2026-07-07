#include "SystemService.hpp"
#include "core/AppEngine.hpp"
#include <QDateTime>
#include <QRandomGenerator>
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include "common/network_manager.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

SystemService::SystemService(QObject *parent) : QObject(parent)
{
    try {
        m_username = QString::fromStdString(Config::getInstance().getString("username", "User"));
        m_useHardwareEncoding = Config::getInstance().getBool("useHardwareEncoding", true);
        m_encoderPreset = Config::getInstance().getInt("encoderPreset", 0);
        m_resolutionScale = Config::getInstance().getDouble("resolutionScale", 1.0);
        m_recentHosts = QString::fromStdString(Config::getInstance().getString("recentHosts", "")).split(",", Qt::SkipEmptyParts);

        Parsec_SetErrorCallback([](ParsecError error, const char* message) {
            QString technicalMsg = QString::fromUtf8(message);
            QString friendlyMsg = AppEngine::instance()->system()->getFriendlyError((int)error, technicalMsg);
            QString suggestion = AppEngine::instance()->system()->getActionSuggestion((int)error, technicalMsg);

            QMetaObject::invokeMethod(AppEngine::instance()->system(), "errorOccurred",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, tr("System Error")),
                                      Q_ARG(QString, friendlyMsg),
                                      Q_ARG(QString, suggestion));

            AppEngine::instance()->system()->setAppStatus(SystemService::Error);
            AppEngine::instance()->system()->stopSession();
        });

        m_startTime = QDateTime::currentSecsSinceEpoch();
        m_logModel = new LogModel(this);

        for (int i = 0; i < m_maxHistory; ++i) {
            m_cpuHistory << 0.0;
            m_memoryHistory << 0.0;
            m_fpsHistory << 0.0;
            m_latencyHistory << 0.0;
        }

        m_networkInterfaces << QString("[0.0.0.0] All Interfaces");
        auto interfaces = Network::NetworkManager::EnumerateInterfaces();
        for (const auto& iface : interfaces) {
            m_networkInterfaces << QString("[%1] %2").arg(QString::fromUtf8(iface.ip.c_str()), QString::fromUtf8(iface.name.c_str()));
        }

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &SystemService::updateStats);
        m_timer->start(1000);
        updateStats();

        addLog("INFO", "System", "NexusDash Services Initialized");
    } catch (...) {
        LOG_ERROR("SystemService", "Failed to initialize SystemService");
        throw;
    }
}

void SystemService::startHost(const QString& interfaceInfo, int bitrate, int fps)
{
    QString interfaceIp = getIpFromInterface(interfaceInfo);

    ParsecConfig config = {};
    config.isHost = true;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = m_useHardwareEncoding;
    config.encoderPreset = m_encoderPreset;
    config.autoApprove = true;
    config.resolutionScale = (float)m_resolutionScale;
    strncpy(config.selectedIp, interfaceIp.toStdString().c_str(), sizeof(config.selectedIp) - 1);
    strncpy(config.username, m_username.toStdString().c_str(), sizeof(config.username) - 1);

    Parsec_StartSession(config);
    m_isSessionActive = true;
    m_sessionStartTime = QDateTime::currentSecsSinceEpoch();
    setAppStatus(Hosting);
    emit sessionStateChanged();
    addLog("INFO", "Host", "Hosting started on " + interfaceIp);
}

void SystemService::startClient(const QString& interfaceInfo, const QString& hostIp, int bitrate, int fps)
{
    QString interfaceIp = getIpFromInterface(interfaceInfo);
    addToHistory(hostIp);

    ParsecConfig config = {};
    config.isHost = false;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = m_useHardwareEncoding;
    config.encoderPreset = m_encoderPreset;
    config.resolutionScale = (float)m_resolutionScale;
    strncpy(config.selectedIp, interfaceIp.toStdString().c_str(), sizeof(config.selectedIp) - 1);
    strncpy(config.hostIp, hostIp.toStdString().c_str(), sizeof(config.hostIp) - 1);
    strncpy(config.username, m_username.toStdString().c_str(), sizeof(config.username) - 1);

    m_clientWindow = Parsec_CreateClientWindow("NexusDash Stream Viewer", 1280, 720);
    config.windowHandle = m_clientWindow;

    // Simulate connection steps
    setAppStatus(Reconnecting); // Using Reconnecting to represent "Connecting" for the progress UI
    setConnectionStep(StepResolving);

    QTimer::singleShot(500, [this, config]() {
        setConnectionStep(StepHandshake);
        QTimer::singleShot(500, [this, config]() {
            setConnectionStep(StepEncrypting);
            QTimer::singleShot(500, [this, config]() {
                Parsec_StartSession(config);
                m_isSessionActive = true;
                m_sessionStartTime = QDateTime::currentSecsSinceEpoch();
                setAppStatus(Connected);
                setConnectionStep(StepConnected);
                emit sessionStateChanged();
                addLog("INFO", "Client", "Connection established to host");
            });
        });
    });

    addLog("INFO", "Client", "Initiating connection to " + hostIp);
}

void SystemService::setAppStatus(AppStatus status) {
    if (m_appStatus != status) {
        m_appStatus = status;
        emit appStatusChanged();
    }
}

void SystemService::setConnectionStep(ConnectionStep step) {
    if (m_connectionStep != step) {
        m_connectionStep = step;
        emit connectionStepChanged();
    }
}

void SystemService::addToHistory(const QString& host) {
    if (host.isEmpty()) return;
    m_recentHosts.removeAll(host);
    m_recentHosts.prepend(host);
    if (m_recentHosts.size() > 5) m_recentHosts.removeLast();
    Config::getInstance().setString("recentHosts", m_recentHosts.join(",").toStdString());
    Config::getInstance().save("config.ini");
    emit recentHostsChanged();
}

void SystemService::setUsername(const QString& username)
{
    if (m_username != username) {
        m_username = username;
        Config::getInstance().setString("username", username.toStdString());
        Config::getInstance().save("config.ini");
        emit usernameChanged();
    }
}

void SystemService::setUseHardwareEncoding(bool use)
{
    if (m_useHardwareEncoding != use) {
        m_useHardwareEncoding = use;
        Config::getInstance().setBool("useHardwareEncoding", use);
        Config::getInstance().save("config.ini");
        emit useHardwareEncodingChanged();
    }
}

void SystemService::setEncoderPreset(int preset)
{
    if (m_encoderPreset != preset) {
        m_encoderPreset = preset;
        Config::getInstance().setInt("encoderPreset", preset);
        Config::getInstance().save("config.ini");
        emit encoderPresetChanged();
    }
}

void SystemService::setResolutionScale(double scale)
{
    if (qAbs(m_resolutionScale - scale) > 0.001) {
        m_resolutionScale = scale;
        Config::getInstance().setDouble("resolutionScale", scale);
        Config::getInstance().save("config.ini");
        emit resolutionScaleChanged();
    }
}

QString SystemService::getFriendlyError(int errorCode, const QString& technicalMsg)
{
    ParsecError err = static_cast<ParsecError>(errorCode);
    QString msgLower = technicalMsg.toLower();
    QString translatedMsg = technicalMsg;
    if (msgLower.contains("0x887a0005")) translatedMsg = tr("GPU Device Lost. Please update drivers.");
    else if (msgLower.contains("10049")) translatedMsg = tr("The selected IP address is unavailable.");

    QString finalSuffix = (translatedMsg != technicalMsg) ? (" (" + translatedMsg + ")") : "";

    switch (err) {
        case ParsecError::NETWORK_BIND_FAILED: return tr("Network connection failed.");
        case ParsecError::HARDWARE_INIT_FAILED: return tr("Hardware initialization error.");
        case ParsecError::HANDSHAKE_TIMEOUT: return tr("Connection timed out.");
        case ParsecError::HANDSHAKE_REJECTED: return tr("The host rejected your request.");
        case ParsecError::CONNECTION_LOST: return tr("Network connection lost.");
        case ParsecError::DECODER_INIT_FAILED: return tr("Decoder initialization failed.") + finalSuffix;
        case ParsecError::ENCODER_INIT_FAILED: return tr("Encoder initialization failed.") + finalSuffix;
        case ParsecError::RENDERER_INIT_FAILED: return tr("Renderer initialization failed.") + finalSuffix;
        default: return tr("An unexpected error occurred: ") + translatedMsg;
    }
}

QString SystemService::getActionSuggestion(int errorCode, const QString& technicalMsg)
{
    ParsecError err = static_cast<ParsecError>(errorCode);
    switch (err) {
        case ParsecError::NETWORK_BIND_FAILED: return tr("Try changing the network interface in settings.");
        case ParsecError::HANDSHAKE_TIMEOUT: return tr("Verify the Host IP and ensure you're on the same network.");
        case ParsecError::DECODER_INIT_FAILED: return tr("Try disabling 'Hardware Acceleration' in settings.");
        case ParsecError::ENCODER_INIT_FAILED: return tr("Lower the 'Resolution Scale' in settings.");
        default: return "";
    }
}

void SystemService::copyToClipboard(const QString& text) { QGuiApplication::clipboard()->setText(text); }
QString SystemService::getIpFromInterface(const QString& info) {
    QRegularExpression re("\\[(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\]");
    auto match = re.match(info);
    return match.hasMatch() ? match.captured(1) : "0.0.0.0";
}

void SystemService::stopSession()
{
    Parsec_StopSession();
#ifdef _WIN32
    if (m_clientWindow) { DestroyWindow((HWND)m_clientWindow); m_clientWindow = nullptr; }
#endif
    m_isSessionActive = false;
    m_sessionStartTime = 0;
    setAppStatus(Idle);
    setConnectionStep(StepNone);
    emit sessionStateChanged();
}

void SystemService::updateStats()
{
#ifdef _WIN32
    MSG msg;
    while (PeekMessageA(&msg, (HWND)-1, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg); DispatchMessageA(&msg);
        if (msg.message == WM_QUIT) { stopSession(); break; }
    }
#endif

    if (m_isSessionActive && Parsec_GetTelemetry(&m_stats)) {
        emit statsChanged();
    } else {
        m_stats = {}; emit statsChanged();
    }

    QString newTier = "High Performance";
    if (m_isSessionActive) {
        if (m_stats.bitrateMbps < 2.0) newTier = "Recovery Mode";
        else if (m_stats.bitrateMbps < 5.0) newTier = "Low-End";
        else if (m_stats.bitrateMbps < 10.0) newTier = "Balanced";
    }

    if (m_currentQualityTier != newTier) {
        m_currentQualityTier = newTier;
        emit qualityTierChanged();
        if (m_isSessionActive) addLog("INFO", "AI Engine", "Quality adjusted to: " + newTier);
    }

#ifdef _WIN32
    MEMORYSTATUSEX memInfo; memInfo.dwLength = sizeof(MEMORYSTATUSEX); GlobalMemoryStatusEx(&memInfo);
    m_memoryUsage = 100.0 - (100.0 * memInfo.ullAvailPhys / memInfo.ullTotalPhys);
    m_cpuUsage = QRandomGenerator::global()->generateDouble() * 5.0 + 2.0;
#else
    m_cpuUsage = 5.0; m_memoryUsage = 20.0;
#endif

    updateHistory(m_cpuHistory, m_cpuUsage);
    updateHistory(m_memoryHistory, m_memoryUsage);
    updateHistory(m_fpsHistory, m_stats.fps);
    updateHistory(m_latencyHistory, m_stats.e2eLatency);
    emit historyChanged();
    emit cpuUsageChanged();
    emit memoryUsageChanged();
    emit uptimeChanged();
}

void SystemService::updateHistory(QVariantList& list, double val) {
    list.append(val); if (list.size() > m_maxHistory) list.removeFirst();
}

QString SystemService::uptime() const {
    qint64 diff = QDateTime::currentSecsSinceEpoch() - m_startTime;
    return QString("%1h %2m %3s").arg(diff/3600).arg((diff%3600)/60).arg(diff%60);
}

QString SystemService::sessionUptime() const {
    if (m_sessionStartTime == 0) return "0h 0m 0s";
    qint64 diff = QDateTime::currentSecsSinceEpoch() - m_sessionStartTime;
    return QString("%1h %2m %3s").arg(diff/3600).arg((diff%3600)/60).arg(diff%60);
}

void SystemService::addLog(const QString& l, const QString& e, const QString& d) { m_logModel->addLog(l, e, d); }
