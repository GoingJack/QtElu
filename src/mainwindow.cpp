#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "willump.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  lcu_service_thread_.setObjectName("NetWorkThread");
  lcu_service_thread_.start();
  GetWillumpService().moveToThread(&lcu_service_thread_);
  qDebug() << "main thread:" << this->thread()
           << "network thread:" << &lcu_service_thread_;
  QMetaObject::invokeMethod(&GetWillumpService(), "initPortAuth",
                            Qt::QueuedConnection, Q_ARG(bool, false));
}

MainWindow::~MainWindow() {
  delete ui;

  lcu_service_thread_.quit();
  lcu_service_thread_.wait();
}
