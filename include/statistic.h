#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#define DECLARE_STATISTIC_VARS(varSuffix)	static unsigned long n ## varSuffix = 0; \
static double xn ## varSuffix = 0, sn ## varSuffix = 0;

#define ACCOUNT_STATISTIC(varSuffix,varValue)	n ## varSuffix++; \
	xn ## varSuffix = xn ## varSuffix + (varValue - xn ## varSuffix) / n ## varSuffix; \
	if (n ## varSuffix > 1) { \
		sn ## varSuffix = (n ## varSuffix - 2) / (n ## varSuffix - 1) * sn ## varSuffix * sn ## varSuffix+ (varValue - xn ## varSuffix) * (varValue - xn ## varSuffix) / n ## varSuffix; \
	}


#endif // __STATISTIC_H__
