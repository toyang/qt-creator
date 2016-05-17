/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "documentmodel.h"
#include "ieditor.h"
#include <coreplugin/documentmanager.h>
#include <coreplugin/idocument.h>
#include <coreplugin/coreicons.h>

#include <utils/algorithm.h>
#include <utils/dropsupport.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QAbstractItemModel>
#include <QDir>
#include <QIcon>
#include <QMimeData>
#include <QSet>
#include <QUrl>

namespace Core {

class DocumentModelPrivate : public QAbstractItemModel
{
    Q_OBJECT

public:
    ~DocumentModelPrivate();

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QModelIndex parent(const QModelIndex &/*index*/) const override { return QModelIndex(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;

    void addEntry(DocumentModel::Entry *entry);
    void removeDocument(int idx);

    int indexOfFilePath(const Utils::FileName &filePath) const;
    int indexOfDocument(IDocument *document) const;

    bool disambiguateDisplayNames(DocumentModel::Entry *entry);

    static QIcon lockedIcon();

private:
    friend class DocumentModel;
    void itemChanged();

    class DynamicEntry
    {
    public:
        DocumentModel::Entry *entry;
        int pathComponents;

        DynamicEntry(DocumentModel::Entry *e) :
            entry(e),
            pathComponents(0)
        {
        }

        DocumentModel::Entry *operator->() const { return entry; }

        void disambiguate()
        {
            entry->document->setUniqueDisplayName(entry->fileName().fileName(++pathComponents));
        }

        void setNumberedName(int number)
        {
            entry->document->setUniqueDisplayName(QStringLiteral("%1 (%2)")
                                                  .arg(entry->document->displayName())
                                                  .arg(number));
        }
    };

    QList<DocumentModel::Entry *> m_entries;
    QMap<IDocument *, QList<IEditor *> > m_editors;
    QHash<QString, DocumentModel::Entry *> m_entryByFixedPath;
};

DocumentModelPrivate::~DocumentModelPrivate()
{
    qDeleteAll(m_entries);
}

static DocumentModelPrivate *d;

DocumentModel::Entry::Entry() :
    document(0),
    isSuspended(false)
{
}

DocumentModel::Entry::~Entry()
{
    if (isSuspended)
        delete document;
}

DocumentModel::DocumentModel()
{
}

void DocumentModel::init()
{
    d = new DocumentModelPrivate;
}

void DocumentModel::destroy()
{
    delete d;
}

QIcon DocumentModel::lockedIcon()
{
    return DocumentModelPrivate::lockedIcon();
}

QAbstractItemModel *DocumentModel::model()
{
    return d;
}

Utils::FileName DocumentModel::Entry::fileName() const
{
    return document->filePath();
}

QString DocumentModel::Entry::displayName() const
{
    return document->displayName();
}

QString DocumentModel::Entry::plainDisplayName() const
{
    return document->plainDisplayName();
}

Id DocumentModel::Entry::id() const
{
    return document->id();
}

int DocumentModelPrivate::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return 2;
    return 0;
}

int DocumentModelPrivate::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_entries.count() + 1/*<no document>*/;
    return 0;
}

void DocumentModel::addEditor(IEditor *editor, bool *isNewDocument)
{
    if (!editor)
        return;

    QList<IEditor *> &editorList = d->m_editors[editor->document()];
    bool isNew = editorList.isEmpty();
    if (isNewDocument)
        *isNewDocument = isNew;
    editorList << editor;
    if (isNew) {
        Entry *entry = new Entry;
        entry->document = editor->document();
        d->addEntry(entry);
    }
}

void DocumentModel::addSuspendedDocument(const QString &fileName, const QString &displayName, Id id)
{
    Entry *entry = new Entry;
    entry->document = new IDocument;
    entry->document->setFilePath(Utils::FileName::fromString(fileName));
    entry->document->setPreferredDisplayName(displayName);
    entry->document->setId(id);
    entry->isSuspended = true;
    d->addEntry(entry);
}

DocumentModel::Entry *DocumentModel::firstSuspendedEntry()
{
    return Utils::findOrDefault(d->m_entries, [](Entry *entry) { return entry->isSuspended; });
}

void DocumentModelPrivate::addEntry(DocumentModel::Entry *entry)
{
    const Utils::FileName fileName = entry->fileName();
    QString fixedPath;
    if (!fileName.isEmpty())
        fixedPath = DocumentManager::fixFileName(fileName.toString(), DocumentManager::ResolveLinks);

    // replace a non-loaded entry (aka 'suspended') if possible
    int previousIndex = indexOfFilePath(fileName);
    if (previousIndex >= 0) {
        DocumentModel::Entry *previousEntry = m_entries.at(previousIndex);
        const bool replace = !entry->isSuspended && previousEntry->isSuspended;
        if (replace) {
            delete previousEntry;
            m_entries[previousIndex] = entry;
            if (!fixedPath.isEmpty())
                m_entryByFixedPath[fixedPath] = entry;
        } else {
            delete entry;
            entry = previousEntry;
        }
        previousEntry = 0;
        disambiguateDisplayNames(entry);
        if (replace)
            connect(entry->document, &IDocument::changed, this, &DocumentModelPrivate::itemChanged);
        return;
    }

    int index;
    const QString displayName = entry->plainDisplayName();
    for (index = 0; index < m_entries.count(); ++index) {
        int cmp = displayName.localeAwareCompare(m_entries.at(index)->plainDisplayName());
        if (cmp < 0)
            break;
        if (cmp == 0 && fileName < d->m_entries.at(index)->fileName())
            break;
    }
    int row = index + 1/*<no document>*/;
    beginInsertRows(QModelIndex(), row, row);
    m_entries.insert(index, entry);
    disambiguateDisplayNames(entry);
    if (!fixedPath.isEmpty())
        m_entryByFixedPath[fixedPath] = entry;
    connect(entry->document, &IDocument::changed, this, &DocumentModelPrivate::itemChanged);
    endInsertRows();
}

bool DocumentModelPrivate::disambiguateDisplayNames(DocumentModel::Entry *entry)
{
    const QString displayName = entry->plainDisplayName();
    int minIdx = -1, maxIdx = -1;

    QList<DynamicEntry> dups;

    for (int i = 0, total = m_entries.count(); i < total; ++i) {
        DocumentModel::Entry *e = m_entries.at(i);
        if (e == entry || e->plainDisplayName() == displayName) {
            e->document->setUniqueDisplayName(QString());
            dups += DynamicEntry(e);
            maxIdx = i;
            if (minIdx < 0)
                minIdx = i;
        }
    }

    const int dupsCount = dups.count();
    if (dupsCount == 0)
        return false;

    if (dupsCount > 1) {
        int serial = 0;
        int count = 0;
        // increase uniqueness unless no dups are left
        forever {
            bool seenDups = false;
            for (int i = 0; i < dupsCount - 1; ++i) {
                DynamicEntry &e = dups[i];
                const Utils::FileName myFileName = e->document->filePath();
                if (e->document->isTemporary() || myFileName.isEmpty() || count > 10) {
                    // path-less entry, append number
                    e.setNumberedName(++serial);
                    continue;
                }
                for (int j = i + 1; j < dupsCount; ++j) {
                    DynamicEntry &e2 = dups[j];
                    if (e->displayName().compare(e2->displayName(), Utils::HostOsInfo::fileNameCaseSensitivity()) == 0) {
                        const Utils::FileName otherFileName = e2->document->filePath();
                        if (otherFileName.isEmpty())
                            continue;
                        seenDups = true;
                        e2.disambiguate();
                        if (j > maxIdx)
                            maxIdx = j;
                    }
                }
                if (seenDups) {
                    e.disambiguate();
                    ++count;
                    break;
                }
            }
            if (!seenDups)
                break;
        }
    }

    emit dataChanged(index(minIdx + 1, 0), index(maxIdx + 1, 0));
    return true;
}

QIcon DocumentModelPrivate::lockedIcon()
{
    const static QIcon icon = Icons::LOCKED.icon();
    return icon;
}

int DocumentModelPrivate::indexOfFilePath(const Utils::FileName &filePath) const
{
    if (filePath.isEmpty())
        return -1;
    const QString fixedPath = DocumentManager::fixFileName(filePath.toString(),
                                                           DocumentManager::ResolveLinks);
    return m_entries.indexOf(m_entryByFixedPath.value(fixedPath));
}

void DocumentModel::removeEntry(DocumentModel::Entry *entry)
{
    // For non suspended entries, we wouldn't know what to do with the associated editors
    QTC_ASSERT(entry->isSuspended, return);
    int index = d->m_entries.indexOf(entry);
    d->removeDocument(index);
}

void DocumentModel::removeEditor(IEditor *editor, bool *lastOneForDocument)
{
    if (lastOneForDocument)
        *lastOneForDocument = false;
    QTC_ASSERT(editor, return);
    IDocument *document = editor->document();
    QTC_ASSERT(d->m_editors.contains(document), return);
    d->m_editors[document].removeAll(editor);
    if (d->m_editors.value(document).isEmpty()) {
        if (lastOneForDocument)
            *lastOneForDocument = true;
        d->m_editors.remove(document);
        d->removeDocument(indexOfDocument(document));
    }
}

void DocumentModelPrivate::removeDocument(int idx)
{
    if (idx < 0)
        return;
    QTC_ASSERT(idx < d->m_entries.size(), return);
    int row = idx + 1/*<no document>*/;
    beginRemoveRows(QModelIndex(), row, row);
    DocumentModel::Entry *entry = d->m_entries.takeAt(idx);
    endRemoveRows();

    const QString fileName = entry->fileName().toString();
    if (!fileName.isEmpty()) {
        const QString fixedPath = DocumentManager::fixFileName(fileName,
                                                               DocumentManager::ResolveLinks);
        m_entryByFixedPath.remove(fixedPath);
    }
    disconnect(entry->document, &IDocument::changed, this, &DocumentModelPrivate::itemChanged);
    disambiguateDisplayNames(entry);
    delete entry;
}

void DocumentModel::removeAllSuspendedEntries()
{
    for (int i = d->m_entries.count()-1; i >= 0; --i) {
        if (d->m_entries.at(i)->isSuspended) {
            int row = i + 1/*<no document>*/;
            d->beginRemoveRows(QModelIndex(), row, row);
            delete d->m_entries.takeAt(i);
            d->endRemoveRows();
        }
    }
    QSet<QString> displayNames;
    foreach (DocumentModel::Entry *entry, d->m_entries) {
        const QString displayName = entry->plainDisplayName();
        if (displayNames.contains(displayName))
            continue;
        displayNames.insert(displayName);
        d->disambiguateDisplayNames(entry);
    }
}

QList<IEditor *> DocumentModel::editorsForDocument(IDocument *document)
{
    return d->m_editors.value(document);
}

QList<IEditor *> DocumentModel::editorsForOpenedDocuments()
{
    return editorsForDocuments(openedDocuments());
}

QList<IEditor *> DocumentModel::editorsForDocuments(const QList<IDocument *> &documents)
{
    QList<IEditor *> result;
    foreach (IDocument *document, documents)
        result += d->m_editors.value(document);
    return result;
}

int DocumentModel::indexOfDocument(IDocument *document)
{
    return d->indexOfDocument(document);
}

int DocumentModelPrivate::indexOfDocument(IDocument *document) const
{
    return Utils::indexOf(m_entries, [&document](DocumentModel::Entry *entry) {
        return entry->document == document;
    });
}

DocumentModel::Entry *DocumentModel::entryForDocument(IDocument *document)
{
    return Utils::findOrDefault(d->m_entries,
                                [&document](Entry *entry) { return entry->document == document; });
}

DocumentModel::Entry *DocumentModel::entryForFilePath(const Utils::FileName &filePath)
{
    const int index = d->indexOfFilePath(filePath);
    if (index < 0)
        return nullptr;
    return d->m_entries.at(index);
}

QList<IDocument *> DocumentModel::openedDocuments()
{
    return d->m_editors.keys();
}

IDocument *DocumentModel::documentForFilePath(const QString &filePath)
{
    const int index = d->indexOfFilePath(Utils::FileName::fromString(filePath));
    if (index < 0)
        return 0;
    return d->m_entries.at(index)->document;
}

QList<IEditor *> DocumentModel::editorsForFilePath(const QString &filePath)
{
    IDocument *document = documentForFilePath(filePath);
    if (document)
        return editorsForDocument(document);
    return QList<IEditor *>();
}

QModelIndex DocumentModelPrivate::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (column < 0 || column > 1 || row < 0 || row >= m_entries.count() + 1/*<no document>*/)
        return QModelIndex();
    return createIndex(row, column);
}

