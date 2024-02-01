// qos.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "qos.h"


// This is an example of an exported variable
QOS_API int nqos=0;

// This is an example of an exported function.
QOS_API int fnqos(void)
{
    return 0;
}

// This is the constructor of a class that has been exported.
Cqos::Cqos()
{
    return;
}
