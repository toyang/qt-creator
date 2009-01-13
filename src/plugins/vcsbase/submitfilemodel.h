/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.3, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#ifndef SUBMITMODEL_H
#define SUBMITMODEL_H

#include "vcsbase_global.h"

#include <QtGui/QStandardItemModel>

namespace VCSBase {

/* A 2-column (checkable, state, file name) model to be used to list the files-
 * in the submit editor. Provides header items and a convience to add files. */

class VCSBASE_EXPORT SubmitFileModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit SubmitFileModel(QObject *parent = 0);

    // Convenience to create and add rows containing a file plus status text.
    static QList<QStandardItem *> createFileRow(const QString &fileName, const QString &status = QString(), bool checked = true);
    QList<QStandardItem *> addFile(const QString &fileName, const QString &status = QString(), bool checked = true);

    // Find convenience that returns the whole row (as opposed to QStandardItemModel::find).
    QList<QStandardItem *> findRow(const QString &text, int column = 0) const;

    // Convenience to obtain a row
    QList<QStandardItem *> rowAt(int row) const;

    // Filter for entries contained in the filter list. Returns the
    // number of deleted entries.
    unsigned filter(const QStringList &filter, int column);
};

}

#endif // SUBMITMODEL_H
