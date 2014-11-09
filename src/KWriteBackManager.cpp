#include "KWriteBackManager.h"
#include<sstream>
#include "lang.h"
#include "malloc_debug.h"
using namespace std;
#ifdef ENABLE_WRITE_BACK
KWriteBackManager writeBackManager;
static KWriteBack *curWriteBack=NULL;
static bool edit=false;
KWriteBackManager::KWriteBackManager() {
}

KWriteBackManager::~KWriteBackManager() {
	destroy();
}
void KWriteBackManager::destroy() {
	std::map<std::string,KWriteBack *>::iterator it;
	for (it=writebacks.begin(); it!=writebacks.end(); it++) {
		(*it).second->release();
	}
	writebacks.clear();
}
KWriteBack * KWriteBackManager::refsWriteBack(std::string name)
{
	lock.RLock();
	KWriteBack *wb = getWriteBack(name);
	if(wb){
		wb->addRef();
	}
	lock.Unlock();
	return wb;
}
KWriteBack * KWriteBackManager::getWriteBack(std::string table_name) {
	KWriteBack *m_table=NULL;
	std::map<std::string,KWriteBack *>::iterator it;
	it = writebacks.find(table_name);
	if (it!=writebacks.end()) {
		return (*it).second;
	}
	return NULL;
}
bool KWriteBackManager::editWriteBack(std::string name, KWriteBack &m_a,
		std::string &err_msg) {
	lock.WLock();
	KWriteBack *m_tmp=getWriteBack(name);
	if(m_tmp==NULL) {
		err_msg=LANG_TABLE_NAME_ERR;
		lock.Unlock();
		return false;
	}
	m_tmp->name=m_a.name;
	m_tmp->msg=m_a.msg;
	lock.Unlock();
	return true;

}
bool KWriteBackManager::newWriteBack(std::string name, std::string msg,
		std::string &err_msg) {
	bool result=false;
	if (name.size()<2 || name.size()>32) {
		err_msg=LANG_TABLE_NAME_LENGTH_ERROR;
		return false;
	}
	KWriteBack *m_a=new KWriteBack;
	m_a->name=name;
	m_a->msg=msg;
	lock.WLock();
	if(getWriteBack(name)!=NULL) {
		delete m_a;
		goto done;
	}
	writebacks.insert(std::pair<std::string,KWriteBack *>(name,m_a));
	result=true;
	done:
	lock.Unlock();
	if(!result) {
		err_msg=LANG_TABLE_NAME_IS_USED;
	}
	return result;
}
bool KWriteBackManager::delWriteBack(std::string name, std::string &err_msg) {
	bool result=false;
	err_msg=LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<string,KWriteBack *>::iterator it=writebacks.find(name);
	if (it==writebacks.end()) {
		lock.Unlock();
		return false;
	}
	if ((*it).second->getRef()>1) {
		err_msg=LANG_TABLE_REFS_ERR;
		lock.Unlock();
		return false;
	}
	(*it).second->release();
	writebacks.erase(it);
	lock.Unlock();
	return true;
}
std::vector<std::string> KWriteBackManager::getWriteBackNames() {
	std::vector<std::string> table_names;
	std::map<std::string,KWriteBack *>::iterator it;
	lock.RLock();
	for(it=writebacks.begin();it!=writebacks.end();it++) {
		table_names.push_back((*it).first);
	}
	lock.Unlock();
	return table_names;
}
std::string KWriteBackManager::writebackList(std::string name) {
	stringstream s;
	std::map<std::string,KWriteBack *>::iterator it;
	s << "<html><LINK href=/kangle.css type='text/css' rel=stylesheet><body>\n";
	KWriteBack *m_a=NULL;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< LANG_NAME << "</td><td>" << LANG_REFS << "</td></tr>\n";
	lock.RLock();

	for(it=writebacks.begin();it!=writebacks.end();it++) {
		m_a = (*it).second;
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE << m_a->name;
		s << "')){ window.location='/writebackdelete?name=" << m_a->name << "';}\">" << LANG_DELETE << "</a>]";
		s << "[<a href=\"/writeback?name=" << m_a->name << "\">" << LANG_EDIT << "</a>]";
		s << "</td><td>" << m_a->name << "</td><td>" << m_a->getRefFast() << "</td></tr>\n";
	}
	if (name.size()>0) {
		m_a=getWriteBack(name);
	} else {
		m_a = NULL;
	}
	s << "</table>\n<hr>";
	s << "<form action=" << (m_a?"/writebackedit":"/writebackadd") << " meth=post>\n";
	s << LANG_NAME << ":<input name=name value='" << (m_a?m_a->name:"") << "'><br>";
	s << LANG_MSG << ":<textarea name=msg rows=7 cols=50>" << (m_a?m_a->msg:"") << "</textarea><br>\n";
	if(m_a) {
		s << "<input type=hidden name=namefrom value='" << m_a->name << "'>\n";
	}
	lock.Unlock();
	s << "<input type=submit value=" << (m_a?LANG_EDIT:LANG_SUBMIT) << "></form>\n";
	s << endTag() << "</body></html>";
	return s.str();

}
bool KWriteBackManager::addWriteBack(KWriteBack *wb)
{
	lock.WLock();
	if (getWriteBack(wb->name)) {
		lock.Unlock();
		return false;
	}
	writebacks.insert(std::pair<std::string,KWriteBack *>(wb->name,wb));
	lock.Unlock();
	return true;
}
bool KWriteBackManager::startElement(std::string &context, std::string &qName,
		std::map<std::string,std::string> &attribute) {
	if (qName=="writeback") {
		assert(curWriteBack==NULL);
		string err_msg;
		if (attribute["type"]=="del") {
			delWriteBack(attribute["name"], err_msg);
			return true;
		}
		if (attribute["type"] == "edit") {
			edit=true;
		} else {
			edit=false;
		}
		curWriteBack=new KWriteBack();
		curWriteBack->name=attribute["name"];
		return true;
	}
	return false;
}
bool KWriteBackManager::startCharacter(std::string &context,
		std::string &qName, char *character, int len) {

	if (qName=="writeback") {
		string err_msg;
		if (curWriteBack) {
			curWriteBack->msg=KXml::decode(character);
			if (!edit) {
				if (!addWriteBack(curWriteBack)) {
					curWriteBack->release();
				}
			} else {
				editWriteBack(curWriteBack->name, *curWriteBack, err_msg);
				delete curWriteBack;
			}
			curWriteBack=NULL;
		}
		return true;
	}
	return false;
}
void KWriteBackManager::buildXML(stringstream &s, int flag) {
	s << "\t<!--writeback start-->\n";
	std::map<std::string,KWriteBack *>::iterator it;
	lock.RLock();
	for(it=writebacks.begin();it!=writebacks.end();it++) {
		if ((*it).second->ext) {
			continue;
		}
		(*it).second->buildXML(s);
	}
	lock.Unlock();
	s << "\t<!--writeback end-->\n";
}
#endif
