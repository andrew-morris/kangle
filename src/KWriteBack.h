#ifndef WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#define WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#include <vector>
#include <string.h>
#include <stdlib.h>
#include<list>
#include<string>
#include "global.h"
#include "KMutex.h"
#include "KJump.h"
#include "KXml.h"
#include "server.h"
#ifdef ENABLE_WRITE_BACK
class KWriteBack:public KJump
{
public:
	KWriteBack()
	{
		ext = cur_config_ext;
	}
	std::string msg;
	void buildXML(std::stringstream &s) {
		s << "\t<writeback name='" << name << "'>";
		s << CDATA_START << KXml::encode(msg) << CDATA_END << "</writeback>\n";
	}
	bool ext;
};
#endif
#endif
