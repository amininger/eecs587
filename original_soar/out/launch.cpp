#include <iostream>
using std::cout;
using std::endl;
#include "mpi.h"
#include <string>
#include "sml_Client.h"
using namespace sml;
using namespace std;
string strip(string s, string lc, string rc) {
	size_t b, e;
	b = s.find_first_not_of(lc);
	e = s.find_last_not_of(rc);
	return s.substr(b, e - b + 1);
}

void printcb(smlPrintEventId id, void *d, Agent *a, char const *m) {
	cout << strip(m, "\n", "\n\t ") << endl;
}
int main(int argc, char** argv){
	MPI::Init(argc, argv); //move
    int id = MPI::COMM_WORLD.Get_rank();
	
	if (id == 0)
	{
		cout << "Agent Processor " << id << " online" << endl;

		Kernel* kernel = Kernel::CreateKernelInNewThread();
		Agent* agent = kernel->CreateAgent("HelloSoar");
		
		agent->RegisterForPrintEvent(smlEVENT_PRINT, printcb, NULL);
	
		string source = "source ";
		source += argv[1];
		
		cout << "---- SOURCING (" << argv[1]  << ") ----" << endl;	
		cout << agent->ExecuteCommandLine(source.c_str()) << endl;
	
		cout << "---- RUNNING ----" << endl;
		agent->ExecuteCommandLine("run");	
		
		kernel->Shutdown();
	}
		
	MPI::Finalize();
	return 0;
}
