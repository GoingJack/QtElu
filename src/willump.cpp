#include "willump.h"

#include <QTimer>
#include <QtWebSockets>

Willump::Willump(QObject* parent) : QObject(parent) {}

Willump::~Willump() {}

void Willump::start(const QString& method,
                    const QString& endpoint,
                    std::function<void(const QByteArray&)> success_do_sth,
                    const QJsonObject& data,
                    int retry_count) {
  QMetaObject::invokeMethod(
      this,
      [=]() {
        QNetworkReply* reply = request(
            method, endpoint,
            QJsonDocument(data).toJson());  // Get user basic information
        connect(reply, &QNetworkReply::finished, this, [=]() {
          if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            if (success_do_sth) {
              QMetaObject::invokeMethod(qApp, [=]() { success_do_sth(data); });
            }
          } else {
            qDebug() << "endpoint:" << endpoint;
            qDebug() << "\nConnected to LCUx https server, but got an invalid "
                        "response for a known URI. Response status code:"
                     << reply
                            ->attribute(
                                QNetworkRequest::HttpStatusCodeAttribute)
                            .toInt()
                     << "\nerror info" << reply->error();
          }

          reply->deleteLater();

          if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "QNetworkReply error: " << method << endpoint << data
                     << "Can't connect to LCUx server. Retrying...";
            if (retry_count > 0) {
              QTimer::singleShot(500, [=]() {
                qInfo() << "endpoint retry" << endpoint << retry_count;
                start(method, endpoint, success_do_sth, data, retry_count - 1);
              });
            }
          }
        });
      },
      Qt::QueuedConnection);
}

bool Willump::initPortAuth(bool is_retry) {
  lcu_pid_ = findLCUProcessId();
  if (lcu_pid_ == "-1") {
    qDebug() << "Couldn't find LCUx process yet. Re-searching process list...";
    if (!is_retry) {
    }
    return false;
  }
  process_args_ = processCommandLine(getProcessCommandLine(lcu_pid_));
  port = process_args_.value("--app-port").toInt();
  authKey = process_args_.value("--remoting-auth-token").toString();
  qDebug() << "port:" << port << "authKey:" << authKey;
  if (rest_session_) {
    rest_session_->deleteLater();
  }
  rest_session_ = new QNetworkAccessManager(this);

  emit onPortAuthReady();
  if (wsClient == nullptr) {
    initWebSocket();
  }

  connectToWebSocket();

  return true;
}

void Willump::initWebSocket() {
  wsClient = new QWebSocket();
  qDebug() << "create websocket!";
  connect(wsClient, &QWebSocket::disconnected, this, [this]() {
    qDebug() << "WebSocket disconnected!!!";
    retry_count_++;
    if (retry_count_ >= 5) {
      emit onMessage(
          "Please check your openssl library dependencies or program running "
          "permissions(administrator). And restart the program.");
      return;
    }
    initPortAuth();
  });
  connect(wsClient, &QWebSocket::connected, this, [=]() {
    retry_count_ = 0;
    wsClient->sendTextMessage(
        "[5,\"OnJsonApiEvent_lol-gameflow_v1_session\"]");  // Subscribe to
                                                            // match status
  });
  connect(wsClient, &QWebSocket::textMessageReceived, this,
          &Willump::handleDealWebSocketReceived);
  connect(wsClient,
          QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors), this,
          [](const QList<QSslError>& errors) { qDebug() << errors; });
}

void Willump::connectToWebSocket() {
  QString authValue =
      "Basic " +
      QByteArray("riot").append(":").append(authKey.toUtf8()).toBase64();
  QSslConfiguration config = QSslConfiguration::defaultConfiguration();
  config.setProtocol(QSsl::AnyProtocol);
  config.setPeerVerifyMode(QSslSocket::VerifyNone);
  wsClient->setSslConfiguration(config);

  QNetworkRequest request(QString("wss://127.0.0.1:%1").arg(port));
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("Authorization", authValue.toUtf8());
  qDebug() << "authValue:" << authValue;

  wsClient->open(request);
}