Qt::DropActions DocumentModelPrivate::supportedDragActions() const
{
    return Qt::MoveAction;
}

QStringList DocumentModelPrivate::mimeTypes() const
{
    return Utils::DropSupport::mimeTypesForFilePaths();
}

DocumentModel::Entry *DocumentModel::entryAtRow(int row)
{
    int entryIndex = row - 1/*<no document>*/;
    if (entryIndex < 0)
        return 0;
    return d->m_entries[entryIndex];
}

int DocumentModel::entryCount()
{
    return d->m_entries.count();
}

QVariant DocumentModelPrivate::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.column() != 0 && role < Qt::UserRole))
        return QVariant();
    const DocumentModel::Entry *entry = DocumentModel::entryAtRow(index.row());
    if (!entry) {
        // <no document> entry
        switch (role) {
        case Qt::DisplayRole:
            return tr("<no document>");
        case Qt::ToolTipRole:
            return tr("No document is selected.");
        default:
            return QVariant();
        }
    }
    switch (role) {
    case Qt::DisplayRole: {
        QString name = entry->displayName();
        if (entry->document->isModified())
            name += QLatin1Char('*');
        return name;
    }
    case Qt::DecorationRole:
        return entry->document->isFileReadOnly() ? lockedIcon() : QIcon();
    case Qt::ToolTipRole:
        return entry->fileName().isEmpty() ? entry->displayName() : entry->fileName().toUserOutput();
    default:
        return QVariant();
    }
    return QVariant();
}

