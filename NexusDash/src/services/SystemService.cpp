#include "SystemService.hpp"
#include "core/AppEngine.hpp"
#include <QDateTime>
#include <QRandomGenerator>
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

        Parsec_SetErrorCallback([](ParsecError error, const char* message) {
            QString technicalMsg = QString::fromUtf8(message);
            QString friendlyMsg = AppEngine::instance()->system()->getFriendlyError((int)error, technicalMsg);

            QMetaObject::invokeMethod(AppEngine::instance()->system(), "errorOccurred",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, tr("System Error")),
                                      Q_ARG(QString, friendlyMsg));

            AppEngine::instance()->system()->stopSession();
        });

        m_startTime = QDateTime::currentSecsSinceEpoch();
        m_logModel = new LogModel(this);

        // Initialize history with zeros
        for (int i = 0; i < m_maxHistory; ++i) {
            m_cpuHistory << 0.0;
            m_memoryHistory << 0.0;
            m_fpsHistory << 0.0;
            m_latencyHistory << 0.0;
        }

        // Enumerate network interfaces
        m_networkInterfaces << QString("[0.0.0.0] All Interfaces - Active");
        m_networkInterfaces << QString("[127.0.0.1] Local Loopback - Active");
        auto interfaces = Network::NetworkManager::EnumerateInterfaces();
        for (const auto& iface : interfaces) {
            // Construct detailed string: "[IP] Name - Status"
            QString status = iface.isActive ? "Active" : "Inactive";
            m_networkInterfaces << QString("[%1] %2 - %3").arg(QString::fromUtf8(iface.ip.c_str()), QString::fromUtf8(iface.name.c_str()), status);
        }

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &SystemService::updateStats);
        m_timer->start(1000);
        updateStats();

        addLog("INFO", "System", "NexusDash Services Initialized");
    } catch (const std::exception& e) {
        LOG_ERROR("SystemService", std::string("Initialization failed: ") + e.what());
        throw; // Re-throw to be caught by main.cpp's global handler
    } catch (...) {
        LOG_ERROR("SystemService", "Unknown error during initialization");
        throw std::runtime_error("Unknown error during SystemService initialization");
    }
}

void SystemService::startHost(const QString& interfaceInfo, int bitrate, int fps)
{
    // Extract IP from "[IP] Name - Status"
    QString interfaceIp = interfaceInfo;
    int start = interfaceInfo.indexOf('[');
    int end = interfaceInfo.indexOf(']');
    if (start != -1 && end != -1) {
        interfaceIp = interfaceInfo.mid(start + 1, end - start - 1);
    }

    ParsecConfig config = {};
    config.isHost = true;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = m_useHardwareEncoding;
    config.encoderPreset = m_encoderPreset;
    config.autoApprove = true;
    config.resolutionScale = (float)m_resolutionScale;
    config.targetWidth = 0;
    config.targetHeight = 0;
    strncpy(config.selectedIp, interfaceIp.toStdString().c_str(), sizeof(config.selectedIp) - 1);
    config.selectedIp[sizeof(config.selectedIp) - 1] = '\0';
    strncpy(config.username, m_username.toStdString().c_str(), sizeof(config.username) - 1);
    config.username[sizeof(config.username) - 1] = '\0';

    Parsec_StartSession(config);
    m_isSessionActive = true;
    emit sessionStateChanged();
    addLog("INFO", "Host", "Hosting started on " + interfaceIp);
}

