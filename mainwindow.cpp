#include "mainwindow.h"
#include "ui_mainwindow.h"

std::vector<int> unicast_check;
int broadcast_check;

//void * btn_check_thread(void * i){

//    if(broadcast_check == 1){
//        for(auto it = unicast_btn_list.begin(); it != unicast_btn_list.end(); it++)
//        {
//            (*it)->setEnabled(0);
//        }
//    }else{
//        for(auto it = unicast_btn_list.begin(); it != unicast_btn_list.end(); it++)
//        {
//            (*it)->setEnabled(1);
//        }
//    }

//    if(unicast_check.empty()){
//        btn_attack->setEnabled(1);
//    }else {
//        btn_attack->setEnabled(0);
//    }
//}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    int ret = 0;
    timer = new QTimer;
    timer->start();
    timer->setInterval(1000);

    system("su -c \"/data/local/tmp/netkillerd&\"");

    // Check if server is running
    char tmp_buf[1024] = {0,};
    FILE *fp = popen("su -c \"ps | grep netkillerd\"", "rt");
    while(fgets(tmp_buf, 1024,fp)){
        if(strstr(tmp_buf, "netkillerd") != NULL){
            ret = 1;
            break;
        }
    }

    if(ret == 0){
        char msg[20] = {0,};
        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Error");
        MsgBox.setText("Server is not running");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            exit(1);
        }
    }

    /* Get basic informations */
    /* 1. interface name */
    char iface_name[20] = { 0, };
    get_iface_name(iface_name);

    /* 2. My MAC */
    QString str_my_mac;
    uint8_t my_mac[6] = { 0, };
    get_my_mac(my_mac, iface_name);
    str_my_mac.sprintf("%02X:%02X:%02X:%02X:%02X:%02X", my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);

    /* 3. gateway IP */
    char str_gw_ip[30] = { 0, };
    uint32_t gw_ip;
    get_gw_ip(str_gw_ip);
    gw_ip = inet_addr(str_gw_ip);

    /* 4. My IP */
    char str_my_ip[30] = { 0, };
    uint32_t my_ip;
    get_my_ip(str_my_ip);
    my_ip = inet_addr(str_my_ip);

    QStringList tableHeader;
    tableHeader << "Interface" << "GW IP" << "Broadcast Attack";
    ui->gwTable->setColumnCount(3);
    ui->gwTable->setHorizontalHeaderLabels(tableHeader);
    ui->gwTable->horizontalHeader()->setStretchLastSection(true);

    ui->gwTable->setColumnWidth(1, 400);
    //    ui->gwTable->setRowHeight(0, 150);

    ui->gwTable->insertRow(ui->gwTable->rowCount()); // Row를 추가합니다.
    ui->gwTable->setItem(0, 0, new QTableWidgetItem(iface_name));
    ui->gwTable->setItem(0, 1, new QTableWidgetItem(str_gw_ip));

    btn_attack = new QPushButton();
    btn_attack->setText("Attack");
    QObject::connect(btn_attack, &QPushButton::clicked, this, &MainWindow::braodAttack);
    ui->gwTable->setCellWidget(0, 2,(QWidget*)btn_attack);

    QStringList tableHeader2;
    tableHeader2 << "MAC" << "IP" << "Attack";
    ui->devTable->setColumnCount(3);
    ui->devTable->setHorizontalHeaderLabels(tableHeader2); // Table Header 설정
    ui->devTable->horizontalHeader()->setStretchLastSection(true);
    ui->devTable->setColumnWidth(0, 400);
    ui->devTable->setColumnWidth(1, 400);


    int server_port = 1234;
    if(!connect_sock(&client_sock, server_port)){

    }

    thread = new Thread(client_sock);
    QObject::connect(thread, &Thread::captured, this, &MainWindow::processCaptured);

    thread->start();

    /* Send basic informations to Netkiller daemon */
    memset(buf, 0x00, BUF_SIZE);
    memcpy(buf, "1", 1);
    send_data(client_sock, buf);

    /* Scan devices */
    memset(buf, 0x00, BUF_SIZE);
    memcpy(buf, "3", 1);
    send_data(client_sock, buf);
}

MainWindow::~MainWindow()
{
    system("su -c \"killall -9 netkillerd\"");
    delete ui;
}


void MainWindow::braodAttack(){
    QPushButton * pb = (QPushButton *)sender();
    char sdata[BUF_SIZE];
    memset(sdata, 0x00, BUF_SIZE);

    if (pb->text() == "Attack"){
        broadcast_check = true;
        pb->setText("Stop");

        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Attack Start");
        MsgBox.setText("broadcast attack started!");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            memcpy(sdata, "2", 1);
            send_data(client_sock, sdata);
        }
    } else {
        pb->setText("Attack");
        broadcast_check = false;

        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Attack Stop");
        MsgBox.setText("broadcast attack stop.");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            memcpy(sdata, "5", 1);
            send_data(client_sock, sdata);
        }
    }
}

void MainWindow::unicastAttack(){
    char sdata[BUF_SIZE];
    QPushButton *pb = qobject_cast<QPushButton *>(QObject::sender());
    int index = pb->property("my_key").toInt();

    memset(sdata, 0x00, BUF_SIZE);
    if (pb->text() == "Attack"){
        unicast_check.push_back(1);
        pb->setText("Stop");

        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Unicast attack start");
        MsgBox.setText(ui->devTable->item(index, 0)->text());
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            sdata[0] = '4';
            sdata[1] = '\t';
            strcat(sdata, ui->devTable->item(index, 0)->text().toStdString().c_str());
            sdata[strlen(sdata)] = '\t';
            strcat(sdata, ui->devTable->item(index, 1)->text().toStdString().c_str());
            sdata[strlen(sdata)] = '\t';
            send_data(client_sock, sdata);
        }
    } else {
        pb->setText("Attack");
        unicast_check.pop_back();

        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Unicast attack stop");
        MsgBox.setText(ui->devTable->item(index, 0)->text());
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            sdata[0] = '6';
            sdata[1] ='\t';
            strcat(sdata, ui->devTable->item(index, 0)->text().toStdString().c_str());
            sdata[strlen(sdata)] = '\t';
            send_data(client_sock, sdata);
        }
    }

}

void MainWindow::processCaptured(char* data)
{
    QString temp = QString(data);
    QStringList info = temp.split("\t");

    if(info[0] == '3'){
        ui->devTable->insertRow(ui->devTable->rowCount());
        int index = ui->devTable->rowCount() - 1;

        ui->devTable->setItem(index, 0, new QTableWidgetItem(info[1]));
        ui->devTable->setItem(index, 1, new QTableWidgetItem(info[2]));
        QPushButton* btn_attack = new QPushButton();

        btn_attack->setText("Attack");
        btn_attack->setProperty("my_key", index);
        unicast_btn_list.append(btn_attack);

        ui->devTable->setCellWidget(index, 2, (QWidget*)btn_attack);
        QObject::connect(btn_attack, &QPushButton::clicked, this, &MainWindow::unicastAttack);
    }


}
