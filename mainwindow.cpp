/*
This software is part of qtcsdr.

Copyright (c) 2015, Andras Retzler <randras@sdr.hu>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ANDRAS RETZLER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QProcess>
#include <QStringList>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define CMD_IQSERVER "pgroup -9 rtl_tcp -a 127.0.0.1 -s %SAMP_RATE% -p 4950 -f 89500000 -g 0"
#define CMD_DISTRIB "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4950; sleep .3; done) | nmux -p 4951 -a 127.0.0.1 -b %NMUX_BUFSIZE% -n %NMUX_BUFCNT%\""
#define CMD_MOD_NOP "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %DECIM% 0.005 HAMMING > /dev/null"
#define CMD_MOD_WFM "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %WFM_DECIM% 0.05 HAMMING  | csdr fmdemod_quadri_cf | csdr fractional_decimator_ff 5 | csdr deemphasis_wfm_ff 48000 50e-6 | csdr convert_f_i16 |  %AUDIOPLAYER%\""
#define CMD_MOD_NFM "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %DECIM% 0.005 HAMMING | csdr fmdemod_quadri_cf | csdr limit_ff | csdr deemphasis_nfm_ff 48000 | csdr fastagc_ff | csdr convert_f_i16 |       %AUDIOPLAYER%\""
#define CMD_MOD_AM  "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %DECIM% 0.005 HAMMING | csdr amdemod_cf | csdr fastdcblock_ff | csdr agc_ff | csdr limit_ff | csdr convert_f_i16 |                           %AUDIOPLAYER%\""
#define CMD_MOD_USB "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %DECIM% 0.005 HAMMING | csdr bandpass_fir_fft_cc 0 0.1 0.05 | csdr realpart_cf | csdr agc_ff | csdr limit_ff | csdr convert_f_i16 |          %AUDIOPLAYER%\""
#define CMD_MOD_LSB "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr shift_addition_cc --fifo %FIFO% | csdr fir_decimate_cc %DECIM% 0.005 HAMMING | csdr bandpass_fir_fft_cc -0.1 0 0.05 | csdr realpart_cf | csdr agc_ff | csdr limit_ff | csdr convert_f_i16 |         %AUDIOPLAYER%\""
#define CMD_FFT     "pgroup -9 bash -c \"(for anything in {0..10}; do ncat 127.0.0.1 4951; sleep .3; done) | csdr convert_u8_f | csdr fft_cc 2048 %FFT_READ_SIZE% | csdr logpower_cf -70 | csdr fft_exchange_sides_ff 2048\""

#define CMD_ARECORD "arecord %ADEVICE% -f S16_LE -r 48000 -c 1"

#define CMD_TX_WFM  "pgroup bash -c \"%ARECORD% | csdr convert_i16_f | csdr gain_ff 70000 | csdr convert_f_samplerf 20833 | (gksu touch; sudo rpitx -i- -m RF -f %TXFREQ%)\""
#define CMD_TX_NFM  "pgroup bash -c \"%ARECORD% | csdr convert_i16_f | csdr gain_ff 7000 | csdr convert_f_samplerf 20833 | (gksu touch; sudo rpitx -i- -m RF -f %TXFREQ%)\""
#define CMD_TX_AM   "pgroup bash -c \"%ARECORD% | csdr convert_i16_f | csdr dsb_fc | csdr add_dcoffset_cc | (gksu touch; sudo rpitx -i- -m IQFLOAT -f %TXFREQ_AM%)\""
#define CMD_TX_USB  "pgroup bash -c \"%ARECORD% | csdr convert_i16_f | csdr dsb_fc | csdr bandpass_fir_fft_cc 0 0.1 0.01 | csdr gain_ff 2 | csdr shift_addition_cc 0.2 | (gksu touch; sudo rpitx -i- -m IQFLOAT -f %TXFREQ_SSB%)\""
#define CMD_TX_LSB  "pgroup bash -c \"%ARECORD% | csdr convert_i16_f | csdr dsb_fc | csdr bandpass_fir_fft_cc -0.1 0 0.01 | csdr gain_ff 2 | csdr shift_addition_cc 0.2 | (gksu touch; sudo rpitx -i- -m IQFLOAT -f %TXFREQ_SSB%)\""

#define NMUX_MEMORY_MBYTE 50

//#define CMD_WFM "pgroup -9 bash -c \"rtl_tcp -s 2400000 -p 4951 -f 89500000 & (sleep 1; nc localhost 4951 | csdr convert_u8_f | csdr shift_addition_cc -0.085 | csdr fir_decimate_cc 10 0.05 HAMMING | csdr fmdemod_quadri_cf | csdr fractional_decimator_ff 5 | csdr deemphasis_wfm_ff 48000 50e-6 | csdr convert_f_i16 | mplayer -cache 768 -quiet -rawaudio samplesize=2:channels=1:rate=48000 -demuxer rawaudio -)\""


QString MainWindow::getNextArgAfter(QString what)
{
    if(QCoreApplication::arguments().contains(what))
    {
        int indexOfWhat = QCoreApplication::arguments().indexOf(what);
        if(QCoreApplication::arguments().count()>indexOfWhat+1)
        {
            if(!QCoreApplication::arguments().at(indexOfWhat+1).startsWith("--"))
            {
                return QCoreApplication::arguments().at(indexOfWhat+1);
            }
        }
    }
    return "";
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), qStdOut(stdout)
{
    ui->setupUi(this);

    QString nextArg;

    if(QCoreApplication::arguments().contains("--mplayer"))
    {
         audioPlayerCommand="mplayer -cache 1024 -quiet -rawaudio samplesize=2:channels=1:rate=48000 %ADEVICE% -demuxer rawaudio -";
         if(!(alsaDevice=getNextArgAfter("--mplayer")).isEmpty())
         {
             audioPlayerCommand=audioPlayerCommand.replace("%ADEVICE%",QString("-ao alsa:device=")+alsaDevice.replace(",",".").replace(":","="));
         }
         else
             audioPlayerCommand=audioPlayerCommand.replace("%ADEVICE%","");
    }
    else
    {
        audioPlayerCommand  = "csdr mono2stereo_i16 | aplay -f S16_LE -r48000 -c2 -D ";
        if (!(alsaDevice=getNextArgAfter("--alsa")).isEmpty())
            audioPlayerCommand+=alsaDevice;
        else
            audioPlayerCommand+="default";
    }
    qDebug() << audioPlayerCommand;

    connect(&tmrRead, SIGNAL(timeout()), this, SLOT(tmrRead_timeout()));
    connect(ui->widgetFFT, SIGNAL(shiftChanged(int)), this, SLOT(on_shiftChanged(int)));
    tmrRead.start(10);
}

void MainWindow::changeDemodulator(QString mode)
{
    //we select the new demodulator
    if(ui->toggleRun->isChecked())
    {
        if(procDemod.processId()) kill(procDemod.processId(), SIGTERM);
        procDemod.waitForFinished(1000);
        procDemod.start("/bin/sh", QStringList() << "-c" << getDemodulatorCommand(mode));
        //procDemod.start(getDemodulatorCommand(mode));
        procDemod.waitForStarted(1000);
        setShift();
    }

    updateFilterBw();
}

void MainWindow::redirectProcessOutput(QProcess &proc, bool onlyStdErr)
{
    if(proc.processId()!=0)
    {
        QString temp = ((onlyStdErr)?"":proc.readAllStandardOutput()) + proc.readAllStandardError();
        if(temp.length()) qStdOut << temp;
    }
    qStdOut.flush();
}

void MainWindow::tmrRead_timeout()
{
    redirectProcessOutput(procDemod);
    redirectProcessOutput(procDistrib);
    redirectProcessOutput(procIQServer);
    redirectProcessOutput(procFFT, true);
    redirectProcessOutput(procTX);

    if(procFFT.processId()!=0)
    {
        FFTDataBuffer += procFFT.readAll();
        while(ui->widgetFFT->takeOneWaterfallLine(&FFTDataBuffer));
    }

}

MainWindow::~MainWindow()
{
    if(ui->toggleRun->isChecked()) on_toggleRun_toggled(false); //so that we kill all subprocesses
    delete ui;
}
/*
void MainWindow::on_toggleWFM_toggled(bool checked) { untoggleOtherModButtonsThan(ui->toggleWFM); }
void MainWindow::on_toggleNFM_toggled(bool checked) { untoggleOtherModButtonsThan(ui->toggleNFM); }
void MainWindow::on_toggleAM_toggled(bool checked)  { untoggleOtherModButtonsThan(ui->toggleAM); }
void MainWindow::on_toggleUSB_toggled(bool checked) { untoggleOtherModButtonsThan(ui->toggleUSB); }
void MainWindow::on_toggleLSB_toggled(bool checked) { untoggleOtherModButtonsThan(ui->toggleLSB); }
*/
void MainWindow::on_shiftChanged(int newOffset)
{
    ui->spinOffset->setValue(newOffset);
}

