/*************************************************************************************
//
//  LICENSE INFORMATION
//
//  BCreator(tm)
//  Software for the control of the 3D Printer, "B9Creator"(tm)
//
//  Copyright 2011-2012 B9Creations, LLC
//  B9Creations(tm) and B9Creator(tm) are trademarks of B9Creations, LLC
//
//  This work is licensed under the:
//      "Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License"
//
//  To view a copy of this license, visit:
//      http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
//
//
//  For updates and to download the lastest version, visit:
//      http://github.com/B9Creations or
//      http://b9creator.com
//
//  The above copyright notice and this permission notice shall be
//    included in all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
//    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
*************************************************************************************/
#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QProcess>
#include <QtGui/QMessageBox>
#include <QtDebug>
#include "helpsystem.h"

HelpSystem::HelpSystem()
    : pHelpProcess(0)
{
}

HelpSystem::~HelpSystem()
{
    if (pHelpProcess && pHelpProcess->state() == QProcess::Running) {
        pHelpProcess->terminate();
        pHelpProcess->waitForFinished();
    }
    pHelpProcess->deleteLater();
}

void HelpSystem::showHelpFile(const QString &file)
{
    if (!startHelp())return;
    QByteArray ba("SetSource ");
    ba.append("qthelp://com.b9creations.b9creator/doc/");
    pHelpProcess->write(ba + file.toLocal8Bit() + '\n');
}

bool HelpSystem::startHelp()
{
    if (!pHelpProcess) pHelpProcess = new QProcess();
    if (pHelpProcess->state() != QProcess::Running) {
        QString app = QDir::currentPath();
#if !defined(Q_OS_MAC)
        app += QLatin1String("/documentation/assistant");
#else
        app += QLatin1String("/Assistant.app/Contents/MacOS/Assistant");
#endif

        QStringList args;
        args << QLatin1String("-collectionFile")
            << QDir::currentPath() + QLatin1String("/documentation/b9creator.qhc")
            << QLatin1String("-enableRemoteControl");
qDebug() << "path to ghc: "<<app << " " << args;

        pHelpProcess->start(app, args);

        if (!pHelpProcess->waitForStarted()) {
            QMessageBox::critical(0, QObject::tr("B9Creator - 3D Printer"),
                QObject::tr("Unable to launch help system (%1)").arg(app));
            return false;
        }
    }
    return true;
}