#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("himashinya");

    auto *label = new QLabel("Hello, himashinya", &window);
    label->setAlignment(Qt::AlignCenter);

    window.setCentralWidget(label);
    window.resize(1100, 700);
    window.show();

    return app.exec();
}