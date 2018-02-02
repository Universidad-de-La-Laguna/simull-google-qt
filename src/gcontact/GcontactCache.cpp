#include <QSqlError>
#include <QDebug>
#include <ctime>
#include <QDir>
#include <ostream>

#include "GcontactCache.h"
#include "GcontactRoutes.h"
#include "Endpoint.h"
#include "gmail/GmailCache.h"

using namespace googleQt;
using namespace gcontact;

#define COMPARE_NO_CASE(N) if (N.compare(o.N, Qt::CaseInsensitive)){return false;}
#define CONFIG_SYNC_TIME "sync-time"

/**
   NameInfo
*/
NameInfo::NameInfo() 
{
};

bool NameInfo::isEmpty()const 
{
    if (isNull())
        return false;
    bool rv = m_fullName.isEmpty() && m_givenName.isEmpty() && m_familyName.isEmpty();
    return rv;
}

NameInfo NameInfo::parse(QDomNode n)
{
    NameInfo rv;        
    rv.m_is_null = true;
    QDomElement elem_name = n.firstChildElement("gd:name");
    if (!elem_name.isNull()) {
        rv.m_is_null = false;
        rv.m_fullName = elem_name.firstChildElement("gd:fullName").text().trimmed();
        rv.m_givenName = elem_name.firstChildElement("gd:givenName").text().trimmed();
        rv.m_familyName = elem_name.firstChildElement("gd:familyName").text().trimmed();
    }
    return rv;
}

void NameInfo::toXmlDoc(QDomDocument& doc, QDomNode& entry_node)const
{
    xml_util::removeNodes(entry_node, "gd:name");
    QDomElement orga_node = xml_util::ensureNode(doc, entry_node, "gd:name");
    xml_util::updateNode(doc, orga_node, "gd:fullName", m_fullName);
    xml_util::updateNode(doc, orga_node, "gd:givenName", m_givenName);
    xml_util::updateNode(doc, orga_node, "gd:familyName", m_familyName);
};


QString NameInfo::toString()const
{
    QString s = "";
    if (!isNull()) {
        s = QString("name=%1,%2,%3").arg(fullName()).arg(givenName()).arg(familyName());
    }
    return s;
};

bool NameInfo::operator==(const NameInfo& o) const 
{
    COMPARE_NO_CASE(m_fullName);
    COMPARE_NO_CASE(m_givenName);
    COMPARE_NO_CASE(m_familyName);
    return true;
};

bool NameInfo::operator!=(const NameInfo& o) const 
{
    return !(*this == o);
};

/**
    OrganizationInfo
*/
OrganizationInfo::OrganizationInfo():m_type_label("other")
{
};

bool OrganizationInfo::isEmpty()const
{
    if (isNull())
        return false;
    bool rv = m_name.isEmpty() && m_title.isEmpty();
    return rv;
}


OrganizationInfo OrganizationInfo::parse(QDomNode n)
{
    OrganizationInfo rv;
    rv.m_is_null = true;
    QDomElement elem_organization = n.firstChildElement("gd:organization");
    if (!elem_organization.isNull()) {
        rv.m_is_null = false;
        rv.m_name = elem_organization.firstChildElement("gd:orgName").text().trimmed();
        rv.m_title = elem_organization.firstChildElement("gd:orgTitle").text().trimmed();


        QDomNamedNodeMap attr_names = elem_organization.attributes();
        if (attr_names.size() > 0) {
            for (int j = 0; j < attr_names.size(); j++) {
                QDomNode n2 = attr_names.item(j);
                if (n2.nodeType() == QDomNode::AttributeNode) {
                    if (n2.nodeName().compare("rel") == 0) {
                        QStringList slist = n2.nodeValue().trimmed().split("#");
                        if (slist.size() == 2) {
                            rv.m_type_label = slist[1];
                        }
                    }
                }
            }
        }
    }
    return rv;
}

QString OrganizationInfo::toString()const 
{
    QString s = "";
    if (!isNull()) {
        s = QString("organization=%1,%2").arg(name()).arg(title());
    }
    return s;
};

QString OrganizationInfo::toXmlString()const 
{
    QString s = "";
    if (!isNull()) {
        s += QString("<gd:organization rel = \"http://schemas.google.com/g/2005#%1\">\n").arg(m_type_label);
        s += QString("    <gd:orgName>%1</gd:orgName>\n").arg(m_name);
        s += QString("    <gd:orgTitle>%1</gd:orgTitle>\n").arg(m_title);
        s += "</gd:organization>\n";
    }
    return s;
};

void OrganizationInfo::toXmlDoc(QDomDocument& doc, QDomNode& entry_node)const 
{
    xml_util::removeNodes(entry_node, "gd:organization");
    QDomElement orga_node = xml_util::ensureNode(doc, entry_node, "gd:organization");
    xml_util::updateNode(doc, orga_node, "gd:orgName", m_name);
    xml_util::updateNode(doc, orga_node, "gd:orgTitle", m_title);

    orga_node.setAttribute("rel", QString("http://schemas.google.com/g/2005#%1").arg(m_type_label));
};

bool OrganizationInfo::operator==(const OrganizationInfo& o) const
{
    COMPARE_NO_CASE(m_name);
    COMPARE_NO_CASE(m_title);
    return true;
};

bool OrganizationInfo::operator!=(const OrganizationInfo& o) const
{
    return !(*this == o);
};


/**
PostalAddress
*/
PostalAddress::PostalAddress() :m_type_label("home")
{
};

QString PostalAddress::toString()const
{
    QString s = "";
    if (!isNull()) {
        s = QString("postalAddress=%1 %2 %3 %4 %5")
            .arg(m_city)
            .arg(m_street)
            .arg(m_region)
            .arg(m_postcode)
            .arg(m_country);
    }
    return s;
};

bool PostalAddress::operator==(const PostalAddress& o) const
{
    COMPARE_NO_CASE(m_city);
    COMPARE_NO_CASE(m_street);
    COMPARE_NO_CASE(m_region);
    COMPARE_NO_CASE(m_postcode);
    COMPARE_NO_CASE(m_country);
    COMPARE_NO_CASE(m_type_label);
    COMPARE_NO_CASE(m_formattedAddress);
    if (m_is_primary != o.m_is_primary)
        return false;
    return true;
};

bool PostalAddress::operator!=(const PostalAddress& o) const
{
    return !(*this == o);
};


/**
    PhoneInfo
*/
PhoneInfo::PhoneInfo() :m_type_label("other")
{
};

QString PhoneInfo::toString()const 
{
    QString s = "";
    if (!isNull()) {
        s = QString("phone=%1,%2,%3").arg(number()).arg(uri()).arg(typeLabel());
        if (m_is_primary) {
            s += ", primary=\"true\"";
        }
    }
    return s;
};


bool PhoneInfo::operator==(const PhoneInfo& o) const 
{
    COMPARE_NO_CASE(m_number);
    COMPARE_NO_CASE(m_uri);
    COMPARE_NO_CASE(m_type_label);
    if (m_is_primary != o.m_is_primary)
        return false;
    return true;

};

bool PhoneInfo::operator!=(const PhoneInfo& o) const 
{
    return !(*this == o);
};


/**
    EmailInfo
*/
EmailInfo::EmailInfo() :m_type_label("other")
{

};

QString EmailInfo::toString()const
{
    QString s = "";
    if (!isNull()) {
        s = QString("email=%1,%2,%3").arg(address()).arg(displayName()).arg(typeLabel());
        if (m_is_primary) {
            s += ", primary=\"true\"";
        }
    }
    return s;
};

bool EmailInfo::operator==(const EmailInfo& o) const
{
    COMPARE_NO_CASE(m_address);
    COMPARE_NO_CASE(m_display_name);
    COMPARE_NO_CASE(m_type_label);
    if (m_is_primary != o.m_is_primary)
        return false;
    return true;
};

bool EmailInfo::operator!=(const EmailInfo& o) const
{
    return !(*this == o);
};

