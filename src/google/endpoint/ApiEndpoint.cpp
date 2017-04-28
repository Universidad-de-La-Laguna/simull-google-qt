#include <iostream>
#include <QBuffer>
#include "ApiEndpoint.h"

#ifdef API_QT_AUTOTEST
#include "ApiAutotest.h"
#define LOG_REQUEST     ApiAutotest::INSTANCE().logRequest(m_last_req_info);
#endif

using namespace googleQt;

ApiEndpoint::ApiEndpoint(ApiClient* c):m_client(c)
{

}

void ApiEndpoint::addAuthHeader(QNetworkRequest& request)
{
#define DEF_USER_AGENT "googleQtC++11-Client"

    QString bearer = QString("Bearer %1").arg(m_client->getToken());
    request.setRawHeader("Authorization", bearer.toUtf8());
    request.setRawHeader("Host", "www.googleapis.com");
    QString user_agent = DEF_USER_AGENT;
    if(m_client && !m_client->userAgent().isEmpty()){
        user_agent = QString("%1 %2")
            .arg(m_client->userAgent())
            .arg(DEF_USER_AGENT);
    }
    request.setRawHeader("User-Agent", user_agent.toUtf8());

#undef DEF_USER_AGENT
};

void ApiEndpoint::runEventsLoop()const
{
    m_loop.exec();
};

void ApiEndpoint::exitEventsLoop()const
{
    m_loop.exit();
};


void ApiEndpoint::setProxy(const QNetworkProxy& proxy)
{
    m_con_mgr.setProxy(proxy);
};

void ApiEndpoint::cancelAll()
{
    NET_REPLIES_IN_PROGRESS copy_of_replies = m_replies_in_progress;
    std::for_each(copy_of_replies.begin(), copy_of_replies.end(), [](std::pair<QNetworkReply*, std::shared_ptr<FINISHED_REQ>> p)
    {
        p.first->abort();
    });
};

void ApiEndpoint::registerReply(std::shared_ptr<requester>& rb, QNetworkReply* r, std::shared_ptr<FINISHED_REQ> finishedLambda)
{
    Q_UNUSED(rb);

    QObject::connect(r, &QNetworkReply::downloadProgress, [&](qint64 bytesProcessed, qint64 total) {
        emit m_client->downloadProgress(bytesProcessed, total);
    });

    QObject::connect(r, &QNetworkReply::uploadProgress, [&](qint64 bytesProcessed, qint64 total) {
        emit m_client->uploadProgress(bytesProcessed, total);
    });

    NET_REPLIES_IN_PROGRESS::iterator i = m_replies_in_progress.find(r);
    if (i != m_replies_in_progress.end())
    {
        qWarning() << "Duplicate reply objects registered, map size:" << m_replies_in_progress.size() << r;
    }

    m_replies_in_progress[r] = finishedLambda;
};

void ApiEndpoint::unregisterReply(QNetworkReply* r)
{
    NET_REPLIES_IN_PROGRESS::iterator i = m_replies_in_progress.find(r);
    if (i == m_replies_in_progress.end())
    {
        qWarning() << "Failed to locate reply objects in registered map, map size:" << m_replies_in_progress.size() << r;
    }
    else
    {
        m_replies_in_progress.erase(i);
    }
    r->deleteLater();
};


void ApiEndpoint::updateLastRequestInfo(QString s)
{
    m_last_req_info = s;
};

QNetworkReply* ApiEndpoint::getData(const QNetworkRequest &req)
{
    QString rinfo = "GET " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for(QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());

    updateLastRequestInfo(rinfo);
    
#ifdef API_QT_AUTOTEST
    LOG_REQUEST;
    return nullptr;
#else
    QNetworkReply *r = m_con_mgr.get(req);
    return r;
#endif
};

QNetworkReply* ApiEndpoint::postData(const QNetworkRequest &req, const QByteArray& data)
{
    QString rinfo = "POST " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for(QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());
    rinfo += QString("--data %1").arg(data.constData());

    /*
    std::cout << "hexl-POST =====" << std::endl;
    std::string hstr = QString(data.toHex()).toStdString();
    int len = hstr.length();
    int idx = 0;
    while(idx < len)
        {
            for(int i = 0; i < 32; i+= 2)
                {
                    std::cout << hstr[idx] << hstr[idx+1] << " ";
                    idx += 2;
                    if(idx >= len)break;                    
                }
            std::cout << std::endl;
        }
    std::cout << std::endl;
    std::cout << "hexl-POST =====" << std::endl;
    */ 

    updateLastRequestInfo(rinfo);
    
#ifdef API_QT_AUTOTEST
    LOG_REQUEST;
    return nullptr;
#else
    QNetworkReply *r = m_con_mgr.post(req, data);
    return r;
#endif
};

