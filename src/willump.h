#ifndef WILLUMP_H
#define WILLUMP_H

#include <QObject>
#include <QJsonObject>

class QWebSocket;
class QNetworkAccessManager;
class QNetworkReply;

class Willump : public QObject {
  Q_OBJECT
 public:
  explicit Willump(QObject* parent = nullptr);
  ~Willump();

  static Willump& getInstance() {
    static Willump wipeService;
    return wipeService;
  }

  // init Elu function
  void start(const QString& method,
             const QString& endpoint,
             std::function<void(const QByteArray& all_data)> success_do_sth,
             const QJsonObject& data = {},
             int retry_count = 0);
  Q_INVOKABLE bool initPortAuth(bool is_retry = false);
  void initWebSocket();
  void connectToWebSocket();
  Q_INVOKABLE QNetworkReply* request(const QString& method,
                                     const QString& endpoint,
                                     const QByteArray& data = QByteArray());

 signals:
  void onPortAuthReady();
  void onMessage(QString info);

 public slots:
  void cleanup();

 private slots:
  void handleDealWebSocketReceived(const QString& text);

 private:
  QString findLCUProcessId();
  QString getProcessCommandLine(const QString& pid);
  QVariantMap processCommandLine(const QString& command_line);
  QVariant convertStringToVariantMap(const QByteArray& json_string);

 private:
  QString lcu_pid_ = "";
  QVariantMap process_args_;
  QWebSocket* wsClient = nullptr;
  int port;
  QString authKey;

  QNetworkAccessManager* rest_session_;
};

Willump& GetWillumpService();

#endif  // WILLUMP_H