/**
    EmailInfoList
*/
EmailInfoList EmailInfoList::parse(QDomNode n)
{
    EmailInfoList rv;    
    QDomElement first_email_elem = n.firstChildElement("gd:email");
    QDomElement email_elem = first_email_elem;
    while (!email_elem.isNull()) {
        EmailInfo email_info;
        email_info.m_is_null = false;
        QDomNamedNodeMap attr_names = email_elem.attributes();
        if (attr_names.size() > 0) {
            for (int j = 0; j < attr_names.size(); j++) {
                QDomNode n2 = attr_names.item(j);
                if (n2.nodeType() == QDomNode::AttributeNode) {
                    if (n2.nodeName().compare("address") == 0) {
                        email_info.m_address = n2.nodeValue().trimmed();
                    }
                    else  if (n2.nodeName().compare("displayName") == 0) {
                        email_info.m_display_name = n2.nodeValue().trimmed();
                    }
                    else if (n2.nodeName().compare("primary") == 0) {
                        QString s = n2.nodeValue().trimmed();
                        email_info.m_is_primary = (s.indexOf("true") != -1);
                    }
                    else if (n2.nodeName().compare("rel") == 0) {
                        QStringList slist = n2.nodeValue().split("#");
                        if (slist.size() == 2) {
                            email_info.m_type_label = slist[1];
                        }
                    }
                }
            }
        }

        rv.m_parts.push_back(email_info);
        email_elem = email_elem.nextSiblingElement("gd:email");
    }

    return rv;
}

QString EmailInfoList::toXmlString()const
{
    QString s = "";
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QString s_is_primary = "";
            QString displayName = "";
            QString address = "";
            if (!p.displayName().isEmpty()) {
                displayName = QString(" displayName=\"%1\"").arg(p.displayName());
            }
            if (p.isPrimary()) {
                s_is_primary = " primary=\"true\"";
            }
            address = QString(" address=\"%1\"").arg(p.address());
            s += QString("<gd:email rel = \"http://schemas.google.com/g/2005#%1\"%2 %3%4/>\n")
                .arg(p.m_type_label)
                .arg(s_is_primary)
                .arg(address)
                .arg(displayName);
        }
    }
    return s;
};

void EmailInfoList::toXmlDoc(QDomDocument& doc, QDomNode& entry_node)const
{
    xml_util::removeNodes(entry_node, "gd:email");
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QDomElement phone_node = xml_util::addNode(doc, entry_node, "gd:email");
            if (p.m_is_primary) {
                phone_node.setAttribute("primary", "true");
            }
            if (!p.displayName().isEmpty()) {
                phone_node.setAttribute("displayName", p.displayName());
            }
            phone_node.setAttribute("address", p.address());
            phone_node.setAttribute("rel", QString("http://schemas.google.com/g/2005#%1").arg(p.m_type_label));
        }
    }
};


/**
    PhoneInfoList
*/
PhoneInfoList PhoneInfoList::parse(QDomNode n)
{
    PhoneInfoList rv;    
    QDomElement first_phone_elem = n.firstChildElement("gd:phoneNumber");
    QDomElement phone_elem = first_phone_elem;
    while (!phone_elem.isNull()) {
        PhoneInfo phone_info;
        phone_info.m_number = phone_elem.text().trimmed();
        phone_info.m_is_null = false;
        QDomNamedNodeMap attr_names = phone_elem.attributes();
        if (attr_names.size() > 0) {
            for (int j = 0; j < attr_names.size(); j++) {
                QDomNode n2 = attr_names.item(j);
                if (n2.nodeType() == QDomNode::AttributeNode) {
                    if (n2.nodeName().compare("uri") == 0) {
                        phone_info.m_uri = n2.nodeValue().trimmed();
                    }
                    else if (n2.nodeName().compare("primary") == 0) {
                        QString s = n2.nodeValue().trimmed();
                        phone_info.m_is_primary = (s.indexOf("true") != -1);
                    }
                    else if (n2.nodeName().compare("rel") == 0) {
                        QStringList slist = n2.nodeValue().trimmed().split("#");
                        if (slist.size() == 2) {
                            phone_info.m_type_label = slist[1];
                        }
                    }
                }
            }
        }

        rv.m_parts.push_back(phone_info);
        phone_elem = phone_elem.nextSiblingElement("gd:phoneNumber");
    }

    return rv;
}

QString PhoneInfoList::toXmlString()const 
{
    QString s = "";
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QString s_is_primary = "";
            if (p.m_is_primary) {
                s_is_primary = " primary=\"true\"";
            }
            s += QString("<gd:phoneNumber rel = \"http://schemas.google.com/g/2005#%1\"%2>\n").arg(p.m_type_label).arg(s_is_primary);
            s += QString("    %1\n").arg(p.m_number);
            s += "</gd:phoneNumber>\n";
        }
    }
    return s;
};

void PhoneInfoList::toXmlDoc(QDomDocument& doc, QDomNode& entry_node)const
{
    xml_util::removeNodes(entry_node, "gd:phoneNumber");
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QDomElement phone_node = xml_util::addNode(doc, entry_node, "gd:phoneNumber");
            if (p.m_is_primary) {
                phone_node.setAttribute("primary", "true");
            }
            if (!p.uri().isEmpty()) {
                phone_node.setAttribute("uri", p.uri());
            }
            phone_node.setAttribute("rel", QString("http://schemas.google.com/g/2005#%1").arg(p.m_type_label));
            xml_util::addText(doc, phone_node, p.number());
            //QDomText tn = doc.createTextNode(QString(p.number()));
            //phone_node.appendChild(tn);
        }
    }
};


/**
PostalAddressList
*/
PostalAddressList PostalAddressList::parse(QDomNode n)
{
    PostalAddressList rv;
    
    QDomElement first_address_elem = n.firstChildElement("gd:structuredPostalAddress");
    QDomElement address_elem = first_address_elem;
    while (!address_elem.isNull()) {
        PostalAddress address;
        address.m_is_null = true;

        QDomNamedNodeMap attr_names = address_elem.attributes();
        if (attr_names.size() > 0) {
            for (int j = 0; j < attr_names.size(); j++) {
                QDomNode n2 = attr_names.item(j);
                if (n2.nodeType() == QDomNode::AttributeNode) {
                    if (n2.nodeName().compare("rel") == 0) {
                        QStringList slist = n2.nodeValue().trimmed().split("#");
                        if (slist.size() == 2) {
                            address.m_type_label = slist[1];
                        }
                    }
                    else if (n2.nodeName().compare("primary") == 0) {
                        QString s = n2.nodeValue().trimmed();
                        address.m_is_primary = (s.indexOf("true") != -1);
                    }
                }
            }
        }

        address.m_is_null = false;
        address.m_city = address_elem.firstChildElement("gd:city").text().trimmed();
        address.m_street = address_elem.firstChildElement("gd:street").text().trimmed();
        address.m_region = address_elem.firstChildElement("gd:region").text().trimmed();
        address.m_postcode = address_elem.firstChildElement("gd:postcode").text().trimmed();
        address.m_country = address_elem.firstChildElement("gd:country").text().trimmed();
        address.m_formattedAddress = address_elem.firstChildElement("gd:formattedAddress").text().trimmed();


        rv.m_parts.push_back(address);
        address_elem = address_elem.nextSiblingElement("gd:structuredPostalAddress");
    }

    return rv;
}

QString PostalAddressList::toXmlString()const 
{
    QString s = "";
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QString s_is_primary = "";
            if (p.isPrimary()) {
                s_is_primary = " primary=\"true\"";
            }
            s += QString("<gd:structuredPostalAddress rel = \"http://schemas.google.com/g/2005#%1\"%2>\n")
                .arg(p.m_type_label)
                .arg(s_is_primary);

            s += QString("    <gd:city>%1</gd:city>\n").arg(p.m_city);
            s += QString("    <gd:street>%1</gd:street>\n").arg(p.m_street);
            s += QString("    <gd:region>%1</gd:region>\n").arg(p.m_region);
            s += QString("    <gd:postcode>%1</gd:postcode>\n").arg(p.m_postcode);
            s += QString("    <gd:country>%1</gd:country>\n").arg(p.m_country);
            s += QString("    <gd:formattedAddress>%1</gd:formattedAddress>\n").arg(p.m_formattedAddress);

            s += "</gd:structuredPostalAddress>\n";
        }
    }
    return s;
};

