

#ifndef SML_EMP_MPI_H
#define SML_EMP_MPI_H
#include "Export.h"

class EXPORT emp_mpi {
public:
    emp_mpi();
    void init(int startWindowSize, bool evenDivFlag);
};
#endif
