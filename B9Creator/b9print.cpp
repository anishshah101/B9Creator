#include <QMessageBox>
#include <QTimer>
#include "b9print.h"
#include "ui_b9print.h"

B9Print::B9Print(B9Terminal *pTerm, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::B9Print)
{
    m_pTerminal = pTerm;
    if(m_pTerminal == NULL) qFatal("FATAL Call to B9Creator with null B9Terminal Pointer");
    ui->setupUi(this);

    // Clear old messages
    ui->lineEditSerialStatus->setText("");
    ui->lineEditProjectorOutput->setText("");

    m_iTbase = m_iTover = 0;
    m_iXOff = m_iYOff =0;
    m_iPrintState = PRINT_NO;
    m_iPaused = PAUSE_NO;
    m_bAbort = false;
    m_sAbortMessage = "Unknown Abort";
    m_iCurLayerNumber = 0;
    m_dLayerThickness = 0.0;
    m_iLastLayer = 0;

    connect(m_pTerminal, SIGNAL(updateConnectionStatus(QString)), this, SLOT(on_updateConnectionStatus(QString)));
    connect(m_pTerminal, SIGNAL(updateProjectorOutput(QString)), this, SLOT(on_updateProjectorOutput(QString)));
    connect(m_pTerminal, SIGNAL(updateProjectorStatus(QString)), this, SLOT(on_updateProjectorStatus(QString)));
    connect(m_pTerminal, SIGNAL(updateProjector(B9PrinterStatus::ProjectorStatus)), this, SLOT(on_updateProjector(B9PrinterStatus::ProjectorStatus)));
    connect(m_pTerminal, SIGNAL(signalAbortPrint(QString)), this, SLOT(on_pushButtonAbort_clicked(QString)));
    connect(m_pTerminal, SIGNAL(PrintReleaseCycleFinished()), this, SLOT(exposeTBaseLayer()));
    connect(m_pTerminal, SIGNAL(pausePrint()), this, SLOT(on_pushButtonPauseResume_clicked()));
    connect(m_pTerminal, SIGNAL(sendStatusMsg(QString)),this, SLOT(setProjMessage(QString)));

    QString sTime = QDateTime::currentDateTime().toString("hh:mm");
    ui->lcdNumberTime->setDigitCount(9);
    ui->lcdNumberTime->display(sTime);
}

B9Print::~B9Print()
{
    delete ui;
}

void B9Print::keyPressEvent(QKeyEvent * pEvent)
{
    // Having this function absorbs the ESC key!
    QDialog::keyReleaseEvent(pEvent);
}

void B9Print::hideEvent(QHideEvent *event)
{
    emit eventHiding();
    event->accept();
}

void B9Print::closeEvent ( QCloseEvent * event )
{
    event->ignore();
    on_pushButtonAbort_clicked();
}

void B9Print::showHelp()
{
    m_HelpSystem.showHelpFile("openfile.html");
}

void B9Print::on_updateConnectionStatus(QString sText)
{
    ui->lineEditSerialStatus->setText(sText);
}

void B9Print::on_updateProjectorOutput(QString sText)
{
    ui->lineEditProjectorOutput->setText(sText);
}

void B9Print::on_updateProjectorStatus(QString sText)
{
    ui->lineEditProjectorStatus->setText(sText);
}

void B9Print::setProjMessage(QString sText)
{
    m_pTerminal->rcSetProjMessage(sText);
}

QString B9Print::updateTimes()
{
    QTime vTimeFinished, vTimeRemains, t;
    int iTime = m_pTerminal->getEstCompleteTimeMS(m_iCurLayerNumber,m_iLastLayer,m_pCPJ->getZLayermm(),m_iTbase+m_iTover);
    int iM = iTime/60000;
    int iH = iM/60;
    iM = (int)((double)iM+0.5) - iH*60;
    QString sLZ = ":0"; if(iM>9)sLZ = ":";
    QString sTimeRemaining = QString::number(iH)+sLZ+QString::number(iM);
    t.setHMS(0,0,0); vTimeRemains = t.addMSecs(iTime);
    vTimeFinished = QTime::currentTime().addMSecs(iTime);
    ui->lcdNumberTime->display(vTimeFinished.toString("hh:mm AP"));
    ui->lcdNumberTimeRemaining->display(sTimeRemaining);
    return "Estimated time remaining: "+sTimeRemaining+"  Estimated Completion Time: "+vTimeFinished.toString("hh:mm AP");
}

double B9Print::curLayerIndexMM()
{
    // layer "0" has zero thickness
    return m_iCurLayerNumber * m_dLayerThickness;
}

