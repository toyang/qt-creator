/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
**
**************************************************************************/

#ifndef QMLMAKESTEP_H
#define QMLMAKESTEP_H

#include <projectexplorer/abstractmakestep.h>

namespace QmlProjectManager {
namespace Internal {

class QmlProject;

class QmlMakeStep : public ProjectExplorer::AbstractMakeStep
{
    Q_OBJECT

public:
    QmlMakeStep(QmlProject *pro);
    virtual ~QmlMakeStep();

    virtual bool init(const QString &buildConfiguration);
    virtual void run(QFutureInterface<bool> &fi);

    virtual QString name();
    virtual QString displayName();
    virtual ProjectExplorer::BuildStepConfigWidget *createConfigWidget();
    virtual bool immutable() const;

    QmlProject *project() const;

    bool buildsTarget(const QString &buildConfiguration, const QString &target) const;
    void setBuildTarget(const QString &buildConfiguration, const QString &target, bool on);

    QStringList additionalArguments(const QString &buildConfiguration) const;
    void setAdditionalArguments(const QString &buildConfiguration, const QStringList &list);

protected:
    virtual void stdOut(const QString &line);

private:
    QmlProject *m_pro;
    QFutureInterface<bool> *m_futureInterface;
};

class QmlMakeStepConfigWidget :public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT

public:
    QmlMakeStepConfigWidget(QmlMakeStep *makeStep);

    virtual QString displayName() const;
    virtual void init(const QString &buildConfiguration);

private:
    QmlMakeStep *m_makeStep;
};

class QmlMakeStepFactory : public ProjectExplorer::IBuildStepFactory
{
public:
    virtual bool canCreate(const QString &name) const;
    virtual ProjectExplorer::BuildStep *create(ProjectExplorer::Project *pro, const QString &name) const;
    virtual QStringList canCreateForProject(ProjectExplorer::Project *pro) const;
    virtual QString displayNameForName(const QString &name) const;
};

} // namespace Internal
} // namespace QmlProjectManager

#endif // MAKESTEP_H