QString MainWindow::getDemodulatorCommand(QString mode)
{
    QString myDemodCmd = "";
    if(mode == "NOOP") myDemodCmd=CMD_MOD_NOP;
    if(mode == "WFM")  myDemodCmd=CMD_MOD_WFM;
    if(mode == "NFM")  myDemodCmd=CMD_MOD_NFM;
    if(mode == "AM")   myDemodCmd=CMD_MOD_AM;
    if(mode == "LSB")  myDemodCmd=CMD_MOD_LSB;
    if(mode == "USB")  myDemodCmd=CMD_MOD_USB;
    myDemodCmd=myDemodCmd
            .replace("%FIFO%", fifoPipePath)
            .replace("%AUDIOPLAYER%", audioPlayerCommand)
            .replace("%SAMP_RATE%",ui->comboSampRate->currentText())
            .replace("%DECIM%", QString::number(ui->comboSampRate->currentText().toInt()/48000))
            .replace("%WFM_DECIM%", QString::number(ui->comboSampRate->currentText().toInt()/240000));
    qDebug() << "myDemodCmd ="<<myDemodCmd;
    return myDemodCmd;
}


void MainWindow::updateFilterBw()
{
    ui->widgetFFT->offsetFreq = ui->spinOffset->value();
    //QString mode = ui->comboDemodMode->currentData().toString();
    QString mode = ui->comboDemodMode->currentText();
    if(mode == "WFM") { ui->widgetFFT->filterLowCut=-70000; ui->widgetFFT->filterHighCut=70000; }
    if(mode == "NFM") { ui->widgetFFT->filterLowCut=-4000; ui->widgetFFT->filterHighCut=4000; }
    if(mode == "AM")  { ui->widgetFFT->filterLowCut=-4000; ui->widgetFFT->filterHighCut=4000; }
    if(mode == "LSB") { ui->widgetFFT->filterLowCut=-4000; ui->widgetFFT->filterHighCut=0; }
    if(mode == "USB") { ui->widgetFFT->filterLowCut=0; ui->widgetFFT->filterHighCut=4000; }
    if(mode == "NOOP") { ui->widgetFFT->filterLowCut=0; ui->widgetFFT->filterHighCut=0; }
}