void B9Print::on_signalAbortPrint()
{
    if(m_iPrintState!=PRINT_ABORT) return;
    m_iPrintState=PRINT_NO;

    // Handle Abort Signals Here
    if(m_sAbortMessage.contains("Jammed Mechanism"))
        m_pTerminal->rcProjectorPwr(false); // Don't try to release if possibly jammed!
    else
        m_pTerminal->rcFinishPrint(5); //Finish at current z position + 5 mm, turn Projector Off

    m_pTerminal->onScreenCountChanged(); // toggles off the screen if need for primary monitor setups
    hide();
    m_pTerminal->setUsePrimaryMonitor(false);
    m_pTerminal->setPrintPreview(false);
    m_pTerminal->onScreenCountChanged();
    m_pTerminal->setEnabled(true);
    QMessageBox::information(0,"Printing Aborted!","PRINT ABORTED\n\n"+m_sAbortMessage);
}

//////////////////////////////////////////////////////////////////////////////////////////
void B9Print::print3D(CrushedPrintJob* pCPJ, int iXOff, int iYOff, int iTbase, int iTover, int iTattach, int iLastLayer, bool bPrintPreview, bool bUsePrimaryMonitor)
{
    // Note if, iLastLayer < 1, print ALL layers.
    // if bPrintPreview, run without turning on the projector

    m_iMinimumTintMS = m_vSettings.value("m_iMinimumTintMS",50).toInt(); // Grab the old value
    if(m_iMinimumTintMS>500)
        m_iMinimumTintMS=555; // Should never get this big, fix it.
    else if (m_iMinimumTintMS<50)
        m_iMinimumTintMS=56;  // Or this little
    m_vSettings.setValue("m_iMinimumTintMS",(int)((double)m_iMinimumTintMS*.9)); //Set it back to 90% of it last value, just to keep it close to the edge

    m_iPrintState = PRINT_NO;
    m_pTerminal->setEnabled(false);
    m_pCPJ = pCPJ;
    m_pTerminal->createNormalizedMask(m_pCPJ->getXYPixelmm());
    m_iTbase = iTbase; m_iTover = iTover; m_iTattach = iTattach;
    m_iXOff = iXOff; m_iYOff = iYOff;
    m_iCurLayerNumber = 0;
    m_iPaused = PAUSE_NO;
    m_bAbort = false;
    m_iLastLayer = iLastLayer;
    if(m_iLastLayer<1)m_iLastLayer = m_pCPJ->getTotalLayers();

    m_pTerminal->setUsePrimaryMonitor(bUsePrimaryMonitor);
    m_pTerminal->setPrintPreview(bPrintPreview);
    m_pTerminal->onScreenCountChanged();

    ui->progressBarPrintProgress->setMinimum(0);
    ui->progressBarPrintProgress->setMaximum(m_iLastLayer);
    ui->progressBarPrintProgress->setValue(0);
    ui->lineEditLayerCount->setText("Total Layers To Print: "+QString::number(m_iLastLayer)+"  Powering up the projector.");

    ui->lcdNumberTime->display(m_pTerminal->getEstCompleteTime(m_iCurLayerNumber,m_iLastLayer,m_pCPJ->getZLayermm(),m_iTbase+m_iTover).toString("hh:mm"));
    QString sTimeUpdate = updateTimes();
    setProjMessage("Total Layers to print: "+QString::number(m_iLastLayer)+"  "+sTimeUpdate);

    if(!bPrintPreview){
        // Turn on the projector and set the warm up time in ms
        ui->pushButtonPauseResume->setEnabled(false);
        ui->pushButtonAbort->setEnabled(false);
        m_pTerminal->rcSetWarmUpDelay(20000);
        m_pTerminal->rcProjectorPwr(true);
    }
    else {
        ui->lineEditProjectorStatus->setText("OFF:  'Print Preview' Mode");
        ui->pushButtonPauseResume->setEnabled(true);
        ui->pushButtonAbort->setEnabled(true);
        m_iPrintState = PRINT_RELEASING;
        m_dLayerThickness = m_pCPJ->getZLayer().toDouble();
        m_pTerminal->rcBasePrint(curLayerIndexMM());
    }
}
void B9Print::on_updateProjector(B9PrinterStatus::ProjectorStatus eStatus)
{
    if(m_iPrintState==PRINT_NO && eStatus==B9PrinterStatus::PS_ON){
        // Projector is warmed up and on!
        ui->pushButtonPauseResume->setEnabled(true); // Enable pause/resume & abort now
        ui->pushButtonAbort->setEnabled(true);
        m_iPrintState = PRINT_RELEASING;
        m_dLayerThickness = m_pCPJ->getZLayer().toDouble();
        m_pTerminal->rcBasePrint(curLayerIndexMM());
    }
}