void PostalAddressList::toXmlDoc(QDomDocument& doc, QDomNode& entry_node)const
{
    xml_util::removeNodes(entry_node, "gd:structuredPostalAddress");
    for (auto& p : m_parts) {
        if (!p.isNull()) {
            QDomElement addr_node = xml_util::addNode(doc, entry_node, "gd:structuredPostalAddress");
            if (p.m_is_primary) {
                addr_node.setAttribute("primary", "true");
            }
            addr_node.setAttribute("rel", QString("http://schemas.google.com/g/2005#%1").arg(p.m_type_label));
            xml_util::updateNode(doc, addr_node, "gd:city", p.m_city);
            xml_util::updateNode(doc, addr_node, "gd:street", p.m_street);
            xml_util::updateNode(doc, addr_node, "gd:region", p.m_region);
            xml_util::updateNode(doc, addr_node, "gd:postcode", p.m_postcode);
            xml_util::updateNode(doc, addr_node, "gd:country", p.m_country);
            xml_util::updateNode(doc, addr_node, "gd:formattedAddress", p.m_formattedAddress);
        }
    }
};

static QString getAttribute(const QDomNode& n, QString name)
{
    QString rv = "";

    QDomNamedNodeMap attr_names = n.attributes();
    for (int j = 0; j < attr_names.size(); j++) {
        QDomNode n2 = attr_names.item(j);
        if (n2.nodeType() == QDomNode::AttributeNode &&
            n2.nodeName().compare(name) == 0) {
            rv = n2.nodeValue().trimmed();
            break;
        }
    }

    rv = rv.remove("\"");

    return rv;
}


/**
    ContactInfo
*/
ContactInfo* ContactInfo::createWithId(QString contact_id) 
{
    ContactInfo* rv = new ContactInfo();
    rv->m_id = contact_id;
    return rv;
};

ContactInfo& ContactInfo::setTitle(QString val)
{
    m_title = val;
    return *this;
};

ContactInfo& ContactInfo::setContent(QString notes) 
{
    m_content = notes;
    return *this;
};

ContactInfo& ContactInfo::setName(const NameInfo& n)
{
    m_name = n;
    return *this;
};

ContactInfo& ContactInfo::addEmail(const EmailInfo& e)
{
    m_emails.m_parts.push_back(e);
    return *this;
};

ContactInfo& ContactInfo::addPhone(const PhoneInfo& p) 
{
    m_phones.m_parts.push_back(p);
    return *this;
};

ContactInfo& ContactInfo::replaceEmails(const std::list<EmailInfo>& lst)
{
    m_emails.m_parts.clear();
    for (auto& p : lst) {
        m_emails.m_parts.push_back(p);
    }
    return *this;
};

ContactInfo& ContactInfo::replacePhones(const std::list<PhoneInfo>& lst)
{
    m_phones.m_parts.clear();
    for (auto& p : lst) {
        m_phones.m_parts.push_back(p);
    }
    return *this;
};


ContactInfo& ContactInfo::addAddress(const PostalAddress& p)
{
    m_address_list.m_parts.push_back(p);
    return *this;
};

ContactInfo& ContactInfo::replaceAddressList(const std::list<PostalAddress>& lst)
{
    m_address_list.m_parts.clear();
    for (auto& p : lst) {
        m_address_list.m_parts.push_back(p);
    }
    return *this;
};


ContactInfo& ContactInfo::setOrganizationInfo(const OrganizationInfo& o) 
{
    m_organization = o;
    return *this;
};

QString ContactInfo::toString()const 
{
    QString s = "";
    s += "id=" + m_id + ";etag=" + m_etag + ";updated=" + m_updated.toString(Qt::ISODate)
        + ";title=" + m_title + "content=" + m_content + ";"
        + m_name.toString() + ";" + m_emails.toString() + ";" + m_phones.toString() + ";" + m_organization.toString() + ";" + m_address_list.toString();
    return s;
};


bool ContactInfo::operator == (const ContactInfo& o) const
{
    ///do not check on m_original_xml_string - it's for cache storage

    COMPARE_NO_CASE(m_etag);
    COMPARE_NO_CASE(m_id);
    COMPARE_NO_CASE(m_title);
    COMPARE_NO_CASE(m_content);

    if (m_updated != o.m_updated) {
        return false;
    }

    if (m_name != o.m_name) {
        return false;
    }

    if (m_organization != o.m_organization) {
        return false;
    }

    if (m_emails != o.m_emails) {
        return false;
    }

    if (m_phones != o.m_phones) {
        return false;
    }

    if (m_address_list != o.m_address_list) {
        return false;
    }

    return true;
};

bool ContactInfo::operator!=(const ContactInfo& o) const 
{
    return !(*this == o);
};

QString ContactInfo::toXml(QString userEmail)const 
{
    QString rv;
    /*
    if (m_title.isEmpty() && m_name.isEmpty() && m_etag.isEmpty()) {
        rv = QString("<entry>\n");
    }
    else {
        rv = QString("<entry xmlns:atom=\"http://www.w3.org/2005/Atom\" xmlns:gd = \"http://schemas.google.com/g/2005\" gd:etag=\"%1\"> <atom:category scheme = \"http://schemas.google.com/g/2005#kind\" term = \"http://schemas.google.com/contact/2008#contact\"/>\n")
            .arg(m_etag);
    }

    if (m_batch_id != googleQt::EBatchId::none) {
        rv += batch2xml(m_batch_id);
        rv += "\n";
    }
    */

    if (m_batch_id != googleQt::EBatchId::none) {
        switch(m_batch_id){
        case googleQt::EBatchId::update:
        case googleQt::EBatchId::delete_operation:{
            rv = QString("<entry gd:etag=\'*\'>\n");
        }break;
        default:{
            if (m_etag.isEmpty()) {
                rv = QString("<entry>\n");
            }
        }
        }
        
        rv += batch2xml(m_batch_id);
        rv += "\n";
    }
    else{
        if (m_etag.isEmpty()) {
            rv = QString("<entry>\n");
        }
        else{
            rv = QString("<entry gd:etag=\"%1\">\n")
                .arg(m_etag);
        }
    }
    

    if(!m_id.isEmpty())rv += QString("<id>http://www.google.com/m8/feeds/contacts/%1/base/%2</id>\n").arg(userEmail).arg(m_id);
    if(!m_title.isEmpty())rv += QString("<title>%1</title>\n").arg(m_title);
    if (!m_title.isEmpty())rv += QString("<content type = \"text\">%1</content>\n").arg(m_content);
    if (!m_name.isEmpty()) {
        rv += "<gd:name>\n";
        rv += QString("    <gd:givenName>%1</gd:givenName>\n").arg(m_name.givenName());
        rv += QString("    <gd:familyName>%1</gd:familyName>\n").arg(m_name.familyName());
        rv += QString("    <gd:fullName>%1</gd:fullName>\n").arg(m_name.fullName());
        rv += "</gd:name>\n";
    }
    if(!m_organization.isEmpty())rv += m_organization.toXmlString();
    rv += m_address_list.toXmlString();
    rv += m_emails.toXmlString();
    rv += m_phones.toXmlString();
    rv += "</entry>";
    return rv;
};


void ContactInfo::mergeEntryNode(QDomDocument& doc, QDomNode& entry_node)const
{
    QDomElement entry_elem = entry_node.toElement();
    if (entry_elem.isNull()) {
        qWarning() << "Invalid entry element";
    }
    else {
        entry_elem.setAttribute("gd:etag", m_etag);
    }

    xml_util::updateNode(doc, entry_node, "title", m_title);
    xml_util::updateNode(doc, entry_node, "content", m_content);
    m_organization.toXmlDoc(doc, entry_node);
    m_name.toXmlDoc(doc, entry_node);
    m_emails.toXmlDoc(doc, entry_node);
    m_phones.toXmlDoc(doc, entry_node);
    m_address_list.toXmlDoc(doc, entry_node);
};


