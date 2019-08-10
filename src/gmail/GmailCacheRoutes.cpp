#include <QDir>
#include <QFile>
#include "GmailCacheRoutes.h"
#include "GmailRoutes.h"
#include "Endpoint.h"
#include "gcontact/GcontactRoutes.h"
#include "gcontact/GcontactCache.h"

using namespace googleQt;

#define EXPECT_STRING_VAL(S, W) if(S.isEmpty()){                        \
        qWarning() << "Expected for cache setup:" << W << "Call setupSQLiteCache first."; \
        return false;                                                   \
    }                                                                   \

mail_cache::GmailCacheRoutes::GmailCacheRoutes(Endpoint& endpoint, 
                                            GmailRoutes& gmail_routes)
    :m_endpoint(endpoint),
    m_gmail_routes(gmail_routes)
{
    m_GMsgCache.reset(new mail_cache::GMailCache(endpoint));
    m_GThreadCache.reset(new mail_cache::GThreadCache(endpoint));
};

mail_cache::GmailCacheRoutes::~GmailCacheRoutes()
{
	clearCache();
};

mail_cache::GMailCache* mail_cache::GmailCacheRoutes::mcache() 
{ 
	if (!m_GMsgCache)
		return nullptr;
	return m_GMsgCache.get(); 
}

mail_cache::GThreadCache* mail_cache::GmailCacheRoutes::tcache() 
{
	if (!m_GThreadCache)
		return nullptr;
	return m_GThreadCache.get();
};

mail_cache::GMailSQLiteStorage* mail_cache::GmailCacheRoutes::storage() 
{
	if (!m_lite_storage)
		return nullptr;
	return m_lite_storage.get();
};

RESULT_LIST<messages::MessageResource>&& mail_cache::GmailCacheRoutes::getUserBatchMessages(EDataState f,
                                                                                            const STRING_LIST& id_list)
{
    return getUserBatchMessages_Async(f, id_list)->waitForResultAndRelease();
};

mail_cache::GMailCacheQueryTask* mail_cache::GmailCacheRoutes::newMessageResultFetcher(EDataState state)
{
    auto rfetcher = new mail_cache::GMailCacheQueryTask(
        state,
        m_endpoint,
        *this,
        m_GMsgCache.get());
    return rfetcher;
};

mail_cache::GThreadCacheQueryTask* mail_cache::GmailCacheRoutes::newThreadResultFetcher(query_ptr q)
{
    auto rfetcher = new mail_cache::GThreadCacheQueryTask(
                                                          *this,
                                                          m_GThreadCache.get(),
                                                          q);
    return rfetcher;
};



ConcurrentValueRunner<QString,
                mail_cache::MessagesReceiver,
                messages::MessageResource>* mail_cache::GmailCacheRoutes::getUserBatchMessages_Async(EDataState f, 
                                                                                         const STRING_LIST& id_list)
{
    std::unique_ptr<mail_cache::MessagesReceiver> mr(new mail_cache::MessagesReceiver(m_gmail_routes, f));
    
    ConcurrentValueRunner<QString,
                    mail_cache::MessagesReceiver,
                    messages::MessageResource>* r = new ConcurrentValueRunner<QString,
                                                                        mail_cache::MessagesReceiver,
                                                                        messages::MessageResource>(id_list, std::move(mr), m_endpoint);
    r->run();
    return r;
};

mail_cache::mdata_result mail_cache::GmailCacheRoutes::getCacheMessages(EDataState state, const STRING_LIST& id_list)
{
    return getCacheMessages_Async(state, id_list)->waitForResultAndRelease();
};


mail_cache::GMailCacheQueryTask* mail_cache::GmailCacheRoutes::getCacheMessages_Async(EDataState state,
    const STRING_LIST& id_list,
    mail_cache::GMailCacheQueryTask* rfetcher /*= nullptr*/)
{
    if (!rfetcher)
    {
        rfetcher = newMessageResultFetcher(state);
    }
    m_GMsgCache->query_Async(state, id_list, rfetcher);
    return rfetcher;
};

mail_cache::GMailCacheQueryTask* mail_cache::GmailCacheRoutes::getNextCacheMessages_Async(
    int messagesCount /*= 40*/,
    QString pageToken /*= ""*/,
    QStringList* labels /*= nullptr*/,
    QString q /*= ""*/)
{
    auto rfetcher = newMessageResultFetcher(EDataState::snippet);

    gmail::ListArg listArg;
    listArg.setMaxResults(messagesCount);
    listArg.setPageToken(pageToken);
    listArg.setQ(q);
    if (labels)
    {
        listArg.labels() = *labels;
    }

    m_gmail_routes.getMessages()->list_Async(listArg)->then([=](std::unique_ptr<messages::MessageListRes> mlist)
    {
        STRING_LIST id_list;
        for (auto& m : mlist->messages())
        {
            id_list.push_back(m.id());
        }
        //        rfetcher->setNextPageToken(mlist->nextpagetoken());
        rfetcher->m_nextPageToken = mlist->nextpagetoken();
        getCacheMessages_Async(EDataState::snippet, id_list, rfetcher);
    },
        [=](std::unique_ptr<GoogleException> ex)
    {
        rfetcher->failed_callback(std::move(ex));
    });

    return rfetcher;
};

