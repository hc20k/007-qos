// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the QOS_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// QOS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef QOS_EXPORTS
#define QOS_API __declspec(dllexport)
#else
#define QOS_API __declspec(dllimport)
#endif

// This class is exported from the dll
class QOS_API Cqos {
public:
	Cqos(void);
	// TODO: add your methods here.
};

extern QOS_API int nqos;

QOS_API int fnqos(void);