bool ContactInfo::parseEntryNode(QDomNode n)
{
    m_original_xml_string = "";
    QTextStream ss(&m_original_xml_string);
    n.save(ss, 4);
    ss.flush();

    if (m_original_xml_string.isEmpty()) {
        qWarning() << "Failed to parse contact xml node";
        return false;
    }

    m_is_null = false;
    m_etag = getAttribute(n, "gd:etag");
    m_id = "";
    QString sid = n.firstChildElement("id").text();
    int base_idx = sid.indexOf("/base/");
    if (base_idx != -1) {
        m_id = sid.right(sid.length() - base_idx - 6);
    }

    QString s = n.firstChildElement("updated").text().trimmed();
    m_updated = QDateTime::fromString(s, Qt::ISODate);
    //qDebug() << "ykh-parsing-date" << s << "res=" << m_updated;
    m_title = n.firstChildElement("title").text().trimmed();
    m_content = n.firstChildElement("content").text().trimmed();
    m_emails = EmailInfoList::parse(n);
    m_phones = PhoneInfoList::parse(n);
    m_name = NameInfo::parse(n);
    m_organization = OrganizationInfo::parse(n);
    m_address_list = PostalAddressList::parse(n);
    m_is_null = !m_etag.isEmpty() && m_id.isEmpty();
    return !m_is_null;
}

bool ContactInfo::setFromDbRecord(QSqlQuery* q)
{
    QString xml = q->value(1).toString();
    if (!parseXml(xml)) {
        qWarning() << "failed to parse contact entry db record" << xml.size();
        return false;
    }
    bool ok = false;
    m_status = ContactXmlPersistant::validatedStatus(q->value(0).toInt(), &ok);
    if (!ok) {
        qWarning() << "Invalid contact db record status" << q->value(0).toInt();
    }
    m_updated = QDateTime::fromMSecsSinceEpoch(q->value(2).toLongLong());
    m_db_id = q->value(3).toInt();
    m_title = q->value(4).toString();
    m_content = q->value(5).toString();

    m_original_xml_string = q->value(6).toString();

    NameInfo n;
    n.setFullName(q->value(7).toString())
        .setGivenName(q->value(8).toString())
        .setFamilyName(q->value(9).toString());


    OrganizationInfo o;
    o.setName(q->value(10).toString())
        .setTitle(q->value(11).toString())
        .setTypeLabel(q->value(12).toString());

    setName(n);
    setOrganizationInfo(o);

    return true;

};

void ContactInfo::assignContent(const ContactInfo& src)
{
    ContactXmlPersistant::assignContent(src);
    m_name = src.m_name;
    m_organization = src.m_organization;
    m_emails = src.m_emails;
    m_phones = src.m_phones;
    m_address_list = src.m_address_list;    
};

/**
    GroupInfo
*/
GroupInfo* GroupInfo::createWithId(QString group_id) 
{
    GroupInfo* rv = new GroupInfo();
    rv->m_id = group_id;
    return rv;
};

bool GroupInfo::parseEntryNode(QDomNode n)
{
    m_original_xml_string = "";
    QTextStream ss(&m_original_xml_string);
    n.save(ss, 4);
    ss.flush();

    if (m_original_xml_string.isEmpty()) {
        qWarning() << "Failed to parse contact xml node";
        return false;
    }
    m_is_null = false;
    m_etag = getAttribute(n, "gd:etag");
    m_id = "";
    QString sid = n.firstChildElement("id").text();
    int base_idx = sid.indexOf("/base/");
    if (base_idx != -1) {
        m_id = sid.right(sid.length() - base_idx - 6);
    }

    QString s = n.firstChildElement("updated").text().trimmed();
    m_updated = QDateTime::fromString(s, Qt::ISODate);
    m_title = n.firstChildElement("title").text().trimmed();
    m_content = n.firstChildElement("content").text().trimmed();

    m_is_null = !m_etag.isEmpty() && m_id.isEmpty();
    return !m_is_null;
}

void GroupInfo::mergeEntryNode(QDomDocument& doc, QDomNode& entry_node)const
{
    QDomElement entry_elem = entry_node.toElement();
    if (entry_elem.isNull()) {
        qWarning() << "Invalid entry element";
    }
    else {
        entry_elem.setAttribute("gd:etag", m_etag);
    }

    xml_util::updateNode(doc, entry_node, "title", m_title);
    xml_util::updateNode(doc, entry_node, "content", m_content);
};

QString GroupInfo::toXml(QString userEmail)const 
{
    QString rv;
        
    if (m_batch_id != googleQt::EBatchId::none) {
        switch(m_batch_id){
        case googleQt::EBatchId::update:
        case googleQt::EBatchId::delete_operation:{
            rv = QString("<entry gd:etag=\'*\'>\n");
        }break;
        default:{
            if (m_etag.isEmpty()) {
                rv = QString("<entry>\n");
            }
        }
        }
        
        rv += batch2xml(m_batch_id);
        rv += "\n";
    }
    else{
        if (m_etag.isEmpty()) {
            rv = QString("<entry>\n");
        }
        else{
            rv = QString("<entry gd:etag=\"%1\">\n")
                .arg(m_etag);
        }
    }


    if (!m_id.isEmpty())rv += QString("<id>http://www.google.com/m8/feeds/groups/%1/base/%2</id>\n").arg(userEmail).arg(m_id);
    if(!m_title.isEmpty())rv += QString("<title>%1</title>\n").arg(m_title);
    if (!m_content.isEmpty())rv += QString("<content type = \"text\">%1</content>\n").arg(m_content);
    rv += "</entry>\n";
    return rv;
};

void GroupInfo::assignContent(const GroupInfo& src)
{
    ContactXmlPersistant::assignContent(src);
}

QString GroupInfo::toString()const
{
    QString s = "";
    s += "id=" + m_id + ";etag=" + m_etag + ";updated=" + m_updated.toString(Qt::ISODate)
        + ";title=" + m_title + "content=" + m_content + ";";
    return s;
};


GroupInfo& GroupInfo::setTitle(QString val)
{
    m_title = val;
    return *this;
};

GroupInfo& GroupInfo::setContent(QString notes)
{
    m_content = notes;
    return *this;
};

bool GroupInfo::operator == (const GroupInfo& o) const
{
    ///do not check on m_original_xml_string - it's for cache storage

    COMPARE_NO_CASE(m_etag);
    COMPARE_NO_CASE(m_id);
    COMPARE_NO_CASE(m_title);
    COMPARE_NO_CASE(m_content);

    return true;
}

bool GroupInfo::operator!=(const GroupInfo& o) const
{
    return !(*this == o);
};

bool GroupInfo::setFromDbRecord(QSqlQuery* q) 
{
    QString xml = q->value(1).toString();
    if (!parseXml(xml)) {
        qWarning() << "failed to parse contact group db record" << xml.size();
        return false;
    }
    bool ok = false;
    m_status = ContactXmlPersistant::validatedStatus(q->value(0).toInt(), &ok);
    if (!ok) {
        qWarning() << "Invalid contact db group record status" << q->value(0).toInt();
    }

    m_updated = QDateTime::fromMSecsSinceEpoch(q->value(2).toLongLong());
    m_db_id = q->value(3).toInt();
    m_title = q->value(4).toString();
    m_content = q->value(5).toString();
    return true;
};

/**
    GContactCache
*/
GContactCache::GContactCache(ApiEndpoint& a):m_endpoint(a)
{

};

void GContactCache::attachSQLStorage(std::shared_ptr<mail_cache::GMailSQLiteStorage> ss) 
{
    m_sql_storage = ss;
};

