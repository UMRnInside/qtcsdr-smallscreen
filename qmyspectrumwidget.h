#ifndef QMYSPECTRUMWIDGET_H
#define QMYSPECTRUMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QImage>
#include <QPointer>

class QMySpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QMySpectrumWidget(QWidget *parent = 0);
    QImage* spectrumImage;
    int FFTSize = 2048;
    bool takeOneWaterfallLine(QByteArray* from);
private:
    void reinit();
    virtual void paintEvent(QPaintEvent*);
    virtual void resizeEvent(QResizeEvent * event);
    void shiftImageOneLineDown();

signals:

public slots:

};

#endif // QMYSPECTRUMWIDGET_H