mail_cache::mdata_result mail_cache::GmailCacheRoutes::getNextCacheMessages(
    int messagesCount /*= 40*/,
    QString pageToken /*= ""*/,
    QStringList* labels /*= nullptr*/,
    QString q /*= ""*/)
{
    return getNextCacheMessages_Async(
        messagesCount,
        pageToken,        
        labels,
        q)->waitForResultAndRelease();
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::trashCacheMessage_Async(QString msg_id)
{
    GoogleVoidTask* rv = m_endpoint.produceVoidTask();
    googleQt::gmail::TrashMessageArg arg(m_endpoint.client()->userId(), msg_id);
    m_gmail_routes.getMessages()->trash_Async(arg)->then([=]()
    {
        //clean up cache
        if (m_lite_storage) {
            std::set<QString> set2remove;
            set2remove.insert(msg_id);
            m_GMsgCache->persistent_clear(set2remove);
            m_lite_storage->deleteAttachmentsFromDb(msg_id);
        }
        rv->completed_callback();
    },
        [=](std::unique_ptr<GoogleException> ex) {
        rv->failed_callback(std::move(ex));
    });
    return rv;
};

/*
ConcurrentValueRunner<QString, 
    mail_cache::ThreadsReceiver, 
    threads::ThreadResource>* mail_cache::GmailCacheRoutes::getUserBatchThreads_Async(const STRING_LIST& id_list)
{
    std::unique_ptr<mail_cache::ThreadsReceiver> tr(new mail_cache::ThreadsReceiver(m_gmail_routes));
    auto r = new ConcurrentValueRunner<QString,
                            mail_cache::ThreadsReceiver,
                            threads::ThreadResource>(id_list, std::move(tr), m_endpoint);
    r->run();
    return r;
};*/

mail_cache::tdata_result mail_cache::GmailCacheRoutes::getNextCacheThreads(
    int messagesCount /*= 40*/,
    QString pageToken /*= ""*/,
    QStringList* labels /*= nullptr*/,
    QString q /*= ""*/) 
{
    return getNextCacheThreads_Async(
        messagesCount,
        pageToken,
        labels,
        q)->waitForResultAndRelease();
};


mail_cache::GThreadCacheQueryTask* mail_cache::GmailCacheRoutes::getNextCacheThreads_Async(
    int threadsCount /*= 40*/,
    QString pageToken /*= ""*/,
    QStringList* labels /*= nullptr*/,
    QString q_str /*= ""*/)
{
    q_str = q_str.trimmed();
    auto rfetcher = newThreadResultFetcher();
    gmail::ListArg listArg;
    listArg.setMaxResults(threadsCount);
    listArg.setPageToken(pageToken);
    listArg.setQ(q_str);
    if (labels)
    {
        listArg.labels() = *labels;
    }

    ///this will return list of thread Ids with HistoryId
    m_gmail_routes.getThreads()->list_Async(listArg)->then([=](std::unique_ptr<threads::ThreadListRes> tlist)
    {
        std::vector<HistId> id_list;
        
        for (auto& m : tlist->threads())
        {
            HistId h;
            h.id = m.id();
            h.hid = m.historyid();
            id_list.push_back(h);
        }
        rfetcher->m_nextPageToken = tlist->nextpagetoken();
        getCacheThreadList_Async(id_list, rfetcher);
    },
        [=](std::unique_ptr<GoogleException> ex)
    {
        rfetcher->failed_callback(std::move(ex));
    });

    return rfetcher;
};

mail_cache::GThreadCacheQueryTask* mail_cache::GmailCacheRoutes::getQCache_Async(
    query_ptr q,
    int threadsCount /*= 40*/,
    QString pageToken /*= ""*/,
    bool monitorProgress /*= false*/)
{
    auto rfetcher = newThreadResultFetcher(q);
    gmail::ListArg listArg;
    listArg.setMaxResults(threadsCount);
    listArg.setPageToken(pageToken);
    listArg.setQ(q->qStr());
    listArg.labels() = q->labelid().split(" ");

    if (monitorProgress) {
        auto p = rfetcher->createProgressNotifier();
        if (p) {
            p->setMaximum(0, QString("query gmail q='%1' l='%2'").arg(q->qStr()).arg(q->labelid()));
        }
    }

    ///this will return list of thread Ids with HistoryId
    m_gmail_routes.getThreads()->list_Async(listArg)->then([=](std::unique_ptr<threads::ThreadListRes> tlist)
    {
        std::vector<HistId> id_list;
        for (auto& m : tlist->threads())
        {
            HistId h;
            h.id = m.id();
            h.hid = m.historyid();
            id_list.push_back(h);
            if (q) {
                if (q->m_tmap.find(m.id()) == q->m_tmap.end()) {
                    q->m_qnew_thread_ids.push_back(m.id());
                }
            }
        }

#ifdef API_QT_AUTOTEST
        if (!q->labelid().isEmpty()) {
            auto lst = q->labelid().split(" ");
            if (!lst.empty()) {
                auto label_id = lst.first();
                auto mcount = q->m_qnew_thread_ids.size();
                if (mcount > 0) {
                    ApiAutotest::INSTANCE().setString4List("messages::MessageResource", "m_labelIds", label_id);
                }
            }
        }
#endif//API_QT_AUTOTEST

        rfetcher->m_nextPageToken = tlist->nextpagetoken();
        getCacheThreadList_Async(id_list, rfetcher);
    q->m_nextPageToken = tlist->nextpagetoken();
    },
        [=](std::unique_ptr<GoogleException> ex)
    {
        rfetcher->failed_callback(std::move(ex));
    });

    return rfetcher;
};

mail_cache::tdata_result mail_cache::GmailCacheRoutes::getQCache(
    mail_cache::query_ptr q,
    int resultsCount /*= 40*/,
    QString pageToken /*= ""*/) 
{
    return getQCache_Async(
        q,
        resultsCount,
        pageToken)->waitForResultAndRelease();

};


mail_cache::GThreadCacheQueryTask* mail_cache::GmailCacheRoutes::getCacheThreadList_Async(
    const std::vector<HistId>& id_list,
    mail_cache::GThreadCacheQueryTask* rfetcher /*= nullptr*/)
{
    if (!rfetcher)
    {
        rfetcher = newThreadResultFetcher();
    }
    m_GThreadCache->queryWithHistory_Async(id_list, rfetcher);
    return rfetcher;
};


bool mail_cache::GmailCacheRoutes::setupSQLiteCache(QString dbPath,
    QString downloadPath,
    QString contactCachePath,
    QString dbName /*= "googleqt"*/,
    QString dbprefix /*= "api"*/,
	int cache_autoload_limit /*= 400*/)
{
    EXPECT_STRING_VAL(m_endpoint.client()->userId(), "UserId");
    EXPECT_STRING_VAL(dbPath, "DB path");
    EXPECT_STRING_VAL(downloadPath, "Download path");
    EXPECT_STRING_VAL(contactCachePath, "contactCachePath path");
    EXPECT_STRING_VAL(dbName, "DB name");
    EXPECT_STRING_VAL(dbprefix, "DB prefix");

    auto cc = m_endpoint.client()->gcontact()->cacheRoutes()->cache();
	if (m_external_contacts_db) {
		/// in case of extarnal contacts DB we don't sync data
		cc->setEnableContactsConfigTable(false);
	}

    m_lite_storage.reset(new mail_cache::GMailSQLiteStorage(m_GMsgCache.get(), m_GThreadCache.get(), cc));
	m_lite_storage->m_external_contacts_db = m_external_contacts_db;
    cc->attachSQLStorage(m_lite_storage.get());
	m_lite_storage->m_cache_autoload_limit = cache_autoload_limit;
    if (!m_lite_storage->init_db(dbPath, downloadPath, contactCachePath, dbName, dbprefix))
    {
		clearCache();
        qWarning() << "Failed to initialize SQLite storage" << dbPath << dbName << dbprefix;
        return false;
    }

    return true;
};

void mail_cache::GmailCacheRoutes::clearCache() 
{
#ifdef API_QT_AUTOTEST
	extern int g__msg_alloc_counter;
	extern int g__thread_alloc_counter;

	m_GMsgCache.reset();
	m_GThreadCache.reset();

	qDebug() << "ykh/clearCache"
		<< "threads" << g__msg_alloc_counter
		<< "msg" << g__thread_alloc_counter;
#endif //API_QT_AUTOTEST

	if (m_lite_storage) {
		m_lite_storage->close_db();
		m_lite_storage.reset();
	}
};

bool mail_cache::GmailCacheRoutes::resetSQLiteCache()
{
    if (!m_GMsgCache) {
        qWarning("GMail cache is not defined. Please call setupSQLiteCache first.");
        return false;
    }

    EXPECT_STRING_VAL(m_endpoint.client()->userId(), "UserId");

    if (!m_lite_storage) {
        qWarning("DB storage of GMail cache is not defined. Please call setupSQLiteCache first.");
        return false;
    }
    QString dbPath = m_lite_storage->m_dbPath;
    QString downloadPath = m_lite_storage->m_downloadDir;
    QString contactCacheDir = m_lite_storage->m_contactCacheDir;
    QString dbName = m_lite_storage->m_dbName;
    QString dbprefix = m_lite_storage->m_metaPrefix;

    EXPECT_STRING_VAL(dbPath, "DB path");
    EXPECT_STRING_VAL(downloadPath, "Download path");
    EXPECT_STRING_VAL(contactCacheDir, "contactCacheDir path");
    EXPECT_STRING_VAL(dbName, "DB name");
    EXPECT_STRING_VAL(dbprefix, "DB prefix");

    if (m_lite_storage) {
        m_lite_storage->close_db();
    }

    /*if (m_GMsgCache) {
        m_GMsgCache->invalidate();
    }*/
    m_GMsgCache.reset(new mail_cache::GMailCache(m_endpoint));
	m_GThreadCache.reset(new mail_cache::GThreadCache(m_endpoint));

    return setupSQLiteCache(dbPath, downloadPath, contactCacheDir, dbName, dbprefix);
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::refreshLabels_Async()
{
    GoogleVoidTask* rv = m_endpoint.produceVoidTask();

    googleQt::GoogleTask<labels::LabelsResultList>* t = m_gmail_routes.getLabels()->list_Async();
    t->then([=](std::unique_ptr<labels::LabelsResultList> lst)
    {
        if (m_lite_storage) {
            for (auto& lbl : lst->labels())
            {
                QString label_id = lbl.id();
                auto db_lbl = m_lite_storage->findLabel(label_id);
                if (db_lbl)
                {
                    m_lite_storage->updateDbLabel(lbl, db_lbl.get());
                }
                else
                {
                    m_lite_storage->insertDbLabel(lbl);
                }
            }
        }
        rv->completed_callback();
    },
        [=](std::unique_ptr<GoogleException> ex) {
        rv->failed_callback(std::move(ex));
    });
    return rv;
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::downloadAttachment_Async(googleQt::mail_cache::msg_ptr m,
    googleQt::mail_cache::att_ptr a,
    QString destinationFolder)
{
    GoogleVoidTask* rv = m_endpoint.produceVoidTask();

    if (a->status() == mail_cache::AttachmentData::statusDownloadInProcess) {
        qWarning() << "attachment download already in progress " << m->id() << a->attachmentId();
    }

    if (!m_lite_storage) {
        qWarning() << "ERROR. Expected storage object.";
        rv->completed_callback();
        return rv;
    }

    QString userId = m_lite_storage->findUser(m->accountId());
    if (userId.isEmpty()) {
        qWarning() << "ERROR. Failed to locate userId" << m->id() << m->accountId();
        rv->completed_callback();
        return rv;
    }

    a->m_status = mail_cache::AttachmentData::statusDownloadInProcess;
    gmail::AttachmentIdArg arg(userId, m->id(), a->attachmentId());
    auto t = m_gmail_routes.getAttachments()->get_Async(arg);
    t->then([=](std::unique_ptr<attachments::MessageAttachment> att)
    {
        QDir dest_dir;
        if (!dest_dir.exists(destinationFolder)) {
            if (!dest_dir.mkpath(destinationFolder)) {
                qWarning() << "Failed to create directory" << destinationFolder;
                return;
            };
        }
        
        QString destFile = destinationFolder + "/" + a->filename();
        if (QFile::exists(destFile)) {
            //create some reasonable unique file name
            QFileInfo fi(destFile);
            QString name = fi.baseName().left(64);
            QString ext = fi.suffix();
            int idx = 1;
            while (idx++ < 1000) {
                destFile = destinationFolder + "/" + name + QString("_%1").arg(idx) + "." + ext;
                if (!QFile::exists(destFile))
                    break;
            }
        }

        QFile file_in(destFile);
        if (file_in.open(QFile::WriteOnly)) {
            file_in.write(QByteArray::fromBase64(att->data(), QByteArray::Base64UrlEncoding));
            file_in.close();

            QFileInfo fi(destFile);
            if (m_lite_storage) {
                m_lite_storage->update_attachment_local_file_db(m, a, fi.fileName());
            }
            emit attachmentsDownloaded(m, a);
        }
        else {
            qWarning() << "Failed to create attachment file" << destFile;
        }
        rv->completed_callback();
    });
    return rv;
};

void mail_cache::GmailCacheRoutes::refreshLabels()
{
    refreshLabels_Async()->waitForResultAndRelease();
};

std::vector<mail_cache::label_ptr> mail_cache::GmailCacheRoutes::getLoadedLabels(std::set<QString>* in_optional_idset)
{
    if (!m_lite_storage) {
        std::vector<mail_cache::label_ptr> on_error;
        return on_error;
    }
    return m_lite_storage->getLabelsInSet(in_optional_idset);
};

std::vector<mail_cache::label_ptr> mail_cache::GmailCacheRoutes::getMessageLabels(mail_cache::MessageData* d)
{
    if (!m_lite_storage) {
        std::vector<mail_cache::label_ptr> on_error;
        return on_error;
    }
    return m_lite_storage->unpackLabels(d->labelsBitMap());
};

std::vector<mail_cache::label_ptr> mail_cache::GmailCacheRoutes::getThreadLabels(mail_cache::ThreadData* d)
{
    if (!m_lite_storage) {
        std::vector<mail_cache::label_ptr> on_error;
        return on_error;
    }
    return m_lite_storage->unpackLabels(d->labelsBitMap());
};

#define TRY_LABEL_MODIFY(F, M, S) bool rv = false;                      \
    try                                                                 \
        {                                                               \
            std::unique_ptr<messages::MessageResource> m = F(M, S)->waitForResultAndRelease(); \
            rv = (!m->id().isEmpty());                                  \
        }                                                               \
    catch (GoogleException& e)                                          \
        {                                                               \
            qWarning() << "setLabel Exception: " << e.what();           \
        }                                                               \
    return rv;                                                          \



bool mail_cache::GmailCacheRoutes::setStarred(mail_cache::MessageData* d, bool set_it)
{
    TRY_LABEL_MODIFY(setStarred_Async, d, set_it);
};

GoogleTask<messages::MessageResource>* mail_cache::GmailCacheRoutes::setStarred_Async(mail_cache::MessageData* d, bool set_it)
{
    return setLabel_Async(mail_cache::sysLabelId(mail_cache::SysLabel::STARRED), d, set_it, true);
};

bool mail_cache::GmailCacheRoutes::setUnread(mail_cache::MessageData* d, bool set_it)
{
    TRY_LABEL_MODIFY(setUnread_Async, d, set_it);
};

GoogleTask<messages::MessageResource>* mail_cache::GmailCacheRoutes::setUnread_Async(mail_cache::MessageData* d, bool set_it)
{
    return setLabel_Async(mail_cache::sysLabelId(mail_cache::SysLabel::UNREAD), d, set_it, true);
};

bool mail_cache::GmailCacheRoutes::setImportant(mail_cache::MessageData* d, bool set_it)
{
    TRY_LABEL_MODIFY(setImportant_Async, d, set_it);
};

GoogleTask<messages::MessageResource>* mail_cache::GmailCacheRoutes::setImportant_Async(mail_cache::MessageData* d, bool set_it)
{
    return setLabel_Async(mail_cache::sysLabelId(mail_cache::SysLabel::IMPORTANT), d, set_it, true);
};


GoogleTask<messages::MessageResource>* mail_cache::GmailCacheRoutes::setLabel_Async(QString label_id,
    mail_cache::MessageData* d,
    bool label_on,
    bool system_label)
{
    int accId = -1;
    if (m_lite_storage) {
        accId = d->accountId();
        if (accId == -1) {
            qWarning() << "ERROR. Invalid account Id" << d->id();
        }
        else {
            mail_cache::LabelData* lbl = m_lite_storage->ensureLabel(accId, label_id, system_label);
            if (!lbl) {
                qWarning() << "failed to create label" << label_id;
            }
            else {
                if (label_on) {
                    d->m_labels |= lbl->labelMask();
                }
                else {
                    d->m_labels &= ~(lbl->labelMask());
                }
            }
        }

        auto t = m_lite_storage->findThread(d->threadId());
        if (t) {
            t->rebuildLabelsMap();
        }

    }

    QString userId = m_lite_storage->findUser(d->accountId());
    if (userId.isEmpty()) {
        qWarning() << "ERROR. Failed to locate userId" << d->id() << d->accountId();
    }

    QString msg_id = d->id();
    gmail::ModifyMessageArg arg(userId, msg_id);
    std::vector <QString> labels;
    labels.push_back(label_id);
    if (label_on)
    {
        arg.setAddlabels(labels);
    }
    else
    {
        arg.setRemovelabels(labels);
    }

    GoogleTask<messages::MessageResource>* t = m_gmail_routes.getMessages()->modify_Async(arg);
    QObject::connect(t, &EndpointRunnable::finished, [=]()
    {
        if (m_GMsgCache &&
            m_lite_storage &&
            accId != -1)
        {
            m_lite_storage->update_message_labels_db(accId, msg_id, d->labelsBitMap());
        }
    });
    return t;
};

bool mail_cache::GmailCacheRoutes::messageHasLabel(mail_cache::MessageData* d, QString label_id)const
{
    bool rv = false;
    if (m_lite_storage) {
        auto lb = m_lite_storage->findLabel(label_id);
        if (lb) {
            rv = d->hasLabel(lb->labelMask());
        }
    }
    return rv;
};

///............
namespace googleQt {
    namespace mail_cache {
        //we can create one label in async context
        class LabelCreator
        {
        public:
            LabelCreator(GmailCacheRoutes& r) :m_r(r) {}
            GoogleTask<QString>* routeRequest(QString label_name)
            {
                GoogleTask<QString>* rv = m_r.endpoint().produceTask<QString>();
                labels::LabelResource arg;
                arg.setName(label_name).setMessagelistvisibility("show").setLabellistvisibility("labelShow");
#ifdef API_QT_AUTOTEST
                ApiAutotest::INSTANCE().addMemberValue("labels::LabelResource", "m_name", label_name);              //ApiAutotest::INSTANCE().addId("labels::LabelResource", thread_id);
#endif
                auto t = m_r.mroutes().getLabels()->create_Async(arg);
                if (t) {
                    QObject::connect(t, &GoogleTask<labels::LabelResource>::finished, [=]()
                    {
                        std::unique_ptr<QString>label_id(new QString);
                        if (t->isCompleted()) {                         
                            auto r = t->get();
                            if (r) {
                                *label_id = r->id();
                                m_r.storage()->insertDbLabel(*r);
                            }
                        };
                        rv->completed_callback(std::move(label_id));
                    });
                }
                return rv;
            };
        protected:
            GmailCacheRoutes&       m_r;
        };//LabelCreator

        //we can delete one label in asyn context
        class LabelDeleter
        {
        public:
            LabelDeleter(GmailCacheRoutes& r) :m_r(r) {}
            GoogleTask<QString>* routeRequest(QString label_id)
            {
                GoogleTask<QString>* rv = m_r.endpoint().produceTask<QString>();

                gmail::IdArg l_id(m_r.endpoint().client()->userId(), label_id);
                auto t = m_r.mroutes().getLabels()->deleteOperation_Async(l_id);
                if (t) {
                    QObject::connect(t, &GoogleTask<labels::LabelResource>::finished, [=]()
                    {
                        std::unique_ptr<QString>label_id_res(new QString);
                        if (t->isCompleted()) {
                            *label_id_res = label_id;
                        }
                        m_r.storage()->deleteDbLabel(label_id);
                        rv->completed_callback(std::move(label_id_res));
                    });
                }
                return rv;
            };
        protected:
            GmailCacheRoutes&       m_r;
        };//LabelDeleter
    }
}

template <class PROCESSOR> googleQt::mail_cache::LabelProcessorTask* mail_cache::GmailCacheRoutes::processLabelList_Async(const STRING_LIST& slist)
{
    LabelProcessorTask* t = new LabelProcessorTask(m_endpoint);
    if (!slist.empty()) {
        std::unique_ptr<PROCESSOR> pr(new PROCESSOR(*this));
        ConcurrentValueRunner<QString, PROCESSOR, QString>* r = new ConcurrentValueRunner<QString, PROCESSOR, QString>(slist, std::move(pr), m_endpoint);
        r->run();
        connect(r, &EndpointRunnable::finished, [=]()
        {
            auto lres = r->detachResult();
            for (const auto& ps : lres) {
                t->m_completed_ids.push_back(*ps);
            }           
            t->completed_callback();
            r->disposeLater();
        });
    }
    else {
        t->completed_callback();
    }
    return t;
};

googleQt::mail_cache::LabelProcessorTask* mail_cache::GmailCacheRoutes::createLabelList_Async(const STRING_LIST& names)
{
    return processLabelList_Async<mail_cache::LabelCreator>(names);
};


GoogleVoidTask* mail_cache::GmailCacheRoutes::deleteLabelList_Async(const STRING_LIST& label_ids)
{
    return processLabelList_Async < mail_cache::LabelDeleter > (label_ids);
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::renameLabels_Async(QString labelId, QString newName)
{
    GoogleVoidTask* rv = m_endpoint.produceVoidTask();
    gmail::IdArg l_id(m_endpoint.client()->userId(), labelId);
    labels::LabelResource lbl;
    lbl.setName(newName).setMessagelistvisibility("show").setLabellistvisibility("labelShow");
    auto t = m_gmail_routes.getLabels()->update_Async(l_id, lbl);
    t->then([=](std::unique_ptr<labels::LabelResource> lr)
    {
        if (m_lite_storage) {
            auto db_lbl = m_lite_storage->findLabel(lr->id());
            if (db_lbl)
            {
                m_lite_storage->updateDbLabel(*lr, db_lbl.get());
            }
            else
            {
                m_lite_storage->insertDbLabel(lbl);
            }

        }
        rv->completed_callback();
    },
    [=](std::unique_ptr<GoogleException> ex) {
        rv->failed_callback(std::move(ex));
    });
    return rv;
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::modifyThreadLabels_Async(thread_ptr t,
    const label_list& labels2add,
    const label_list& labels2remove)
{
    GoogleVoidTask* rv = m_endpoint.produceVoidTask();
    if (t->id().isEmpty() || (labels2add.empty() && labels2remove.empty())) {
        rv->completed_callback();
        return rv;
    }

    googleQt::gmail::ModifyMessageArg arg(m_endpoint.client()->userId(), t->id());
    for (const auto& lb : labels2add) {
        arg.addAddLabel(lb->labelId());
        t->setupLimboLabels(labels2add);
    }
    for (const auto& lb : labels2remove) {
        arg.addRemoveLabel(lb->labelId());
    }

    auto tt = m_gmail_routes.getThreads()->modify_Async(arg);
    tt->then([=](std::unique_ptr<threads::ThreadResource> tr)
    {
        CACHE_LIST<mail_cache::MessageData> new_messages;
        CACHE_LIST<mail_cache::MessageData> updated_messages;

        for (auto& m : tr->messages()) {
            auto m2 = t->findMessage(t->id());
            if (!m2) {
                uint64_t lbits = 0;
                if (m_lite_storage) {
                    lbits = m_lite_storage->packLabels(m.labelids());
                }

                msg_ptr md(new MessageData(storage()->currentAccountId(),
                    m.id(),
                    t->id(), lbits));
                t->add_msg(md);
                new_messages.push_back(md);
            }
            else {
                const std::vector <QString>& labels = m.labelids();
                if (labels.size() > 0) {
                    uint64_t lbits = 0;
                    if (m_lite_storage) {
                        lbits = m_lite_storage->packLabels(labels);
                    }
                    m2->updateLabels(lbits);
                    updated_messages.push_back(m2);
                }
            }
        }

        if (m_lite_storage) {
            if (!new_messages.empty()) {
                m_lite_storage->m_mstorage->insert_db(EDataState::labels, new_messages);
            }
            if (!updated_messages.empty()) {
                m_lite_storage->m_mstorage->update_db(EDataState::labels, updated_messages);
            }
        }

        t->rebuildLabelsMap();
        rv->completed_callback();
    },
        [=](std::unique_ptr<GoogleException> ex) {
        rv->failed_callback(std::move(ex));
    });

    return rv;
};

GoogleVoidTask* mail_cache::GmailCacheRoutes::modifyThreadListLabels_Async(const thread_list& tlist, const label_list& labels2add, const label_list& labels2remove)
{
    class ThreadModifier
    {
    public:
        ThreadModifier(GmailCacheRoutes& r, const label_list& labels2add, const label_list& labels2remove)
            :m_r(r), m_labels2add(labels2add), m_labels2remove(labels2remove) {}
        GoogleVoidTask* routeRequest(thread_ptr t)
        {
            return m_r.modifyThreadLabels_Async(t, m_labels2add, m_labels2remove);
        }
    protected:
        GmailCacheRoutes&     m_r;
        label_list            m_labels2add;
        label_list            m_labels2remove;
    };//LabelDeleter

    GoogleVoidTask* t = m_endpoint.produceVoidTask();
    if (!tlist.empty()) {
        std::unique_ptr<ThreadModifier> pr(new ThreadModifier(*this, labels2add, labels2remove));
        ConcurrentArgRunner<thread_ptr, ThreadModifier>* r = new ConcurrentArgRunner<thread_ptr, ThreadModifier>(tlist, std::move(pr), m_endpoint);
        r->run();
        connect(r, &EndpointRunnable::finished, [=]()
        {
            t->completed_callback();
            r->disposeLater();
        });
    }
    else {
        t->completed_callback();
    }
    return t;
};

///............


#ifdef API_QT_AUTOTEST
void mail_cache::GmailCacheRoutes::autotestThreadDBLoad(const std::vector<HistId>& id_list)
{
    auto* r = getCacheThreadList_Async(id_list);
    auto res = r->waitForResultAndRelease();
    ApiAutotest::INSTANCE() << QString("loaded/cached %1 threads, mem_cache-hit: %2, db-cache-hit: %3")
        .arg(res->result_list.size())
        .arg(r->mem_cache_hit_count())
        .arg(r->db_cache_hit_count());
};

void mail_cache::GmailCacheRoutes::runAutotest()
{
//#define AUTOTEST_SIZE 100
#define AUTOTEST_SIZE 10
#define AUTOTEST_GENERATE_SNIPPET autotestThreadDBLoad(id_list);

    ApiAutotest::INSTANCE() << "start-mail-test";
    ApiAutotest::INSTANCE() << "1";
    ApiAutotest::INSTANCE() << "2";
    ApiAutotest::INSTANCE() << "3";
    ApiAutotest::INSTANCE() << "4";

    auto m = gmail::SendMimeMessageArg::EXAMPLE(0, 0);
    QString rfc822_sample = m->toRfc822();
    ApiAutotest::INSTANCE() << "==== rcf822 ====";
    ApiAutotest::INSTANCE() << rfc822_sample;
    QJsonObject js;
    m->toJson(js);
    ApiAutotest::INSTANCE() << js["raw"].toString();
    ApiAutotest::INSTANCE() << "";

    std::vector<HistId> id_list;
    for (int i = 1; i <= AUTOTEST_SIZE; i++)
    {
        HistId hid;
        hid.id = QString("idR_%1").arg(i);
        hid.hid = 2 * i;
        id_list.push_back(hid);
    };

    ApiAutotest::INSTANCE().enableRequestLog(false);
    ApiAutotest::INSTANCE().enableAttachmentDataGeneration(false);
    ApiAutotest::INSTANCE().enableProgressEmulation(false);
    AUTOTEST_GENERATE_SNIPPET;
    AUTOTEST_GENERATE_SNIPPET;

    m_GMsgCache->mem_clear();
    AUTOTEST_GENERATE_SNIPPET;

    std::function<void(void)>deleteFirst10 = [=]()
    {
        std::set<QString> set2remove;
        for (auto j = id_list.begin(); j != id_list.end(); j++)
        {
            set2remove.insert(j->id);
            if (set2remove.size() >= 10)
                break;
        }

        m_GThreadCache->persistent_clear(set2remove);
    };

    deleteFirst10();
    AUTOTEST_GENERATE_SNIPPET;

    deleteFirst10();
    AUTOTEST_GENERATE_SNIPPET;
    AUTOTEST_GENERATE_SNIPPET;

    int idx = 1;
    bool print_cache = true;
    if (print_cache)
    {
        using MSG_LIST = std::vector<std::shared_ptr<mail_cache::MessageData>>;
        mail_cache::mdata_result lst = mcache()->topCacheData(-1, 0);
        for (auto& i : lst->result_list)
        {
            mail_cache::MessageData* m = i.get();
            QString s = QString("%1. %2 %3").arg(idx).arg(m->id()).arg(m->snippet());
            ApiAutotest::INSTANCE() << s;
            idx++;
            if (idx > 20)
            {
                ApiAutotest::INSTANCE() << "...";
                break;
            }
        }
        QString s = QString("Total messages %1").arg(lst->result_list.size());
        ApiAutotest::INSTANCE() << s;
    }

    qDebug() << "check latest emails";
    bool check_email = true;
    if (check_email)
    {
        idx = 1;
        auto lst2 = getNextCacheThreads();
        for (auto& i : lst2->result_list)
        {
            auto mlst = i->messages();
            for (auto& j : mlst)
            {
                auto m = j.get();
                QString s = QString("%1. %2 %3").arg(idx).arg(m->id()).arg(m->snippet());
                ApiAutotest::INSTANCE() << s;
                idx++;
            }

            QString s = QString("Next(new) threads %1").arg(lst2->result_list.size());
            ApiAutotest::INSTANCE() << s;
        }
    }

    //....
    qDebug() << "check Q cache";
    bool check_q = true;
    if (check_q)
    {
        idx = 1;

        STRING_LIST qfilters;
        qfilters.push_back("from:mike@gmail.com");
        qfilters.push_back("from:judy@gmail.com");
        qfilters.push_back("from:john@gmail.com");
        qfilters.push_back("from:mary@gmail.com");

        for (auto& q_str : qfilters) {
            auto q1 = m_lite_storage->m_qstorage->ensure_q(q_str, "");
            if (q1) {
                auto lst2 = getQCache(q1);
                for (auto& i : lst2->result_list) {
                    auto mlst = i->messages();
                    for (auto& j : mlst)
                    {
                        auto m = j.get();
                        QString s = QString("%1. %2 %3").arg(idx).arg(m->id()).arg(m->snippet());
                        ApiAutotest::INSTANCE() << s;
                        idx++;
                    }

                    QString s = QString("Next(new) Q/threads %1").arg(lst2->result_list.size());
                    ApiAutotest::INSTANCE() << s;
                }
            }
        }
    }
    //...

    bool randomize_labels = true;
    if (randomize_labels)
    {
        auto labels = getLoadedLabels();

        if (labels.size() > 0)
        {
            std::set<QString> setG;
            setG.insert("INBOX");
            setG.insert("SENT");
            setG.insert("STARRED");
            setG.insert("IMPORTANT");
            setG.insert("DRAFT");
            setG.insert("TRASH");
            auto imperative_groups = getLoadedLabels(&setG);


            auto l_iterator = labels.begin();
            auto g_iterator = imperative_groups.begin();
            uint64_t lmask = (*l_iterator)->labelMask();
            uint64_t gmask = (*g_iterator)->labelMask();
            auto lst = tcache()->topCacheData(-1, 0);
            int counter = 0;
            int totalNumber = lst->result_list.size();
            int imperative_group_size = totalNumber / imperative_groups.size();
            if (imperative_group_size == 0)
                imperative_group_size = 1;
            qDebug() << QString("Resetting labels: %1 on %2 threads, with imperative group size %3")
                .arg(labels.size())
                .arg(totalNumber)
                .arg(imperative_group_size);
            for (auto& i : lst->result_list)
            {
                auto result_mask = lmask | gmask;
                auto t = i.get();
                auto mlst = t->messages();
                QString s = QString("label-down thread '%1' with %2 messages to %3")
                            .arg(t->id())
                            .arg(mlst.size())
                            .arg(result_mask);
                ApiAutotest::INSTANCE() << s;

                for (auto& j : mlst) {
                    mail_cache::MessageData* m = j.get();
                    m_lite_storage->update_message_labels_db(m_lite_storage->m_accId, m->id(), result_mask);
                }

                counter++;
                l_iterator++; if (l_iterator == labels.end())l_iterator = labels.begin();
                lmask = (*l_iterator)->labelMask();

                if (counter > imperative_group_size) {
                    counter = 0;

                    g_iterator++; if (g_iterator == imperative_groups.end())g_iterator = imperative_groups.begin();
                    gmask = (*g_iterator)->labelMask();

                    qDebug() << QString("Next Label group: %1/%2")
                        .arg((*g_iterator)->labelName())
                        .arg(gmask);
                }

                //qDdebug() << "updating thread";
                /*
                mail_cache::MessageData* m = i.get();
                m_lite_storage->update_message_labels_db(m_lite_storage->m_accId, m->id(), lmask | gmask);

                counter++;
                l_iterator++; if (l_iterator == labels.end())l_iterator = labels.begin();
                lmask = (*l_iterator)->labelMask();

                if (counter > imperative_group_size) {
                    counter = 0;

                    g_iterator++; if (g_iterator == imperative_groups.end())g_iterator = imperative_groups.begin();
                    gmask = (*g_iterator)->labelMask();

                    qDebug() << QString("Next Label group: %1/%2")
                        .arg((*g_iterator)->labelName())
                        .arg(gmask);
                }
                */
            }
        }
    }


    qDebug() << "check latest threads";
    bool check_thread = true;
    if (check_thread)
    {
        idx = 1;
        auto lst2 = getNextCacheThreads();
        for (auto& i : lst2->result_list)
        {
            auto* t = i.get();
            QString s = QString("%1. %2 %3 %4")
                .arg(idx)
                .arg(t->id())
                .arg(t->historyId())
                .arg(t->snippet());
            ApiAutotest::INSTANCE() << s;
            idx++;
        }
        QString s = QString("Next(new) threads %1").arg(lst2->result_list.size());
        ApiAutotest::INSTANCE() << s;
    }

    ApiAutotest::INSTANCE().enableProgressEmulation(true);
    ApiAutotest::INSTANCE().enableAttachmentDataGeneration(true);
    ApiAutotest::INSTANCE().enableRequestLog(true);

#undef AUTOTEST_SIZE
};

#endif //API_QT_AUTOTEST