bool GContactCache::ensureContactTables()
{
    /// contacts ///
    QString sql_entries = QString("CREATE TABLE IF NOT EXISTS %1gcontact_entry(contact_db_id INTEGER PRIMARY KEY, acc_id INTEGER NOT NULL, entry_id TEXT, etag TEXT, title TEXT, content TEXT,"
                                  "full_name TEXT, given_name TEXT, family_name TEXT, orga_name TEXT, orga_title TEXT, orga_label TEXT, xml_original TEXT, xml_current TEXT, updated INTEGER, status INTEGER)").arg(m_sql_storage->m_metaPrefix);
    if (!m_sql_storage->execQuery(sql_entries))
        return false;

    QString sql_groups = QString("CREATE TABLE IF NOT EXISTS %1gcontact_group(group_db_id INTEGER PRIMARY KEY, acc_id INTEGER NOT NULL, entry_id TEXT, etag TEXT, title TEXT, content TEXT,"
        "xml_original TEXT, updated INTEGER, status INTEGER)").arg(m_sql_storage->m_metaPrefix);
    if (!m_sql_storage->execQuery(sql_groups))
        return false;


    QString sql_config = QString("CREATE TABLE IF NOT EXISTS %1gcontact_config(acc_id INTEGER NOT NULL, config_name TEXT, config_value TEXT)").arg(m_sql_storage->m_metaPrefix);
    if (!m_sql_storage->execQuery(sql_config))
        return false;


#ifdef GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD
    QString sql_fields = QString("CREATE TABLE IF NOT EXISTS %1gcontact_records(record_id INTEGER PRIMARY KEY, contact_db_id INTEGER NOT NULL, obj_kind INTEGER NOT NULL, group_idx INTEGER NOT NULL, record_name TEXT NOT NULL, record_value TEXT)").arg(m_sql_storage->m_metaPrefix);
    if (!m_sql_storage->execQuery(sql_fields))
        return false;
#endif //GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD

    return true;
};

bool GContactCache::storeContactEntries() 
{
    using CLIST = std::list<ContactInfo::ptr>;
    CLIST new_contacts;
    CLIST updated_contacts;
    for (auto& c : m_contacts.items()) {
        if (c->isDbIdNull()) {
            new_contacts.push_back(c);
        }
        else {
            updated_contacts.push_back(c);
        }
    }

    if (new_contacts.size() > 0) {
        QString sql_insert;
        sql_insert = QString("INSERT INTO  %1gcontact_entry(acc_id, title, content, full_name, given_name, family_name, orga_name, orga_title, orga_label, xml_original, xml_current, updated, status, entry_id, etag)"
            " VALUES(%2, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId);
        QSqlQuery* q = m_sql_storage->prepareQuery(sql_insert);
        if (!q) {
            qWarning() << "Failed to prepare contact insert-SQL" << sql_insert;
            return false;
        }

        for (auto c : new_contacts) {
            q->addBindValue(c->title());
            q->addBindValue(c->content());
            q->addBindValue(c->name().fullName());
            q->addBindValue(c->name().givenName());
            q->addBindValue(c->name().familyName());
            q->addBindValue(c->organization().name());
            q->addBindValue(c->organization().title());
            q->addBindValue(c->organization().typeLabel());
            q->addBindValue(c->originalXml());
            q->addBindValue(c->toXml(m_endpoint.apiClient()->userId()));
            qlonglong upd_time = c->updated().toMSecsSinceEpoch();
            q->addBindValue(upd_time);
            q->addBindValue(c->status());
            q->addBindValue(c->id());
            q->addBindValue(c->etag());
            if (q->exec()) {
                int eid = q->lastInsertId().toInt();
                c->setDbID(eid);
            }
            else {
                QString error = q->lastError().text();
                qWarning() << "ERROR. Failed to insert contact to DB" << error;
                continue;
            }
        }

#ifdef GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD
        /// store records ///
        QString sql_insert_record;
        sql_insert_record = QString("INSERT INTO  %1gcontact_records(contact_db_id, obj_kind, group_idx, record_name, record_value)"
            " VALUES(?, ?, ?, ?, ?)")
            .arg(m_sql_storage->m_metaPrefix);
        q = m_sql_storage->prepareQuery(sql_insert_record);
        if (!q) {
            qWarning() << "Failed to prepare contact insert-SQL" << sql_insert_record;
            return false;
        }

        int contactId = 0;
        std::function<void(QSqlQuery*)> header_binder = [&contactId](QSqlQuery* q)
        {
            q->addBindValue(contactId);
        };

        for (auto c : new_contacts) {
            contactId = c->dbID();
            if (!c->insertDbRecords(q, header_binder)) {
                return false;
            }
        }
#endif// GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD
    }
    else if (updated_contacts.size() > 0) {
        QString sql_update;
        sql_update = QString("UPDATE  %1gcontact_entry SET title=?, content=?, full_name=?, given_name=?, family_name=?, orga_name=?, orga_title=?, "
            "orga_label=?, xml_original=?, xml_current=?, updated=?, status=? WHERE contact_db_id=? AND acc_id = %2")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId);
        QSqlQuery* q = m_sql_storage->prepareQuery(sql_update);
        if (!q) {
            qWarning() << "Failed to prepare contact update-SQL" << sql_update;
            return false;
        }

        for (auto c : updated_contacts) {
            q->addBindValue(c->title());
            q->addBindValue(c->content());
            q->addBindValue(c->name().fullName());
            q->addBindValue(c->name().givenName());
            q->addBindValue(c->name().familyName());
            q->addBindValue(c->organization().name());
            q->addBindValue(c->organization().title());
            q->addBindValue(c->organization().typeLabel());
            q->addBindValue(c->originalXml());
            q->addBindValue(c->toXml(m_endpoint.apiClient()->userId()));
            qlonglong upd_time = c->updated().toMSecsSinceEpoch();
            q->addBindValue(upd_time);
            q->addBindValue(c->status());
            q->addBindValue(c->dbID());
            if (!q->exec()) {
                QString error = q->lastError().text();
                qWarning() << "ERROR. Failed to update contact entry" << error;
                continue;
            }
        }
    }


    return true;
};

bool GContactCache::storeContactGroups()
{
    using CLIST = std::list<GroupInfo::ptr>;
    CLIST new_groups;
    CLIST updated_groups;
    for (auto& g : m_groups.items()) {
        if (g->isDbIdNull()) {
            new_groups.push_back(g);
        }
        else {
            updated_groups.push_back(g);
        }
    }

    if (new_groups.size() > 0) {
        QString sql_insert;
        sql_insert = QString("INSERT INTO  %1gcontact_group(acc_id, title, content, xml_original, updated, status, entry_id, etag)"
            " VALUES(%2, ?, ?, ?, ?, ?, ?, ?)")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId);

        QSqlQuery* q = m_sql_storage->prepareQuery(sql_insert);
        if (!q) {
            qWarning() << "Failed to prepare contact group insert-SQL" << sql_insert;
            return false;
        }

        for (auto c : new_groups) {
            q->addBindValue(c->title());
            q->addBindValue(c->content());
            q->addBindValue(c->originalXml());
            qlonglong upd_time = c->updated().toMSecsSinceEpoch();
            q->addBindValue(upd_time);
            q->addBindValue(c->status());
            q->addBindValue(c->id());
            q->addBindValue(c->etag());
            if (q->exec()) {
                int eid = q->lastInsertId().toInt();
                c->setDbID(eid);
            }
            else {
                QString error = q->lastError().text();
                qWarning() << "ERROR. Failed to insert contact group to DB" << error;
                continue;
            }
        }//for
    }
    else if (updated_groups.size() > 0) {
        QString sql_update;
        sql_update = QString("UPDATE  %1gcontact_group SET title=?, content=?, xml_original=?, updated=?, status=? WHERE group_db_id=? AND acc_id = %2")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId);

        QSqlQuery* q = m_sql_storage->prepareQuery(sql_update);
        if (!q) {
            qWarning() << "Failed to prepare contact group update-SQL" << sql_update;
            return false;
        }

        for (auto c : updated_groups) {
            q->addBindValue(c->title());
            q->addBindValue(c->content());
            q->addBindValue(c->originalXml());
            qlonglong upd_time = c->updated().toMSecsSinceEpoch();
            q->addBindValue(upd_time);
            q->addBindValue(c->status());
            q->addBindValue(c->dbID());
            if (!q->exec()) {
                QString error = q->lastError().text();
                qWarning() << "ERROR. Failed to update contact group" << error;
                continue;
            }
        }//for
    }

    return true;
};

