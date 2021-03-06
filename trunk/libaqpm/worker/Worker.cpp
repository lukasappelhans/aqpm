/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#include "Worker.h"

#include "aqpmworkeradaptor.h"

#include "../ConfigurationParser.h"
#include "w_callbacks.h"
#include "../Backend.h"

#include <QtDBus/QDBusConnection>
#include <QDebug>

namespace AqpmWorker {

class Worker::Private
{
    public:
        Private() : ready(false) {};

        pmdb_t *db_local;
        pmdb_t *dbs_sync;

        bool ready;
};

Worker::Worker(QObject *parent)
 : QObject(parent)
 , d(new Private())
{
    new AqpmworkerAdaptor(this);

    QDBusConnection dbus = QDBusConnection::systemBus();

    dbus.registerObject( "/Worker", this );

    qDebug() << "Aqpm worker registered on the System Bus as" << dbus.baseService();

    if ( !dbus.registerService( "org.chakraproject.aqpmworker" ) ) {
        qFatal("Failed to register alias Service on DBus");
    } else {
        qDebug() << "Service successfully exported on the System Bus.";
    }

    setuid(0);
    seteuid(0);

    qDebug() << "uid and euid:" << getuid() << geteuid();

    alpm_initialize();

    connect(AqpmWorker::CallBacks::instance(), SIGNAL(streamTransDlProg(const QString&, int, int, int, int, int, int)),
            SIGNAL(streamTransDlProg(const QString&, int, int, int, int, int, int)));
    connect(AqpmWorker::CallBacks::instance(), SIGNAL(streamTransProgress(int, const QString&, int, int, int)),
            SIGNAL(streamTransProgress(int, const QString&, int, int, int)));
    connect(AqpmWorker::CallBacks::instance(), SIGNAL(streamTransEvent(int, void*, void*)),
            SIGNAL(streamTransEvent(int, void*, void*)));

    setUpAlpm();
}

Worker::~Worker()
{
}

bool Worker::isWorkerReady()
{
    return d->ready;
}

void Worker::setUpAlpm()
{
    Aqpm::PacmanConf pdata;

    pdata = Aqpm::ConfigurationParser::instance()->getPacmanConf(true);

    alpm_option_set_root("/");
    alpm_option_set_dbpath("/var/lib/pacman");
    alpm_option_add_cachedir("/var/cache/pacman/pkg");

    d->db_local = alpm_db_register_local();

    alpm_option_set_dlcb(AqpmWorker::cb_dl_progress);
    alpm_option_set_totaldlcb(AqpmWorker::cb_dl_total);
    alpm_option_set_logcb(AqpmWorker::cb_log);

    if (pdata.logFile.isEmpty()) {
        alpm_option_set_logfile("/var/log/pacman.log");
    } else {
        alpm_option_set_logfile(pdata.logFile.toAscii().data());
    }

    /* Register our sync databases, kindly taken from pacdata */

    if (!pdata.loaded) {
        qDebug() << "Error Parsing Pacman Configuration!!";
    }

    for (int i = 0; i < pdata.syncdbs.size(); ++i) {
        if (pdata.serverAssoc.size() <= i) {
            qDebug() << "Could not find a matching repo for" << pdata.syncdbs.at(i);
            continue;
        }

        d->dbs_sync = alpm_db_register_sync(pdata.syncdbs.at(i).toAscii().data());

        if (alpm_db_setserver(d->dbs_sync, pdata.serverAssoc.at(i).toAscii().data()) == 0) {
            qDebug() << pdata.syncdbs.at(i) << "--->" << pdata.serverAssoc.at(i);
        } else {
            qDebug() << "Failed to add" << pdata.syncdbs.at(i) << "!!";
        }
    }

    if (!pdata.xferCommand.isEmpty()) {
        qDebug() << "XFerCommand is:" << pdata.xferCommand;
        alpm_option_set_xfercommand(pdata.xferCommand.toAscii().data());
    }

    alpm_option_set_nopassiveftp(pdata.noPassiveFTP);

    foreach(const QString &str, pdata.HoldPkg) {
        alpm_option_add_holdpkg(str.toAscii().data());
    }

    foreach(const QString &str, pdata.IgnorePkg) {
        alpm_option_add_ignorepkg(str.toAscii().data());
    }

    foreach(const QString &str, pdata.IgnoreGrp) {
        alpm_option_add_ignoregrp(str.toAscii().data());
    }

    foreach(const QString &str, pdata.NoExtract) {
        alpm_option_add_noextract(str.toAscii().data());
    }

    foreach(const QString &str, pdata.NoUpgrade) {
        alpm_option_add_noupgrade(str.toAscii().data());
    }

    //alpm_option_set_usedelta(pdata.useDelta); Until a proper implementation is there
    alpm_option_set_usesyslog(pdata.useSysLog);

    d->ready = true;
    emit workerReady();
}

void Worker::updateDatabase()
{
    qDebug() << "Starting DB Update";

    if (alpm_trans_init(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_ALLDEPS, AqpmWorker::cb_trans_evt,
                        AqpmWorker::cb_trans_conv, AqpmWorker::cb_trans_progress) == -1) {
        emit workerFailure();
        qDebug() << "Error!";
        return;
    }

    int r;
    alpm_list_t *syncdbs;

    syncdbs = alpm_list_first(alpm_option_get_syncdbs());

    QStringList list;

    while (syncdbs != NULL) {
        pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(syncdbs);

        list.append(QString((char *)alpm_db_get_name(dbcrnt)));
        syncdbs = alpm_list_next(syncdbs);
    }

    qDebug() << "Found " << list;

    emit dbQty(list);

    syncdbs = alpm_list_first(alpm_option_get_syncdbs());

    while (syncdbs != NULL) {
        qDebug() << "Updating DB";
        pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(syncdbs);
        QString curdbname((char *)alpm_db_get_name(dbcrnt));

        emit dbStatusChanged(curdbname, Aqpm::Backend::Checking);

        r = alpm_db_update(0, dbcrnt);

        if (r == 1) {
            emit dbStatusChanged(curdbname, Aqpm::Backend::Unchanged);
        } else if (r < 0) {
            emit dbStatusChanged(curdbname, Aqpm::Backend::DatabaseError);
        } else {
            emit dbStatusChanged(curdbname, Aqpm::Backend::Updated);
        }

        syncdbs = alpm_list_next(syncdbs);
    }

    if (alpm_trans_release() == -1) {
        if (alpm_trans_interrupt() == -1) {
            emit workerFailure();
        }
    }

    qDebug() << "Database Update Performed";

    emit workerSuccess();
}

void Worker::processQueue(QVariantList packages, QVariantList flags)
{

}

}