void MainWindow::on_toggleRun_toggled(bool checked)
{
    if(checked)
    {
        ui->widgetFFT->sampleRate=ui->comboSampRate->currentText().toInt();
        ui->comboSampRate->setEnabled(false);
        fifoPipePath = QString("/tmp/qtcsdr_shift_pipe_")+QString::number(rand());
        mkfifo(fifoPipePath.toStdString().c_str(), 0600);
        fifoPipe=open(fifoPipePath.toStdString().c_str(), O_RDWR);
        setShift();
        QString IQCommand = QString(CMD_IQSERVER).replace("%SAMP_RATE%",ui->comboSampRate->currentText());
        qDebug() << "IQCommand =" << IQCommand;
        procIQServer.start("/bin/sh", QStringList() << "-c" << IQCommand);
        procIQServer.waitForStarted(1000);
        int nmuxBufsize = 0, nmuxBufcnt = 0, sampRate = ui->comboSampRate->currentText().toInt();
        while (nmuxBufsize < sampRate/4) nmuxBufsize += 4096; //taken from OpenWebRX
        while (nmuxBufsize * nmuxBufcnt < NMUX_MEMORY_MBYTE * 1e6) nmuxBufcnt += 1;
        QString distribCommand = QString(CMD_DISTRIB)
                .replace("%NMUX_BUFSIZE%", QString::number(nmuxBufsize))
                .replace("%NMUX_BUFCNT%", QString::number(nmuxBufcnt));
        qDebug() << "distribCommand =" << distribCommand;
        procDistrib.start("/bin/sh", QStringList() << "-c" << distribCommand);
        procDistrib.waitForStarted(1000);
        procDemod.start("/bin/sh", QStringList() << "-c" << getDemodulatorCommand( ui->comboDemodMode->currentText() ));
        QString FFTCommand = QString(CMD_FFT).replace("%FFT_READ_SIZE%", QString::number(ui->comboSampRate->currentText().toInt()/10));
        qDebug() << "FFTCommand =" << FFTCommand;
        procFFT.start("/bin/sh", QStringList() << "-c" << FFTCommand);
        on_spinFreq_valueChanged(ui->spinFreq->value());
        on_comboDirectSamp_currentIndexChanged(0);
        updateFilterBw();
    }
    else
    {
        ui->comboSampRate->setEnabled(true);
        unlink(fifoPipePath.toStdString().c_str());
        if(procDemod.processId()!=0)    kill(procDemod.processId(), SIGTERM);
        if(procDistrib.processId()!=0)  kill(procDistrib.processId(), SIGTERM);
        if(procIQServer.processId()!=0) kill(procIQServer.processId(), SIGTERM);
        if(procFFT.processId()!=0)      kill(procFFT.processId(), SIGTERM);
        procFFT.readAll();
        FFTDataBuffer.clear();
    }
}

