#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTextBrowser;
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  void handleMessage(QString msg);

 private:
  Ui::MainWindow* ui;
  QThread lcu_service_thread_;

  QTextBrowser *show_msg_ = nullptr;
};
#endif  // MAINWINDOW_H
