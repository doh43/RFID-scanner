#include <QApplication>
#include "./include/adminwindow.h" // change later
#include "../include/rfidwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    RfidWindow scan_window;
    scan_window.show();

    return app.exec();
}
