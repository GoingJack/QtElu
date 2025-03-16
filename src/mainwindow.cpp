#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "willump.h"
#include <QTextBrowser>
#include <QVBoxLayout>

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

  connect(&GetWillumpService(), &Willump::onMessage, this,
          &MainWindow::handleMessage);


  // init ui
  QVBoxLayout *layout = new QVBoxLayout(ui->centralwidget);

  show_msg_ = new QTextBrowser(ui->centralwidget);

  layout->addWidget(show_msg_);
  show_msg_->setReadOnly(true);
}

MainWindow::~MainWindow() {
  delete ui;

  lcu_service_thread_.quit();
  lcu_service_thread_.wait();
}

void MainWindow::handleMessage(QString msg)
{
    show_msg_->append(msg);
}
