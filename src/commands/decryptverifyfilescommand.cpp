/* -*- mode: c++; c-basic-offset:4 -*-
    commands/decryptverifyfilescommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "decryptverifyfilescommand.h"

#include "fileoperationspreferences.h"
#include "command_p.h"

#include "crypto/decryptverifyfilescontroller.h"
#include "crypto/autodecryptverifyfilescontroller.h"

#include <utils/filedialog.h>

#include <Libkleo/Stl_Util>

#include <KLocalizedString>
#include "kleopatra_debug.h"

#include <QStringList>

#include <exception>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Crypto;

class DecryptVerifyFilesCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::DecryptVerifyFilesCommand;
    DecryptVerifyFilesCommand *q_func() const
    {
        return static_cast<DecryptVerifyFilesCommand *>(q);
    }
public:
    explicit Private(DecryptVerifyFilesCommand *qq, KeyListController *c,
                     bool forceManualMode=false);
    ~Private();

    QStringList selectFiles() const;

    void init();

private:
    void slotControllerDone()
    {
        finished();
    }
    void slotControllerError(int, const QString &msg)
    {
        KMessageBox::error(parentWidgetOrView(), msg, i18n("Decrypt/Verify Failed"));
        finished();
    }

private:
    QStringList files;
    std::shared_ptr<const ExecutionContext> shared_qq;
    DecryptVerifyFilesController *mController;
};

DecryptVerifyFilesCommand::Private *DecryptVerifyFilesCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const DecryptVerifyFilesCommand::Private *DecryptVerifyFilesCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

DecryptVerifyFilesCommand::Private::Private(DecryptVerifyFilesCommand *qq, KeyListController *c, bool forceManualMode)
    : Command::Private(qq, c),
      files(),
      shared_qq(qq, [](DecryptVerifyFilesCommand*){})
{
    FileOperationsPreferences prefs;
    if (!forceManualMode &&
        GpgME::hasFeature(0, GpgME::BinaryAndFineGrainedIdentify) &&
        prefs.autoDecryptVerify()) {
        mController = new AutoDecryptVerifyFilesController();
    } else {
        mController = new DecryptVerifyFilesController();
    }

}

DecryptVerifyFilesCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
    delete mController;
}

DecryptVerifyFilesCommand::DecryptVerifyFilesCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

DecryptVerifyFilesCommand::DecryptVerifyFilesCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

DecryptVerifyFilesCommand::DecryptVerifyFilesCommand(const QStringList &files, KeyListController *c, bool forceManualMode)
    : Command(new Private(this, c, forceManualMode))
{
    d->init();
    d->files = files;
}

DecryptVerifyFilesCommand::DecryptVerifyFilesCommand(const QStringList &files, QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
    d->files = files;
}

void DecryptVerifyFilesCommand::Private::init()
{
    mController->setExecutionContext(shared_qq);
    connect(mController, SIGNAL(done()), q, SLOT(slotControllerDone()));
    connect(mController, SIGNAL(error(int,QString)), q, SLOT(slotControllerError(int,QString)));
}

DecryptVerifyFilesCommand::~DecryptVerifyFilesCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

void DecryptVerifyFilesCommand::setFiles(const QStringList &files)
{
    d->files = files;
}

void DecryptVerifyFilesCommand::setOperation(DecryptVerifyOperation op)
{
    try {
        d->mController->setOperation(op);
    } catch (...) {}
}

DecryptVerifyOperation DecryptVerifyFilesCommand::operation() const
{
    return d->mController->operation();
}

void DecryptVerifyFilesCommand::doStart()
{

    try {

        if (d->files.empty()) {
            d->files = d->selectFiles();
        }
        if (d->files.empty()) {
            d->finished();
            return;
        }
        d->mController->setFiles(d->files);
        d->mController->start();

    } catch (const std::exception &e) {
        d->information(i18n("An error occurred: %1",
                            QString::fromLocal8Bit(e.what())),
                       i18n("Decrypt/Verify Files Error"));
        d->finished();
    }
}

void DecryptVerifyFilesCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    d->mController->cancel();
}

QStringList DecryptVerifyFilesCommand::Private::selectFiles() const
{
    return FileDialog::getOpenFileNames(parentWidgetOrView(), i18n("Select One or More Files to Decrypt and/or Verify"), QStringLiteral("enc"));
}

#undef d
#undef q

#include "moc_decryptverifyfilescommand.cpp"
