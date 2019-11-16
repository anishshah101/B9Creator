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
//  This file is part of B9Creator
//
//    B9Creator is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    B9Creator is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with B9Creator .  If not, see <http://www.gnu.org/licenses/>.
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

#include "logfilemanager.h"
#include <QTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QUrl>
#include <QDebug>
#include <QtDebug>
#include <QApplication>
#include <QtMessageHandler>
#include <QDesktopServices>
#include <qapplication.h>
#include "OS_Wrapper_Functions.h"

bool bGlobalPrinting;

QString sLogFileName;

void messageHandler(QtMsgType type,const QMessageLogContext& context, const QString &msg)
{
    QFile outFile(sLogFileName);
    QString text;
    switch(type)
    {
    case QtDebugMsg:
        text = QString("Debug:");
        break;

    case QtWarningMsg:
        text = QString("Warning:");
        break;
    case QtCriticalMsg:
        text = QString("Critical:");
        break;
    case QtFatalMsg:
        text = QString("Fatal:");
    }

    // 设置输出信息格式
    QString context_info = QString("File:(%1) Line:(%2)").arg(QString(context.file)).arg(context.line);
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ddd");
    QString current_date = QString("(%1)").arg(current_date_time);
    QString message = QString("%1 %2 %3 %4").arg(text).arg(context_info).arg(msg).arg(current_date);

    // 输出信息至文件中（读写、追加形式）
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream text_stream(&outFile);
    text_stream << message << "\r\n";
    outFile.flush();
    outFile.close();
}

LogFileManager::LogFileManager(QString sLogFile, QString sHeader)
{
    bGlobalPrinting = false;
    QFile::remove(sLogFile);
    sLogFileName = sLogFile;

    QFile outFile(sLogFile);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << sHeader << "\r\n\r" << endl;
    outFile.close();
    //注册MessageHandler
    qInstallMessageHandler(messageHandler);
}

LogFileManager::~LogFileManager()
{
    qInstallMessageHandler(nullptr);
}

void LogFileManager::setPrinting(bool bPrinting)
{
    bGlobalPrinting = bPrinting;
}

void LogFileManager::openLogFileInFolder()
{
    QString path = CROSS_OS_GetDirectoryFromLocationTag("DOCUMENTS_DIR");
    path += "/" + sLogFileName;
    qDebug() << "Log File Location "+path;
    path = "file:///" + path;
    QDesktopServices::openUrl(QUrl(path));
}