bool GContactCache::storeConfig()
{
    if (m_sync_time.isValid()) {

        QString sql_del_old;
        sql_del_old = QString("DELETE FROM  %1gcontact_config WHERE acc_id=%2 AND config_name='%3'")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId)
            .arg(CONFIG_SYNC_TIME);

        if (!m_sql_storage->execQuery(sql_del_old)) {
            qWarning() << "Failed to delete old contact config del-SQL" << sql_del_old;
            return false;
        };


        QString sql_insert;
        sql_insert = QString("INSERT INTO %1gcontact_config(acc_id, config_name, config_value)"
            " VALUES(%2, ?, ?)")
            .arg(m_sql_storage->m_metaPrefix)
            .arg(m_sql_storage->m_accId);

        QSqlQuery* q = m_sql_storage->prepareQuery(sql_insert);
        if (!q) {
            qWarning() << "Failed to prepare contact config insert-SQL" << sql_insert;
            return false;
        }
        q->addBindValue(CONFIG_SYNC_TIME);
        q->addBindValue(m_sync_time.toString());

        if (!q->exec()) {
            QString error = q->lastError().text();
            qWarning() << "ERROR. Failed to insert contact config to DB" << error;
            return false;
        }
    }

    return true;
};

bool GContactCache::storeContactsToDb() 
{
    if (!storeContactEntries()) {
        return false;
    }

    if (!storeContactGroups()) {
        return false;
    }

    if (!storeConfig()) {
        return false;
    }

    return true;
};

bool GContactCache::loadContactsFromDb() 
{
    if (!loadContactEntriesFromDb()) {
        return false;
    }

    if (!loadContactGroupsFromDb()) {
        return false;
    }

    if (!loadContactConfigFromDb()) {
        return false;
    }

    return true;
};

bool GContactCache::clearDbCache() 
{
    if (!m_sql_storage) {
        qWarning() << "Sql cache is not setup";
        return false;
    }

    m_contacts.clear();
    m_groups.clear();
    m_configs.clear();
    QDateTime invalid_time;
    m_sync_time.swap(invalid_time);

    QString sql = QString("DELETE FROM %1gcontact_group WHERE acc_id=%2")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);

    if (!m_sql_storage->execQuery(sql)) {
        return false;
    };

    sql = QString("DELETE FROM %1gcontact_entry WHERE acc_id=%2")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);

    if (!m_sql_storage->execQuery(sql)) {
        return false;
    };

    sql = QString("DELETE FROM %1gcontact_config WHERE acc_id=%2")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);

    if (!m_sql_storage->execQuery(sql)) {
        return false;
    };

    return true;
};

bool GContactCache::mergeContactsAndStoreToDb(GroupList& server_glist, ContactList& server_clist) 
{
    mergeEntries(server_clist);
    mergeGroups(server_glist);

    m_sync_time = m_contacts.recalcUpdatedTime(QDateTime());
    m_sync_time = m_groups.recalcUpdatedTime(m_sync_time);

    bool rv = storeContactsToDb();
    return rv;
};

bool GContactCache::loadContactGroupsFromDb()
{
    m_groups.clear();

    QString sql = QString("SELECT status, xml_original, updated, group_db_id, title, content FROM %1gcontact_group WHERE acc_id=%2 AND status IN(1,2) ORDER BY title")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);
    QSqlQuery* q = m_sql_storage->selectQuery(sql);
    if (!q)
        return false;

    while (q->next())
    {
        std::shared_ptr<GroupInfo> g(new GroupInfo);
        if (g->setFromDbRecord(q)) {
            m_groups.add(g);
        };
    }

    return true;
};

bool GContactCache::loadContactEntriesFromDb()
{
    m_contacts.clear();

    QString sql = QString("SELECT status, xml_current, updated, contact_db_id, title, content, xml_original, full_name, given_name, family_name, orga_name, orga_title, orga_label FROM %1gcontact_entry WHERE acc_id=%2 AND status IN(1,2) ORDER BY title")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);
    QSqlQuery* q = m_sql_storage->selectQuery(sql);
    if (!q)
        return false;

    while (q->next())
    {
        std::shared_ptr<ContactInfo> c(new ContactInfo);
        if (c->setFromDbRecord(q)) {
            m_contacts.add(c);
        };
    }

    return true;
};

bool GContactCache::loadContactConfigFromDb() 
{
    m_configs.clear();

    QString sql = QString("SELECT config_name, config_value FROM %1gcontact_config WHERE acc_id=%2")
        .arg(m_sql_storage->m_metaPrefix)
        .arg(m_sql_storage->m_accId);
    QSqlQuery* q = m_sql_storage->selectQuery(sql);
    if (!q)
        return false;

    while (q->next())
    {
        QString name = q->value(0).toString();
        QString value = q->value(1).toString();
        m_configs[name] = value;

        if (name.compare(CONFIG_SYNC_TIME, Qt::CaseInsensitive) == 0) {
            m_sync_time = QDateTime::fromString(value);
        }
    }

    return true;
};

void GContactCache::mergeEntries(ContactList& entry_list) 
{
    m_contacts.mergeList(entry_list);
    /*
    Q_UNUSED(entry_list);
    for (auto sc : entry_list.items()) {
        auto c = m_contacts.findById(sc->id());
        if (!c) {
            m_contacts.add(c);
        }
        else {
            if (c->isModified()) {
                c->markAsRetired();
                sc->markAsClean();
                m_contacts.add(sc);
            }
            else {
                *c = *sc;
                c->markAsClean();
            }
        }
    }
    */
};

void GContactCache::mergeGroups(GroupList& group_list) 
{
    //Q_UNUSED(group_list);
    m_groups.mergeList(group_list);
};


GcontactCacheRoutes::GcontactCacheRoutes(googleQt::Endpoint& endpoint, GcontactRoutes& ):m_endpoint(endpoint)
{
    m_GContactsCache.reset(new GContactCache(endpoint));
};

GcontactCacheQueryTask* GcontactCacheRoutes::synchronizeContacts_Async()
{
    GcontactCacheQueryTask* rv = new GcontactCacheQueryTask(m_endpoint, m_GContactsCache);

    if (!m_GContactsCache->m_sql_storage) {
        std::unique_ptr<GoogleException> ex(new GoogleException("Local cache DB is not setup. Call setupCache first"));
        rv->failed_callback(std::move(ex));
        return rv;
    }

    if (!m_GContactsCache->lastSyncTime().isValid()) {
        ///first time sync, purge local cache just in case..
        if (!m_GContactsCache->clearDbCache()) {
            std::unique_ptr<GoogleException> ex(new GoogleException("Failed to clear local DB cache."));
            rv->failed_callback(std::move(ex));
            return rv;
        }
    }

    return reloadCache_Async(rv, m_GContactsCache->lastSyncTime());
};

GcontactCacheQueryTask* GcontactCacheRoutes::reloadCache_Async(GcontactCacheQueryTask* rv, QDateTime dtUpdatedMin)
{
    ContactsListArg entries_arg;
    entries_arg.setMaxResults(200);

    if (dtUpdatedMin.isValid()) {
        entries_arg.setUpdatedMin(dtUpdatedMin);
    }

    auto entries_task = m_endpoint.client()->gcontact()->getContacts()->list_Async(entries_arg);
    entries_task->then([=](std::unique_ptr<gcontact::ContactsListResult> lst)
    {
        rv->m_result_contacts = lst->detachData();

        ContactGroupListArg groups_arg;
        groups_arg.setMaxResults(200);

        auto groups_task = m_endpoint.client()->gcontact()->getContactGroup()->list_Async(groups_arg);
        groups_task->then([=](std::unique_ptr<gcontact::ContactGroupListResult> lst)
        {
            rv->m_result_groups = lst->detachData();
            if (m_GContactsCache->mergeContactsAndStoreToDb(*(rv->m_result_groups.get()), *(rv->m_result_contacts.get()))) {
                rv->completed_callback();
            }
            else {
                std::unique_ptr<GoogleException> ex(new GoogleException("Failed to merge/store DB cache."));
                rv->failed_callback(std::move(ex));
            }
        },
            [=](std::unique_ptr<GoogleException> ex) 
        {
            rv->failed_callback(std::move(ex));
        });
    },
        [=](std::unique_ptr<GoogleException> ex)
    {
        rv->failed_callback(std::move(ex));
    });

    return rv;
};