QNetworkReply* Willump::request(const QString& method,
                                const QString& endpoint,
                                const QByteArray& data) {
  QNetworkRequest request;
  QSslConfiguration config = QSslConfiguration::defaultConfiguration();
  config.setProtocol(QSsl::AnyProtocol);
  config.setPeerVerifyMode(QSslSocket::VerifyNone);
  request.setSslConfiguration(config);

  QUrl url(QString("https://127.0.0.1:%1%2").arg(port).arg(endpoint));
  request.setUrl(url);
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("Accept", "application/json");
  QByteArray auth =
      QByteArray("Basic ") +
      QByteArray("riot").append(":").append(authKey.toUtf8()).toBase64();
  request.setRawHeader("Authorization", auth);

  QNetworkReply* reply = nullptr;
  if (method.toLower() == "get") {
    reply = rest_session_->get(request);
  } else if (method.toLower() == "post") {
    reply = rest_session_->post(request, data);
  } else if (method.toLower() == "put") {
    reply = rest_session_->put(request, data);
  } else if (method.toLower() == "patch") {
    reply = rest_session_->sendCustomRequest(request, "PATCH", data);
  } else if (method.toLower() == "delete") {
    reply = rest_session_->deleteResource(request);
  } else {
    qDebug() << "Invalid HTTP method:" << method;
  }
  return reply;
}

void Willump::handleDealWebSocketReceived(const QString& text) {
  auto list = convertStringToVariantMap(text.toUtf8()).toList();

  if (list.size() > 2) {
    if (list[1].toString() == "OnJsonApiEvent_lol-gameflow_v1_session") {
      auto received_map = list[2].toMap();
      auto phase = list[2].toMap()["data"].toMap()["phase"].toString();
      qDebug() << "current phase: " << phase;
      emit onMessage("current phase: " + phase);
      if (phase == "ReadyCheck") {
        qDebug() << " start check game!!!";
        start("post", "/lol-matchmaking/v1/ready-check/accept",
              [](const QByteArray& data) {});
      }
    }
  }
}

QString Willump::findLCUProcessId() {
  QProcess* process = new QProcess();
  process->start("tasklist");
  process->waitForFinished();
  QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
  QStringList processList =
      output.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  foreach (const QString& processInfo, processList) {
    QStringList parts = processInfo.trimmed().split(QRegExp("\\s+"));
    QString processName = parts[0];
    if (processName == "LeagueClientUx.exe" ||
        processName == "LeagueClientUx") {
      qDebug() << "get lol process pid: " << parts[1];
      return parts[1];
    }
  }

  delete process;
  return "-1";
}

QString Willump::getProcessCommandLine(const QString& pid) {
  QProcess process;
  process.start("wmic", QStringList()
                            << "process"
                            << "where" << QString("ProcessId=%1").arg(pid)
                            << "get"
                            << "CommandLine"
                            << "/format:list");
  process.waitForFinished();
  QString output = process.readAllStandardOutput();
  return output;
}

QVariantMap Willump::processCommandLine(const QString& command_line) {
  QString cmd = command_line;
  cmd.remove('\r');
  cmd.remove('\n');
  cmd.remove('\"');
  QStringList cmdline_args = cmd.split(" ");
  QVariantMap result;

  for (const QString& cmdline_arg : cmdline_args) {
    int equalIndex = cmdline_arg.indexOf('=');
    if (equalIndex != -1) {
      result[cmdline_arg.mid(0, equalIndex).trimmed()] =
          cmdline_arg.mid(equalIndex + 1).trimmed();
    } else {
      result[cmdline_arg] = "";
    }
  }
  return result;
}

QVariant Willump::convertStringToVariantMap(const QByteArray& json_string) {
  QJsonDocument jsonDocument = QJsonDocument::fromJson(json_string);
  if (jsonDocument.isArray()) {
    QJsonArray jsonArray = jsonDocument.array();
    return QVariant(jsonArray);
  } else if (jsonDocument.isObject()) {
    QJsonObject jsonObject = jsonDocument.object();
    return QVariant(jsonObject);
  } else {
    return QVariant::fromValue(json_string);
  }
}

void Willump::cleanup() {
    if (wsClient) {
        wsClient->close();
        wsClient->deleteLater();
        wsClient = nullptr;
    }
    
    if (rest_session_) {
        rest_session_->deleteLater();
        rest_session_ = nullptr;
    }
}

Willump& GetWillumpService() {
  return Willump::getInstance();
}