void MainWindow::setShift()
{
    QString shiftString = QString::number(-ui->spinOffset->value()/(float)ui->comboSampRate->currentText().toInt())+"\n";
    write(fifoPipe,shiftString.toStdString().c_str(),shiftString.length());
    ui->widgetFFT->offsetFreq = ui->spinOffset->value();
}

void MainWindow::sendCommand(unsigned char cmd_num, unsigned value)
{
    //docs: https://github.com/pinkavaj/rtl-sdr/blob/master/src/rtl_tcp.c#L324
    unsigned char cmd[5];
    cmd[0] = cmd_num; //set_freq
    cmd[1] = (value>>24)&0xff;
    cmd[2] = (value>>16)&0xff;
    cmd[3] = (value>>8)&0xff;
    cmd[4] = value&0xff;
    procDistrib.write((char*)cmd, 5);
}


void MainWindow::on_spinFreq_valueChanged(int val)
{
    (void)val;
    ui->spinCenter->setValue(ui->spinFreq->value()-ui->spinOffset->value());
    sendCommand(RTLTCP_SET_FREQ, ui->spinCenter->value());
    //procDemod.write("\x01\x05\x55\xa9\x60");
    //procDemod.write("macska\n");
}

void MainWindow::on_spinOffset_valueChanged(int arg1)
{
    (void)arg1;
    setShift();
    ui->spinFreq->setValue(ui->spinCenter->value()+ui->spinOffset->value());
}

void MainWindow::on_spinCenter_valueChanged(int arg1)
{
    (void)arg1;
    sendCommand(RTLTCP_SET_FREQ, ui->spinCenter->value());
    ui->spinFreq->setValue(ui->spinCenter->value()+ui->spinOffset->value());
}

void MainWindow::on_comboDirectSamp_currentIndexChanged(int index)
{
    (void)index;
    sendCommand(RTLTCP_SET_DIRECT_SAMPLING,ui->comboDirectSamp->currentIndex());
}

void MainWindow::on_comboDemodMode_currentIndexChanged(int index)
{
    // TODO: use index instead of currentText() ?
    (void)index;
    QString mode = ui->comboDemodMode->currentText();
    changeDemodulator(mode);
    updateFilterBw();
    sendCommand(RTLTCP_SET_FREQ, ui->spinCenter->value());
}