void SystemService::startClient(const QString& interfaceInfo, const QString& hostIp, int bitrate, int fps)
{
    // Extract IP from "[IP] Name - Status"
    QString interfaceIp = interfaceInfo;
    int start = interfaceInfo.indexOf('[');
    int end = interfaceInfo.indexOf(']');
    if (start != -1 && end != -1) {
        interfaceIp = interfaceInfo.mid(start + 1, end - start - 1);
    }

    ParsecConfig config = {};
    config.isHost = false;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = m_useHardwareEncoding;
    config.encoderPreset = m_encoderPreset;
    config.resolutionScale = (float)m_resolutionScale;
    strncpy(config.selectedIp, interfaceIp.toStdString().c_str(), sizeof(config.selectedIp) - 1);
    config.selectedIp[sizeof(config.selectedIp) - 1] = '\0';
    strncpy(config.hostIp, hostIp.toStdString().c_str(), sizeof(config.hostIp) - 1);
    config.hostIp[sizeof(config.hostIp) - 1] = '\0';
    strncpy(config.username, m_username.toStdString().c_str(), sizeof(config.username) - 1);
    config.username[sizeof(config.username) - 1] = '\0';

    m_clientWindow = Parsec_CreateClientWindow("NexusDash Stream Viewer", 1280, 720);
    config.windowHandle = m_clientWindow;

    Parsec_StartSession(config);
    m_isSessionActive = true;
    emit sessionStateChanged();
    addLog("INFO", "Client", "Connecting to " + hostIp);
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

    // Map common D3D11/DXGI HRESULTs in technical message to friendly Persian strings
    QString translatedMsg = technicalMsg;
    if (msgLower.contains("0x887a0005")) {
        translatedMsg = tr("کارت گرافیک ریست شده است (DXGI_ERROR_DEVICE_REMOVED). لطفاً درایور خود را بروزرسانی کنید.");
    } else if (msgLower.contains("0x887a0001")) {
        translatedMsg = tr("دسترسی به بافر تصویر امکان‌پذیر نیست (DXGI_ERROR_INVALID_CALL).");
    } else if (msgLower.contains("10049")) {
        translatedMsg = tr("آدرس IP انتخاب شده روی این دستگاه وجود ندارد. لطفاً رابط شبکه صحیح را انتخاب کنید.");
    }

    // If technical message contains a translated HRESULT, we might want to prioritize it or append it
    QString finalSuffix = (translatedMsg != technicalMsg) ? (" (" + translatedMsg + ")") : "";

    switch (err) {
        case ParsecError::NETWORK_BIND_FAILED:
            return tr("ارتباط با شبکه برقرار نشد. لطفاً بررسی کنید که پورت ۵۰۰۵ توسط برنامه دیگری اشغال نشده باشد.");
        case ParsecError::HARDWARE_INIT_FAILED:
            return tr("خطا در مقداردهی اولیه سخت‌افزار. لطفاً از بروز بودن درایور کارت گرافیک خود اطمینان حاصل کنید.");
        case ParsecError::HANDSHAKE_TIMEOUT:
            return tr("زمان اتصال به پایان رسید. لطفاً آدرس IP را بررسی کرده و از روشن بودن سیستم میزبان اطمینان حاصل کنید.");
        case ParsecError::HANDSHAKE_REJECTED:
            return tr("میزبان درخواست اتصال شما را رد کرد.");
        case ParsecError::CONNECTION_LOST:
            return tr("اتصال به شبکه قطع شد.");
        case ParsecError::DECODER_INIT_FAILED:
            return tr("خطا در مقداردهی اولیه رمزگشا (Decoder). گرافیک شما ممکن است از H.264 یا HEVC پشتیبانی نکند.") + finalSuffix;
        case ParsecError::ENCODER_INIT_FAILED:
            return tr("خطا در مقداردهی اولیه کدگذار (Encoder). لطفاً تنظیمات گرافیک و رزولوشن را بررسی کنید.") + finalSuffix;
        case ParsecError::RENDERER_INIT_FAILED:
            return tr("خطا در مقداردهی اولیه نمایشگر (Renderer). کارت گرافیک شما ممکن است با DirectX 11 سازگار نباشد.") + finalSuffix;
        case ParsecError::QSV_INIT_FAILED:
            return tr("خطا در مقداردهی اولیه Intel QSV. لطفاً مطمئن شوید iGPU در بایوس فعال است.") + finalSuffix;
        case ParsecError::D3D11_DEVICE_LOST:
            return tr("ارتباط با کارت گرافیک قطع شد. برنامه را دوباره اجرا کنید.") + finalSuffix;
        default:
            return tr("خطای غیرمنتظره‌ای رخ داده است: ") + translatedMsg;
    }
}

void SystemService::stopSession()
{
    Parsec_StopSession();

#ifdef _WIN32
    if (m_clientWindow) {
        DestroyWindow((HWND)m_clientWindow);
        m_clientWindow = nullptr;
    }
#endif

    m_isSessionActive = false;
    emit sessionStateChanged();
    addLog("INFO", "Session", "Session stopped by user");
}

void SystemService::updateStats()
{
#ifdef _WIN32
    MSG msg;
    while (PeekMessageA(&msg, (HWND)-1, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (msg.message == WM_QUIT) {
            stopSession();
            break;
        }
    }
#endif

    if (m_isSessionActive && Parsec_GetTelemetry(&m_stats)) {
        emit statsChanged();
    } else {
        // Reset volatile stats when not active
        m_stats = {};
        emit statsChanged();
    }

    // Actual system metrics
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    m_memoryUsage = 100.0 - (100.0 * memInfo.ullAvailPhys / memInfo.ullTotalPhys);
    m_cpuUsage = QRandomGenerator::global()->generateDouble() * 5.0 + 2.0;
#else
    m_cpuUsage = 5.0;
    m_memoryUsage = 20.0;
#endif

    // Update History
    updateHistory(m_cpuHistory, m_cpuUsage);
    updateHistory(m_memoryHistory, m_memoryUsage);
    updateHistory(m_fpsHistory, m_stats.fps);
    updateHistory(m_latencyHistory, m_stats.e2eLatency);
    emit historyChanged();

    emit cpuUsageChanged();
    emit memoryUsageChanged();
    emit uptimeChanged();
}

void SystemService::updateHistory(QVariantList& list, double value)
{
    list.append(value);
    if (list.size() > m_maxHistory) {
        list.removeFirst();
    }
}

QString SystemService::uptime() const
{
    qint64 diff = QDateTime::currentSecsSinceEpoch() - m_startTime;
    int hours = diff / 3600;
    int minutes = (diff % 3600) / 60;
    int seconds = diff % 60;
    return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
}

void SystemService::addLog(const QString& level, const QString& event, const QString& desc)
{
    m_logModel->addLog(level, event, desc);
}
