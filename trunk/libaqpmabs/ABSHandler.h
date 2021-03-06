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

#ifndef ABSHANDLER_H
#define ABSHANDLER_H

#include "Visibility.h"

#include <QProcess>

namespace Aqpm {

class AQPM_EXPORT ABSHandler : public QObject {

    Q_OBJECT

    public:

        ABSHandler *instance();

        ABSHandler();
        virtual ~ABSHandler();

        static QString getABSPath( const QString &package );
        bool setUpBuildingEnvironment(const QString &package, const QString &p);
        bool cleanBuildingEnvironment(const QString &package, const QString &p);

        void updateABSTree();

        static QStringList getMakeDepends(const QString &package);

    private Q_SLOTS:
        void slotABSUpdated(int code, QProcess::ExitStatus e);
        void slotOutputReady();

    Q_SIGNALS:
        void absTreeUpdated(bool success);
        void absUpdateOutput(const QString &output);

    private:
        class Private;
        Private *d;
};

}

#endif /* ABSHANDLER_H_ */
