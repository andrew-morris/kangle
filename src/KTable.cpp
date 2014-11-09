/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "KTable.h"
#include "KChain.h"
#include "KModelManager.h"
#include "malloc_debug.h"
#include "time_utils.h"
using namespace std;

bool KTable::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute,KAccess *kaccess) {

	if (context->getParentName() == TABLE_CONTEXT && context->qName
			== CHAIN_CONTEXT) {
		assert(curChain==NULL);
		curChain = new KChain();
	}
	if (curChain) {
		return curChain->startElement(context, attribute,kaccess);
	}
	return false;
}
void KTable::empty() {
	vector<KChain *>::iterator it;
	for (it = chain.begin(); it != chain.end(); it++) {
		//		(*it)->destroy();
		delete (*it);
	}
	chain.clear();
}
int KTable::insertChain(int index, KChain *newChain) {
	if (index < 0 ||  index>=(int)chain.size()) {
		chain.push_back(newChain);
		return chain.size() - 1;
	} else {		
		chain.insert(chain.begin() + index,newChain);		
		return index;
	}
}
bool KTable::delChain(std::string name) {
	 vector<KChain *>::iterator it;
	 for (it = chain.begin(); it != chain.end(); it++) {
		 if ((*it)->name == name) {
			 delete (*it);
			 chain.erase(it);
			 return true;
		 }
	 }
	 return false;
 }
 bool KTable::editChain(std::string name, KUrlValue *urlValue,KAccess *kaccess) {
	vector<KChain *>::iterator it;
	for (it = chain.begin(); it != chain.end(); it++) {
		if ((*it)->name == name) {
			(*it)->clear();
			return (*it)->edit(urlValue,kaccess,false);
		}
	}
	KChain *chain = new KChain();
	chain->name = name;
	bool result = chain->edit(urlValue,kaccess,false);
	insertChain(-1,chain);
	return result;
}
int KTable::getChain(const char *chain_name)
{
	vector<KChain *>::iterator it;
	int id = 0;
	for (it = chain.begin(); it != chain.end(); it++, id++) {
		if((*it)->name == chain_name){
			return id;
		}
	}
	return -1;
}
bool KTable::match(KHttpRequest *rq, KHttpObject *obj, int &jumpType,
		KJump **jumpTable, unsigned &checked_table,
		const char **hitTable, int *chainId) {
	vector<KChain *>::iterator it;
	KTable *m_table = NULL;
	KChain *matchChain;
	int id = 0;
	for (it = chain.begin(); it != chain.end(); ) {
		matchChain = (*it);
		if (matchChain->expire>0 && matchChain->expire<=kgl_current_sec) {
			it = chain.erase(it);
			continue;
		}
		it++;
		id++;
		if (matchChain->match(rq, obj, jumpType, jumpTable)) {
			switch (jumpType) {
			case JUMP_CONTINUE:
				continue;
			case JUMP_TABLE:
				m_table = (KTable *) (*jumpTable);
				if(m_table){
					assert(m_table);
/*					if (checked_table.find(m_table) != checked_table.end()) {
						jumpType = JUMP_DENY;
						//ËÀÑ­»·
						return true;
					}
					checked_table[m_table] = 1;
				*/
					if(checked_table++ > 10){
						jumpType = JUMP_DENY;
						//jump tableÌ«¶à
						return true;
					}				
					if (m_table->match(rq, obj, jumpType, jumpTable, checked_table,
							hitTable, chainId)) {
						return true;
					}
				}
				continue;
			default:
				*hitTable = name.c_str();
				*chainId = id;
				return true;
			}
		}
	}
	return false;
}
bool KTable::startCharacter(KXmlContext *context, char *character, int len) {
	if (curChain) {
		return curChain->startCharacter(context, character, len);
	}
	return false;
}
bool KTable::endElement(KXmlContext *context) {

	if (context->getParentName() == TABLE_CONTEXT && context->qName
			== CHAIN_CONTEXT) {
		if (curChain) {
			/*
			 if (chainJumpName.size() > 0) {
			 delChain( chainJumpName);
			 }
			 */
			insertChain(-1, curChain);
			curChain = NULL;
		}
		return true;
	}
	if (curChain) {
		return curChain->endElement(context);
	}
	return false;
}
void KTable::htmlTable(std::stringstream &s,const char *vh,u_short accessType) {
	s << name << "," << LANG_REFS << refs;
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE << name
			<< "')){ window.location='tabledel?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_DELETE << "</a>]";
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_EMPTY << name
			<< "')){ window.location='tableempty?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_EMPTY << "</a>]";
	s << "[<a href=\"javascript:tablerename(" << accessType << ",'" << name << "');\">"
			<< LANG_RENAME << "</a>]";
	s << "<table border=1 cellspacing=0 width=100%><tr><td>" << LANG_OPERATOR
			<< "</td><td>" << klang["id"] << "</td><td>" << LANG_ACTION
			<< "</td><td>" << klang["acl"] << "</td><td>" << klang["mark"]
			<< "</td><td>";
	s << LANG_HIT_COUNT << "</td></tr>\n";
	vector<KChain *>::iterator it;
	int j = 0;
	int id = 0;
	for (it = chain.begin(); it != chain.end(); it++, j++) {
		s
				<< "<tr><td>[<a href=\"javascript:if(confirm('Are you sure to delete the chain";
		//		s << "chain name=" << (*it)->name
		s << "')){ window.location='delchain?vh=" << vh << "&access_type=" << accessType
				<< "&id=" << j << "&table_name=" << name << "';}\">"
				<< LANG_DELETE << "</a>][<a href='editchainform?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_EDIT << "</a>][<a href='addchain?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_INSERT << "</a>]</td><td><div>" << id++ << " " << (*it)->name << "</div></td><td>";
		switch ((*it)->jumpType) {
		case JUMP_DENY:
			s << LANG_DENY;
			break;
		case JUMP_ALLOW:
			s << LANG_ALLOW;
			break;
		case JUMP_CONTINUE:
			s << klang["LANG_CONTINUE"];
			break;
		case JUMP_TABLE:
			s << LANG_TABLE;
			break;
		case JUMP_SERVER:
			s << klang["server"];
			break;
		case JUMP_WBACK:
			s << LANG_WRITE_BACK;
			break;
		case JUMP_VHS:
			s << klang["vhs"];
			break;
		case JUMP_FCGI:
			s << "fastcgi";
			break;
		case JUMP_PROXY:
			s << klang["proxy"];
			break;
		case JUMP_DEFAULT:
			s << klang["default"];
			break;
		}
		if ((*it)->jump) {
			s << ":" << (*it)->jump->name;
		}
		s << "</td><td>";
		(*it)->getAclShortHtml(s);
		s << "</td><td>";
		(*it)->getMarkShortHtml(s);
		s << "</td>";
		s << "<td>" << (*it)->hit_count << "</td>";
		s << "</tr>\n";
	}
	s << "</table>";
	s << "[<a href='addchain?vh=" << vh << "&access_type=" << accessType
			<< "&add=1&id=-1&table_name=" << name << "'>" << LANG_INSERT
			<< "</a>]<br><br>";

}
bool KTable::buildXML(const char *chain_name,std::stringstream &s,int flag)
{
	if (chain_name==NULL || *chain_name=='\0') {
		buildXML(s,flag);
		return true;
	}
	KChain *m_chain = findChain(chain_name);
	if (m_chain==NULL) {
		return false;
	}
	s << "\t\t<table name='" << this->name << "'>\n";
	s << "\t\t\t<chain ";//id='" << index++ << "'";
	m_chain->buildXML(s,flag);
	s << "\t\t\t</chain>\n";
	s << "\t\t</table>\n";
	return true;
}
void KTable::buildXML(std::stringstream &s,int flag) {
	std::vector<KChain *>::iterator it;	
	std::stringstream c;
	for (it = chain.begin(); it != chain.end(); it++) {
		if (TEST(flag,CHAIN_SKIP_EXT) && (*it)->ext) {
			continue;
		}
		c << "\t\t\t<chain ";
		(*it)->buildXML(c,flag);
		c << "\t\t\t</chain>\n";
	}
	if (TEST(flag,CHAIN_SKIP_EXT) && ext && c.str().size()==0) {
		return;
	}
	s << "\t\t<table name='" << this->name << "'>\n";
	s << c.str();
	s << "\t\t</table>\n";
	return;
}
KTable::KTable() {
	curChain = NULL;
	ext = cur_config_ext;
}
KTable::~KTable() {
	empty();
}
std::string KTable::addChainForm(KChain *chain,u_short accessType) {
	stringstream s;
	if (chain) {
		chain->getEditHtml(s,accessType);
	}
	return s.str();
}
bool KTable::delChain(int index) {
	if(index<0 || index>=(int)chain.size()){
		return false;
	}
	vector<KChain *>::iterator it = chain.begin() + index;
	delete (*it);
	chain.erase(it);
	return true;
}
bool KTable::editChain(int index, KUrlValue *urlValue,KAccess *kaccess) {
	if(index<0 || index>=(int)chain.size()){
		return false;
	}
	return chain[index]->edit(urlValue,kaccess,true);
}
bool KTable::addAcl(int index, std::string acl, bool mark,KAccess *kaccess) {
	if(index<0 || index>=(int)chain.size()){
		return false;
	}
	if (mark) {
		return chain[index]->addMark(acl,"",kaccess)!=NULL;
	} else {
		return chain[index]->addAcl(acl,"",kaccess)!=NULL;
	}
}
bool KTable::delAcl(int index, std::string acl, bool mark) {
	if(index<0 || index>=(int)chain.size()){
		return false;
	}
	if (mark) {
		return chain[index]->delMark(acl);
	} else {
		return chain[index]->delAcl(acl);
	}

}
KChain *KTable::findChain(const char *name)
{
	vector<KChain *>::iterator it;
	for (it = chain.begin(); it != chain.end(); it++) {
		if((*it)->name == name){
			return (*it);
		}
	}
	return NULL;
}
