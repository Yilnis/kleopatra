/*
    This file is part of Kleopatra's test suite.
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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

#include "kleo_test.h"

#include <QGpgME/Protocol>
#include <QGpgME/VerifyDetachedJob>
#include <QGpgME/KeyListJob>
#include <QGpgME/DecryptVerifyJob>


#include <gpgme++/error.h>
#include <gpgme++/verificationresult.h>
#include <gpgme++/decryptionresult.h>
#include <gpgme++/key.h>
#include <QTimer>
#include <QObject>
#include <QSignalSpy>

// Replace this with a gpgme version check once GnuPG Bug #2092
// ( https://bugs.gnupg.org/gnupg/issue2092 ) is fixed.
#define GPGME_MULTITHREADED_KEYLIST_BROKEN

Q_DECLARE_METATYPE(GpgME::VerificationResult)

class VerifyTest : public QObject
{
    Q_OBJECT
private:

    // Data shared with all tests
    QByteArray mSignature;
    QByteArray mSignedData;
    const QGpgME::Protocol *mBackend;
    QEventLoop mEventLoop;

    // Data for testParallelVerifyAndKeyListJobs()
    QList<QGpgME::VerifyDetachedJob *> mParallelVerifyJobs;
    QList<QGpgME::KeyListJob *> mParallelKeyListJobs;

    // Data for testMixedParallelJobs()
    QList<QGpgME::Job *> mRunningJobs;
    int mJobsStarted;

public Q_SLOTS:
    void slotParallelKeyListJobFinished()
    {
        mParallelKeyListJobs.removeAll(static_cast<QGpgME::KeyListJob *>(sender()));

        // When all jobs are done, quit the event loop
        if (mParallelVerifyJobs.isEmpty() && mParallelKeyListJobs.isEmpty()) {
            mEventLoop.quit();
        }
    }

    void slotParallelVerifyJobFinished(GpgME::VerificationResult result)
    {
        // Verify the result of the job is correct
        QVERIFY(mParallelVerifyJobs.contains(static_cast<QGpgME::VerifyDetachedJob *>(sender())));
        QCOMPARE(result.signature(0).validity(), GpgME::Signature::Full);
        mParallelVerifyJobs.removeAll(static_cast<QGpgME::VerifyDetachedJob *>(sender()));

        // Start a key list job
        QGpgME::KeyListJob *job = mBackend->keyListJob();
        mParallelKeyListJobs.append(job);
        connect(job, &QGpgME::Job::done,
                this, &VerifyTest::slotParallelKeyListJobFinished);
        QVERIFY(!job->start(QStringList()));
    }

    void someJobDone()
    {
        // Don't bother checking any results here
        mRunningJobs.removeAll(static_cast<QGpgME::Job *>(sender()));
    }

    void startAnotherJob()
    {
        static int counter = 0;
        counter++;

        // Randomly kill a running job
        if (counter % 10 == 0 && !mRunningJobs.isEmpty()) {
            mRunningJobs.at(counter % mRunningJobs.size())->slotCancel();
        }

        // Randomly either start a keylist or a verify job
        QGpgME::Job *job;
        if (counter % 2 == 0) {
            QGpgME::VerifyDetachedJob *vdj = mBackend->verifyDetachedJob();
            QVERIFY(!vdj->start(mSignature, mSignedData));
            job = vdj;
        } else {
            QGpgME::KeyListJob *klj = mBackend->keyListJob();
            QVERIFY(!klj->start(QStringList()));
            job = klj;
        }
        mRunningJobs.append(job);
        connect(job, &QGpgME::Job::done, this, &VerifyTest::someJobDone);

        // Quit after 2500 jobs, that should be enough
        mJobsStarted++;
        if (mJobsStarted >= 2500) {
            QTimer::singleShot(1000, &mEventLoop, &QEventLoop::quit);
        } else {
            QTimer::singleShot(0, this, &VerifyTest::startAnotherJob);
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<GpgME::VerificationResult>();

        const QString sigFileName = QLatin1String(KLEO_TEST_DATADIR) + QLatin1String("/test.data.sig");
        const QString dataFileName = QLatin1String(KLEO_TEST_DATADIR) + QLatin1String("/test.data");

        QFile sigFile(sigFileName);
        QVERIFY(sigFile.open(QFile::ReadOnly));
        QFile dataFile(dataFileName);
        QVERIFY(dataFile.open(QFile::ReadOnly));

        mSignature = sigFile.readAll();
        mSignedData = dataFile.readAll();

        mBackend = QGpgME::openpgp();
    }

    void testVerify()
    {
        QGpgME::VerifyDetachedJob *job = mBackend->verifyDetachedJob();
        QSignalSpy spy(job, SIGNAL(result(GpgME::VerificationResult)));
        QVERIFY(spy.isValid());
        GpgME::Error err = job->start(mSignature, mSignedData);
        QVERIFY(!err);
        QTest::qWait(1000);   // ### we need to enter the event loop, can be done nicer though

        QCOMPARE(spy.count(), 1);
        GpgME::VerificationResult result = spy.takeFirst().at(0).value<GpgME::VerificationResult>();
        QCOMPARE(result.numSignatures(), 1U);

        GpgME::Signature sig = result.signature(0);
        QCOMPARE(sig.summary() & GpgME::Signature::KeyMissing, 0);
        QCOMPARE((quint64) sig.creationTime(), Q_UINT64_C(1530524124));
        QCOMPARE(sig.validity(), GpgME::Signature::Full);
    }

    /* Test that the decrypt verify job also works with signed only, not
     * encrypted PGP messages */
    void testDecryptVerifyOpaqueSigned()
    {
        const QString sigFileName = QLatin1String(KLEO_TEST_DATADIR) +
                                    QLatin1String("/test.data.signed-opaque.asc");
        std::pair<GpgME::DecryptionResult, GpgME::VerificationResult> result;
        QByteArray plaintext;
        QFile sigFile(sigFileName);
        QVERIFY(sigFile.open(QFile::ReadOnly));
        const QByteArray ciphertext = sigFile.readAll();

        QGpgME::DecryptVerifyJob *job = mBackend->decryptVerifyJob();
        result = job->exec(ciphertext, plaintext);
        QVERIFY(result.first.error().code());

        QVERIFY(result.second.numSignatures());
        GpgME::Signature sig = result.second.signature(0);
        QVERIFY(sig.validity() == GpgME::Signature::Validity::Full);
        QVERIFY(!sig.status().code());
        QVERIFY(QString::fromUtf8(plaintext).startsWith(
                    QLatin1String("/* -*- mode: c++; c-basic-offset:4 -*-")));
    }

#ifndef GPGME_MULTITHREADED_KEYLIST_BROKEN
    // The following two tests are disabled because they trigger an
    // upstream bug in gpgme. See: https://bugs.gnupg.org/gnupg/issue2092
    // Which has a testcase attached that does similar things using gpgme
    // directly and triggers various problems.
    void testParallelVerifyAndKeyListJobs()
    {
        // ### Increasing 10 to 500 makes the verify jobs fail!
        // ^ This should also be reevaluated if the underlying bug in gpgme
        // is fixed.
        for (int i = 0; i < 10; ++i) {
            QGpgME::VerifyDetachedJob *job = mBackend->verifyDetachedJob();
            mParallelVerifyJobs.append(job);
            QVERIFY(!job->start(mSignature, mSignedData));
            connect(job, SIGNAL(result(GpgME::VerificationResult)),
                    this, SLOT(slotParallelVerifyJobFinished(GpgME::VerificationResult)));
        }

        mEventLoop.exec();
    }

    void testMixedParallelJobs()
    {
        mJobsStarted = 0;
        QTimer::singleShot(0, this, SLOT(startAnotherJob()));
        mEventLoop.exec();
    }
#endif
};

QTEST_KLEOMAIN(VerifyTest)

#include "test_verify.moc"