#ifdef GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD
/**
MRecordDbPersistant
*/
void MRecordDbPersistant::clearDbMaps()
{
    m_id2name.clear();
    m_name2id.clear();
};

bool MRecordDbPersistant::insertDbRecord(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder, int group_idx, QString recordName, QString recordValue)
{
    header_binder(q);
    q->addBindValue(objKind());
    q->addBindValue(group_idx);
    q->addBindValue(recordName);
    q->addBindValue(recordValue);
    if (q->exec()) {
        int rid = q->lastInsertId().toInt();
        m_id2name[rid] = recordName;
        m_name2id[recordName] = rid;
    }
    else {
        QString error = q->lastError().text();
        qWarning() << "ERROR. Failed to insert contact to DB" << error;
        return false;
    }

    return true;
};

bool MRecordDbPersistant::insertDbRecord(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder, int group_idx, QString recordName, bool recordValue)
{
    return insertDbRecord(q, header_binder, group_idx, recordName, recordValue ? QString("1") : QString("0"));
};

bool PostalAddress::insertDb(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder, int group_idx)
{
    clearDbMaps();
    insertDbRecord(q, header_binder, group_idx, "city", m_city);
    insertDbRecord(q, header_binder, group_idx, "street", m_street);
    insertDbRecord(q, header_binder, group_idx, "region", m_region);
    insertDbRecord(q, header_binder, group_idx, "postcode", m_postcode);
    insertDbRecord(q, header_binder, group_idx, "country", m_country);
    insertDbRecord(q, header_binder, group_idx, "type_label", m_type_label);
    insertDbRecord(q, header_binder, group_idx, "formattedAddress", m_formattedAddress);
    insertDbRecord(q, header_binder, group_idx, "is_primary", m_is_primary);
    return true;
};

bool PhoneInfo::insertDb(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder, int group_idx)
{
    clearDbMaps();
    insertDbRecord(q, header_binder, group_idx, "number", m_number);
    insertDbRecord(q, header_binder, group_idx, "uri", m_uri);
    insertDbRecord(q, header_binder, group_idx, "type_label", m_type_label);
    insertDbRecord(q, header_binder, group_idx, "is_primary", m_is_primary);

    return true;
};

bool EmailInfo::insertDb(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder, int group_idx)
{
    clearDbMaps();
    insertDbRecord(q, header_binder, group_idx, "address", m_address);
    insertDbRecord(q, header_binder, group_idx, "display_name", m_display_name);
    insertDbRecord(q, header_binder, group_idx, "type_label", m_type_label);
    insertDbRecord(q, header_binder, group_idx, "is_primary", m_is_primary);

    return true;
};

bool ContactInfo::insertDbRecords(QSqlQuery* q, std::function<void(QSqlQuery*)> header_binder)
{
    if (!m_emails.insertDb(q, header_binder)) {
        return false;
    };

    if (!m_phones.insertDb(q, header_binder)) {
        return false;
    };

    if (!m_address_list.insertDb(q, header_binder)) {
        return false;
    };

    return true;
};

#endif //GOOGLE_QT_CONTACT_DB_STRUCT_AS_RECORD

#ifdef API_QT_AUTOTEST

std::unique_ptr<ContactInfo> ContactInfo::EXAMPLE(int param, int )
{
    QString email = QString("me%1@gmail.com").arg(param);
    QString first = QString("First-Name%1").arg(param);
    QString last = QString("Last-Name%1").arg(param);

    ContactInfo ci;
    NameInfo n;
    EmailInfo e1, e2;
    PhoneInfo p1, p2;
    OrganizationInfo o;
    PostalAddress a1, a2;

    n.setFamilyName(last).setGivenName(first).setFullName(first + " " + last);
    e1.setAddress(email).setDisplayName(first + " " + last).setPrimary(true).setTypeLabel("home");
    e2.setAddress(QString("2") + email).setDisplayName(first + " " + last).setPrimary(false).setTypeLabel("work");
    p1.setNumber(QString("1-111-111%1").arg(param)).setPrimary(true);
    p2.setNumber(QString("2-222-222%1").arg(param)).setPrimary(false);
    o.setName("1-organization-name").setTitle("title-in-the-organization");
    a1.setCity("Mountain View-1").setTypeLabel("work")
        .setStreet(QString("1111-%1 Amphitheatre Pkwy").arg(param))
        .setRegion("CA").setPostcode(QString("11111-%1").arg(param))
        .setCountry("United States")
        .setPrimary(true);

    a2.setCity("Mountain View-2").setTypeLabel("home")
        .setStreet(QString("2222-%1 Amphitheatre Pkwy").arg(param))
        .setRegion("NY").setPostcode(QString("22222-%1").arg(param))
        .setCountry("United States")
        .setPrimary(false);


    ci.setName(n).setTitle("Title for " + first + " " + last)
        .addEmail(e1).addEmail(e2)
        .addPhone(p1).addPhone(p2)
        .setContent(QString("My notest on new contact for '%1'").arg(first))
        .setOrganizationInfo(o)
        .addAddress(a1).addAddress(a2);

    ci.m_etag = "my-contact-etag";
    ci.m_id = "my-contact-id";

    std::unique_ptr<ContactInfo> rv(new ContactInfo);
    QString xml = ci.toXml("me@gmail.com");
    rv->parseXml(xml);
    return rv;
}

std::unique_ptr<GroupInfo> GroupInfo::EXAMPLE(int context_index, int parent_content_index) 
{
    GroupInfo g;
    g.setContent(QString("cgroup-content %1-%2").arg(context_index).arg(parent_content_index));
    g.setTitle(QString("cgroup-title  %1-%2").arg(context_index).arg(parent_content_index));
    g.m_etag = "my-group-etag";
    g.m_id = "my-group-id";



    std::unique_ptr<GroupInfo> rv(new GroupInfo);
    QString xml = g.toXml("me@gmail.com");
    rv->parseXml(xml);
    return rv;
};

template <class T>
bool autoTestCheckImportExport(T* ci) 
{
    QString xml = ci->toXml("me@gmail.com");
    QByteArray data(xml.toStdString().c_str());

    QDomDocument doc;
    QString errorMsg = 0;
    int errorLine;
    int errorColumn;
    if (!doc.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
        ApiAutotest::INSTANCE() << QString("Failed to export contacts XML document: %1 line=%2 column=%2").arg(errorMsg).arg(errorLine).arg(errorColumn);
        ApiAutotest::INSTANCE() << xml;
        return false;
    }

    T ci2;
    if (!ci2.parseXml(data)) {
        ApiAutotest::INSTANCE() << "Contacts Xml parse error";
        return false;
    };

    bool rv = (*ci == ci2);
    if (!rv) {
        //this line is here to put breakpoint cause something is wrong
        //it's code can be compiled for debug anyway..
        rv = (*ci == ci2);
    }
    return rv;
}