void B9Print::on_pushButtonPauseResume_clicked()
{
    if(m_iPrintState == PRINT_NO) return; // not printing yet.

    if(m_iPaused==PAUSE_YES){
        // Time to Resume...
        m_iPaused = PAUSE_NO;
        ui->pushButtonPauseResume->setText("Pause");
        ui->pushButtonAbort->setEnabled(true);
        exposureOfTOverLayersFinished();
    }
    else if(m_iPaused==PAUSE_NO){
        // Time to Pause....
        m_iPaused = PAUSE_WAIT;
        ui->pushButtonPauseResume->setText("Pausing...");
        ui->pushButtonPauseResume->setEnabled(false);
        ui->pushButtonAbort->setEnabled(false);
        setProjMessage("Pausing...");
    }
}

void B9Print::on_pushButtonAbort_clicked(QString sAbortText)
{
    m_sAbortMessage = sAbortText;
    if(m_sAbortMessage.contains("Jammed Mechanism")||
       (m_sAbortMessage.contains("Projector"))){ //&&(m_iPrintState == PRINT_NO&&m_iPaused==PAUSE_NO))){
        // Special cases, always handle it asap.
        m_pTerminal->rcSetCPJ(NULL); //blank
        ui->pushButtonAbort->setText("Abort");
        m_iPrintState = PRINT_ABORT;
        on_signalAbortPrint();
        return;
    }

    if(m_iPrintState == PRINT_NO||m_iPrintState == PRINT_ABORT||m_iPaused==PAUSE_YES||m_iPaused==PAUSE_WAIT) return; // no abort if paused, not printing or already aborting
    ui->pushButtonAbort->setText("Aborting...");
    ui->lineEditLayerCount->setText("Aborting...");
    ui->pushButtonPauseResume->setEnabled(false);
    ui->pushButtonAbort->setEnabled(false);
    setProjMessage("Aborting...");
    m_bAbort = true;
}

void B9Print::setSlice(int iSlice)
{
    if(m_iLastLayer<1)
        m_pTerminal->rcSetCPJ(NULL);
    else {
        m_pCPJ->setCurrentSlice(iSlice);
        m_pTerminal->rcSetCPJ(m_pCPJ);
    }
}

void B9Print::exposeTBaseLayer(){
    //Release & reposition cycle completed, time to expose the new layer
    if(m_iPrintState==PRINT_NO || m_iPrintState == PRINT_DONE || m_iPrintState == PRINT_ABORT)return;
    if(m_bAbort){
        // We're done, release and raise
        m_pTerminal->rcSetCPJ(NULL); //blank
        ui->pushButtonAbort->setText("Abort");
         m_iPrintState = PRINT_ABORT;
        on_signalAbortPrint();
        return;
    }

    //Start Tbase Print exposure
    ui->progressBarPrintProgress->setValue(m_iCurLayerNumber+1);
    ui->lineEditLayerCount->setText("Creating Layer "+QString::number(m_iCurLayerNumber+1)+" of "+QString::number(m_iLastLayer)+",  "+QString::number(100.0*(double)(m_iCurLayerNumber+1)/(double)m_iLastLayer,'f',1)+"% Complete");
    setSlice(m_iCurLayerNumber);
    m_vClock.start(); // image is out there, start the clock running!
    QString sTimeUpdate = updateTimes();
    if(m_iPaused==PAUSE_WAIT){
        ui->lineEditLayerCount->setText("Pausing...");
        setProjMessage("Pausing...");
    }
    else{
        setProjMessage("(Press'p' to pause, 'A' to ABORT)  " + sTimeUpdate+"  Creating Layer "+QString::number(m_iCurLayerNumber+1)+" of "+QString::number(m_iLastLayer));
    }
    m_iPrintState = PRINT_EXPOSING;
    // set timer
    int iAdjExposure = m_pTerminal->getLampAdjustedExposureTime(m_iTbase);
    if(m_iCurLayerNumber==0) iAdjExposure = m_pTerminal->getLampAdjustedExposureTime(m_iTattach);  //Layer 0 has different exposure timing
    if(iAdjExposure>0){
        QTimer::singleShot(iAdjExposure-m_vClock.elapsed(), this, SLOT(startExposeTOverLayers()));
        return;
    }
    else
    {
        startExposeTOverLayers(); // If this is getting called, we're taking too long!
        qDebug() << "EXPOSURE TIMING ERROR:  Tbase exposed for too long!, Tbase is set too small or computer too slow?" << iAdjExposure;
        return;
    }
}

