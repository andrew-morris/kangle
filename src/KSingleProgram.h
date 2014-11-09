/*
 * KSingleProgram.h
 *
 *  Created on: 2010-4-30
 *      Author: keengo
 */

#ifndef KSINGLEPROGRAM_H_
#define KSINGLEPROGRAM_H_
#include "KFile.h"
class KSingleProgram {
public:
	KSingleProgram();
	virtual ~KSingleProgram();
	bool checkRunning(const char *pidFile);
	bool lock(const char *pidFile);
	void unlock();
	bool savePid(int savePid);

	int pid;
private:
	FILE_HANDLE fd;
#ifdef _WIN32
	OVERLAPPED ov;
#endif
};
extern KSingleProgram singleProgram;
#endif /* KSINGLEPROGRAM_H_ */