std::unique_ptr<ContactInfo> createMergeContactParty(QString xmlOrigin) 
{
    std::unique_ptr<ContactInfo> c = std::unique_ptr<ContactInfo>(new ContactInfo);

    c->parseXml(xmlOrigin);
    c->setTitle(c->title() + "=NEW-TITLE=");
    c->setContent("=NEW-CONTENT=");

    OrganizationInfo o;
    o.setName("=NEW-organization-name=").setTitle("=NEW-title-in-the-organization=");
    c->setOrganizationInfo(o);
    NameInfo n;
    n.setFamilyName("=NEW-last=").setGivenName("=NEW-first").setFullName("=NEW-first_and_last=");
    c->setName(n);

    PhoneInfo p1, p2;
    std::list<PhoneInfo> lst;
    p1.setNumber("=NEW-1-111-1111=").setPrimary(true);
    p2.setNumber("=NEW-2-222-2222=").setPrimary(false);
    lst.push_back(p1);
    lst.push_back(p2);
    c->replacePhones(lst);

    EmailInfo e1, e2;
    e1.setAddress("=NEW-first-email=").setDisplayName("=NEW-first-last=").setPrimary(true).setTypeLabel("home");
    e2.setAddress("=NEW-second-email=").setDisplayName("=NEW-second-first-last=").setPrimary(false).setTypeLabel("work");
    std::list<EmailInfo> e_lst;
    e_lst.push_back(e1);
    e_lst.push_back(e2);
    c->replaceEmails(e_lst);

    PostalAddress a1, a2;
    a1.setCity("=NEW-ADDR-1=").setStreet("=NEW-STREET1=").setRegion("=NEW-REGION1=").setPostcode("=NEW-ZIP1=").setCountry("=NEW-COUNTRY1=").setPrimary(true);
    a2.setCity("=NEW-ADDR-2=").setStreet("=NEW-STREET2=").setRegion("=NEW-REGION2=").setPostcode("=NEW-ZIP2=").setCountry("=NEW-COUNTRY2=").setPrimary(false);
    std::list<PostalAddress> a_lst;
    a_lst.push_back(a1);
    a_lst.push_back(a2);
    c->replaceAddressList(a_lst);

    return c;
}

std::unique_ptr<GroupInfo> createMergeGroupParty(QString xmlOrigin)
{
    std::unique_ptr<GroupInfo> g = std::unique_ptr<GroupInfo>(new GroupInfo);
    g->parseXml(xmlOrigin);
    g->setTitle(g->title() + "=NEW-G-TITLE=");
    g->setContent("=NEW-G-CONTENT=");
    return g;
}

static bool autoTestCheckContactMerge(ContactInfo* ci)
{
    QString xml = ci->toXml("me@gmail.com");
    auto c = createMergeContactParty(xml);
    return autoTestCheckImportExport<ContactInfo>(c.get());
}

static bool autoTestCheckGroupMerge(GroupInfo* ci)
{
    QString xml = ci->toXml("me@gmail.com");
    auto c = createMergeGroupParty(xml);
    return autoTestCheckImportExport<GroupInfo>(c.get());
}

static void autoTestStoreAndCheckIdentityOnLoad(gcontact::contact_cache_ptr ccache)
{
    ContactList contact_list_copy = ccache->contacts();
    GroupList group_list_copy = ccache->groups();
    ccache->storeContactsToDb();
    ccache->loadContactsFromDb();
    if (contact_list_copy == ccache->contacts()) {
        ApiAutotest::INSTANCE() << QString("contacts-DB-identity - OK / %1").arg(contact_list_copy.items().size());
    }
    else {
        ApiAutotest::INSTANCE() << QString("contacts-DB-identity - ERROR / %1").arg(contact_list_copy.items().size());
    }

    if (group_list_copy == ccache->groups()) {
        ApiAutotest::INSTANCE() << QString("groups-DB-identity - OK / %1").arg(group_list_copy.items().size());
    }
    else {
        ApiAutotest::INSTANCE() << QString("groups-DB-identity - ERROR / %1").arg(group_list_copy.items().size());
    }
}

static void autoTestModifyRandomly(gcontact::contact_cache_ptr ccache)
{
    ContactList& contact_list_copy = ccache->contacts();
    GroupList& group_list_copy = ccache->groups();
    for (auto c : contact_list_copy.items()) {
        c->setTitle(c->title() + "-m");
        c->setContent(c->content() + "-m");

        NameInfo n = c->name();
        n.setFamilyName(n.familyName() + "-m");
        n.setGivenName(n.givenName() + "-m");
        n.setFullName(n.fullName() + "m");
        c->setName(n);

        OrganizationInfo o = c->organization();
        o.setName(o.name() + "-m");
        o.setTitle(o.title() += "-m");
        c->setOrganizationInfo(o);

        c->markAsModified();
    }

    for (auto g : group_list_copy.items()) {
        g->setTitle(g->title() + "-m");
        g->setContent(g->content() + "-m");

        g->markAsModified();
    }
}

void GcontactCacheRoutes::runAutotest() 
{
    ApiAutotest::INSTANCE() << "start-gcontact-test";
    ApiAutotest::INSTANCE() << "1";
    ApiAutotest::INSTANCE() << "2";
    ApiAutotest::INSTANCE() << "3";
    ApiAutotest::INSTANCE() << "4";

    /// generate entries
    ContactList& lst = m_GContactsCache->contacts();
    for (int i = 0; i < 10; i++) {
        auto c = ContactInfo::EXAMPLE(i, 0);
        if (!autoTestCheckImportExport<ContactInfo>(c.get())) {
            ApiAutotest::INSTANCE() << "contact Xml export/identity error";;
        }
        else {
            ApiAutotest::INSTANCE() << QString("contact-identity - OK / %1").arg(i+1);

            if (!autoTestCheckContactMerge(c.get())) {
                ApiAutotest::INSTANCE() << "contact Xml merge error";;
            }
            else {
                ApiAutotest::INSTANCE() << QString("contact-merge - OK / %1").arg(i + 1);
                lst.add(std::move(c));
            }
        }
    }

    /// generate groups
    GroupList& g_lst = m_GContactsCache->groups();
    for (int i = 0; i < 10; i++) {
        auto g = GroupInfo::EXAMPLE(i, 0);
        if (!autoTestCheckImportExport<GroupInfo>(g.get())) {
            ApiAutotest::INSTANCE() << "group Xml export/identity error";;
        }
        else {
            ApiAutotest::INSTANCE() << QString("group-identity - OK / %1").arg(i + 1);
            if (!autoTestCheckGroupMerge(g.get())) {
                ApiAutotest::INSTANCE() << "group Xml merge error";;
            }
            else {
                ApiAutotest::INSTANCE() << QString("group-merge - OK / %1").arg(i + 1);
                g_lst.add(std::move(g));
            }
        }
    }

    ApiAutotest::INSTANCE() << "test persistance identity";
    autoTestStoreAndCheckIdentityOnLoad(m_GContactsCache);
    autoTestModifyRandomly(m_GContactsCache);
    ApiAutotest::INSTANCE() << "test persistance after modification identity";
    autoTestStoreAndCheckIdentityOnLoad(m_GContactsCache);

    m_GContactsCache->clearDbCache();

    ApiAutotest::INSTANCE().enableRequestLog(false);

    std::function<void(QString)> auto_sync_step = [=](QString slabel) 
    {
        synchronizeContacts_Async()->waitForResultAndRelease();
        ApiAutotest::INSTANCE() << QString("%1 %2/%3")
            .arg(slabel)
            .arg(m_GContactsCache->groups().items().size())
            .arg(m_GContactsCache->contacts().items().size());
    };

    std::function<void(int, int)> auto_sync_reserveIds = [=](int start, int num) 
    {
        IDSET idset;
        for (int i = start; i < start + num; i++) {
            QString sid = QString("id-%1").arg(i);
            idset.insert(sid);
        }
        ApiAutotest::INSTANCE().addIdSet("gcontact::ContactsListResult", idset);
    };

    std::function<void()> auto_sync_modifyContacts = [=]() 
    {
        for (auto c : m_GContactsCache->contacts().items()) {
            c->setTitle("m-" + c->title());
            c->markAsModified();
        }
        for (auto g : m_GContactsCache->groups().items()) {
            g->setTitle("m-" + g->title());
            g->markAsModified();
        }
        m_GContactsCache->storeContactsToDb();
    };

    auto_sync_reserveIds(1, 10);
    auto_sync_step("Sync contacts-1/clone");
    auto_sync_reserveIds(1, 10);
    auto_sync_step("Sync contacts-2/copy-over");

    auto_sync_modifyContacts();

    auto_sync_reserveIds(1, 10);
    auto_sync_step("Sync contacts-3/copy-over-and-retire");

    auto_sync_modifyContacts();
    auto_sync_reserveIds(1, 2);
    auto_sync_step("Sync contacts-4/some-copy-over-and-retire-some-sync");


    ApiAutotest::INSTANCE().enableRequestLog(true);
    ApiAutotest::INSTANCE() << "";
};
#endif