void B9Print::startExposeTOverLayers(){
    if(m_iCurLayerNumber==0){exposureOfTOverLayersFinished(); return;} //Skip this on first layer (0)

    m_vClock.start();  //restart for Tover interval pace

    m_iMinimumTintMS = m_vSettings.value("m_iMinimumTintMS",50).toInt(); // We default to 50ms but adjust it upwards when it gets hit.
    m_iMinimumTintMSWorstCase=m_iMinimumTintMS;

//    qDebug() << " MinTintMS " << m_iMinimumTintMS;

    int iAdjTover = m_pTerminal->getLampAdjustedExposureTime(m_iTover);
    m_iTintNum = (int)((double)iAdjTover/(double)m_iMinimumTintMS);
    if(m_iTintNum > 255) m_iTintNum = 255; // maximum number of time intervals we chop Tover into is 256

    m_dTintMS = (double)iAdjTover/(double)m_iTintNum; // The time of each interval in fractional ms, will always be >= m_iMinimumTintMS
    m_iCurTintIndex = 0;
    exposureOfCurTintLayerFinished();
}

void B9Print::exposureOfCurTintLayerFinished(){
    // Turn off the pixels at the curent point
//    qDebug() <<"INTERVAL COUNT " << m_iCurTintIndex;
    if(m_pTerminal->rcClearTimedPixels((double)m_iCurTintIndex*255.0/(double)m_iTintNum) || m_iCurTintIndex>=m_iTintNum)
    {
        exposureOfTOverLayersFinished();  // We're done with Tover
        m_vSettings.setValue("m_iMinimumTintMS",m_iMinimumTintMSWorstCase);
        return;
    }

    m_iCurTintIndex ++;
    int iAdjustedInt = (int)(m_dTintMS * (double)m_iCurTintIndex)-m_vClock.elapsed();
    if(iAdjustedInt>0){
        QTimer::singleShot(iAdjustedInt, this, SLOT(exposureOfCurTintLayerFinished()));
        return;
    }
    else
    {
        if(m_iCurTintIndex==1)m_iMinimumTintMSWorstCase=m_iMinimumTintMS-iAdjustedInt;
//        qDebug()<<"Adjusting Minimum Tover interval size " << m_iMinimumTintMSWorstCase << "ms";
        exposureOfCurTintLayerFinished(); // If this is getting called, we're taking too long!
        return;
    }
}

void B9Print::exposureOfTOverLayersFinished(){
    if(m_iPrintState==PRINT_NO)return;
    m_pTerminal->rcSetCPJ(NULL); //blank
    //Cycle to next layer or finish
    if(m_iPaused==PAUSE_WAIT){
        m_iPaused=PAUSE_YES;
        m_pTerminal->rcSTOP();
        m_pTerminal->rcCloseVat();
        ui->pushButtonPauseResume->setText("Resume");
        ui->pushButtonPauseResume->setEnabled(true);
        ui->lineEditLayerCount->setText("Paused.  Manual positioning toggle switches are enabled.");
        m_pTerminal->rcSetProjMessage(" Paused.  Manual positioning toggle switches are enabled.  Press 'p' when ready to resume printing.");
        return;
    }

    if(m_bAbort){
        // We're done
        m_pTerminal->rcSetCPJ(NULL); //blank
        ui->pushButtonAbort->setText("Abort");
        m_iPrintState = PRINT_ABORT;
        on_signalAbortPrint();
        return;
    }
    else if(m_iCurLayerNumber==m_iLastLayer-1){
        // We're done, release and raise
        m_iPrintState=PRINT_DONE;
        m_pTerminal->rcFinishPrint(25.4); //Finish at current z position + 25.4 mm, turn Projector Off
        setProjMessage("Finished!");
        ui->lineEditLayerCount->setText("Finished!");
        m_pTerminal->setEnabled(true);
        m_pTerminal->setUsePrimaryMonitor(false);
        m_pTerminal->setPrintPreview(false);
        m_pTerminal->onScreenCountChanged();
        hide();
        QMessageBox::information(0,"Finished","PRINT COMPLETED\n\nAll "+QString::number(m_iLastLayer)+" layers built.");
    }
    else
    {
        // do next layer
        m_iCurLayerNumber++;  // set the next layer number
        m_pTerminal->rcNextPrint(curLayerIndexMM());
        m_iPrintState = PRINT_RELEASING;
        ui->lineEditLayerCount->setText("Releasing Layer "+QString::number(m_iCurLayerNumber)+", repositioning to layer "+QString::number(m_iCurLayerNumber+1));
        QString sTimeUpdate = updateTimes();
        setProjMessage("(Press'p' to pause, 'A' to ABORT)  " + sTimeUpdate+"  Release and cycle to Layer "+QString::number(m_iCurLayerNumber+1)+" of "+QString::number(m_iLastLayer));
     }
}