QNetworkReply* ApiEndpoint::postData(const QNetworkRequest &req, QHttpMultiPart* mpart)
{
    QString rinfo = "POST " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for (QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());
    rinfo += QString("--data %1").arg("multipart");

    updateLastRequestInfo(rinfo);

#ifdef API_QT_AUTOTEST
    Q_UNUSED(mpart);
    LOG_REQUEST;
    return nullptr;
#else
    QNetworkReply *r = m_con_mgr.post(req, mpart);
    return r;
#endif
};

QNetworkReply* ApiEndpoint::putData(const QNetworkRequest &req, const QByteArray& data)
{    
    QString rinfo = "PUT " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for(QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());
    rinfo += QString("--data %1").arg(data.constData());
    updateLastRequestInfo(rinfo);
    
#ifdef API_QT_AUTOTEST
    LOG_REQUEST;
    return nullptr;
#else
    QNetworkReply *r = m_con_mgr.put(req, data);
    return r;
#endif
};

QNetworkReply* ApiEndpoint::deleteData(const QNetworkRequest &req)
{    
    QString rinfo = "DELETE " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for(QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());

    updateLastRequestInfo(rinfo);
    
#ifdef API_QT_AUTOTEST
    LOG_REQUEST;
    return nullptr;
#else
    QNetworkReply *r = m_con_mgr.deleteResource(req);
    return r;
#endif
};

QNetworkReply* ApiEndpoint::patchData(const QNetworkRequest &req, const QByteArray& data)
{
    QString rinfo = "PATCH " + req.url().toString() + "\n";
    QList<QByteArray> lst = req.rawHeaderList();
    for(QList<QByteArray>::iterator i = lst.begin(); i != lst.end(); i++)
        rinfo += QString("--header %1 : %2 \n").arg(i->constData()).arg(req.rawHeader(*i).constData());
    rinfo += QString("--data %1").arg(data.constData());

    updateLastRequestInfo(rinfo);
#ifdef API_QT_AUTOTEST
    LOG_REQUEST;
    return nullptr;
#else
    QBuffer *buf = new QBuffer();
    buf->setData(data);
    buf->open(QBuffer::ReadOnly);
    QNetworkReply *r = m_con_mgr.sendCustomRequest(req, "PATCH", buf);
    buf->setParent(r);
    return r;
#endif
};


///MPartUpload_requester
QNetworkReply* ApiEndpoint::MPartUpload_requester::request(QNetworkRequest& r)
{
    QByteArray meta_bytes;
    QJsonDocument doc(m_js_out);
    if (m_js_out.isEmpty()) {
        meta_bytes.append("null");
    }
    else {
        QJsonDocument doc(m_js_out);
        meta_bytes = doc.toJson(QJsonDocument::Compact);
    }

    /*
      QHttpMultiPart *mpart = new QHttpMultiPart(QHttpMultiPart::RelatedType);

      QHttpPart metaPart;
      metaPart.setRawHeader("Content-Type", "application/json; charset = UTF-8");
      metaPart.setBody(meta_bytes);

      QHttpPart dataPart;
      dataPart.setRawHeader("Content-Type", "application/octet-stream");
      dataPart.setRawHeader("Content-Transfer-Encoding", "base64");
      if(m_readFrom){
      dataPart.setBody(m_readFrom->readAll().toBase64(QByteArray::Base64UrlEncoding));
      }

      mpart->append(metaPart);
      mpart->append(dataPart);

      QNetworkReply* reply = m_ep.postData(r, mpart);
      mpart->setParent(reply);
      return reply;
    */

                
    QString delimiter("OooOOoo17gqt");
    QByteArray bytes2post = QString("--%1\r\n").arg(delimiter).toStdString().c_str();
    bytes2post += QString("Content-Type: application/json; charset=UTF-8\r\n").toStdString().c_str();             
    bytes2post += "\r\n";
    bytes2post += meta_bytes;
                
    bytes2post += "\r\n";
    bytes2post += QString("--%1\r\n").arg(delimiter).toStdString().c_str();
    bytes2post += QString("Content-Type: application/octet-stream\r\n").toStdString().c_str();
    bytes2post += QString("Content-Transfer-Encoding: base64\r\n\r\n").toStdString().c_str();
    //bytes2post += "\r\n";
    if (m_readFrom) {
        //        bytes2post += m_readFrom->readAll().toBase64(QByteArray::Base64UrlEncoding);
        bytes2post += m_readFrom->readAll().toBase64(QByteArray::Base64Encoding);
    }
    bytes2post += "\r\n";
    bytes2post += QString("--%1--\r\n").arg(delimiter).toStdString().c_str();

    QString content_str = QString("multipart/related; boundary=%1").arg(delimiter);
    r.setRawHeader("Content-Type", content_str.toStdString().c_str());
    r.setRawHeader("Content-Length", QString("%1").arg(bytes2post.size()).toStdString().c_str());
    return m_ep.postData(r, bytes2post);
}