Qt::ItemFlags DocumentModelPrivate::flags(const QModelIndex &index) const
{
    const DocumentModel::Entry *e = DocumentModel::entryAtRow(index.row());
    if (!e || e->fileName().isEmpty())
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QMimeData *DocumentModelPrivate::mimeData(const QModelIndexList &indexes) const
{
    auto data = new Utils::DropMimeData;
    foreach (const QModelIndex &index, indexes) {
        const DocumentModel::Entry *e = DocumentModel::entryAtRow(index.row());
        if (!e || e->fileName().isEmpty())
            continue;
        data->addFile(e->fileName().toString());
    }
    return data;
}

int DocumentModel::rowOfDocument(IDocument *document)
{
    if (!document)
        return 0 /*<no document>*/;
    return indexOfDocument(document) + 1/*<no document>*/;
}

void DocumentModelPrivate::itemChanged()
{
    IDocument *document = qobject_cast<IDocument *>(sender());

    int idx = indexOfDocument(document);
    if (idx < 0)
        return;
    const QString fileName = document->filePath().toString();
    QString fixedPath;
    if (!fileName.isEmpty())
        fixedPath = DocumentManager::fixFileName(fileName, DocumentManager::ResolveLinks);
    DocumentModel::Entry *entry = d->m_entries.at(idx);
    bool found = false;
    // The entry's fileName might have changed, so find the previous fileName that was associated
    // with it and remove it, then add the new fileName.
    for (auto it = m_entryByFixedPath.begin(), end = m_entryByFixedPath.end(); it != end; ++it) {
        if (it.value() == entry) {
            found = true;
            if (it.key() != fixedPath) {
                m_entryByFixedPath.remove(it.key());
                if (!fixedPath.isEmpty())
                    m_entryByFixedPath[fixedPath] = entry;
            }
            break;
        }
    }
    if (!found && !fixedPath.isEmpty())
        m_entryByFixedPath[fixedPath] = entry;
    if (!disambiguateDisplayNames(d->m_entries.at(idx))) {
        QModelIndex mindex = index(idx + 1/*<no document>*/, 0);
        emit dataChanged(mindex, mindex);
    }
}

QList<DocumentModel::Entry *> DocumentModel::entries()
{
    return d->m_entries;
}

} // namespace Core

#include "documentmodel.moc"
