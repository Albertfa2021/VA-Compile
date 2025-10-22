/*
 *  VAShell - Eine Shell (Kommandozeilen) zur Steuerung von VA mittels Lua
 */

#include <iostream>
#include <ITAException.h>

#include <VACore.h>
#include <VACoreFactory.h>
#include <VACoreVersion.h>
#include <VAException.h>
#include <VALuaShell.h>

using namespace std;

int main(int argc, char* argv[]) {

	IVACore* pVACore(nullptr);
	IVALuaShell* pShell(nullptr);

	try {
		pVACore = VACore::CreateCoreInstance();
		// TODO: Nötig? pVACore->SetDebugStream(&std::cout);

		pShell = IVALuaShell::Create();
		pShell->SetCoreInstance(pVACore);

		CVACoreVersionInfo ver;
		pVACore->GetVersionInfo(&ver);
		cout << ver.ToString() << " initializing ..." << endl << endl;

		pVACore->Initialize();

		cout << endl << "VACore Ready." << endl << endl;
		if (argc > 1) {
			// Skript ausführen
			for (int i=1; i<argc; i++) {
				std::string sExecLine = argv[i];
				pShell->ExecuteLuaScript(sExecLine);
			}
		} else {
			// Shell-Modus
			cout << "> ";
			std::string sLine;
			while (true) {
				getline(cin, sLine);
				// DEBUG: cout << "Got line >" << sLine << "<" << endl;
				if (sLine == "exit") break;
				if (sLine == "quit") break;
				pShell->HandleInputLine(sLine);
				cout << "> ";
			}
		}

		delete pShell;
		delete pVACore;

		return 0;

	} catch (CVAException& e) {
		delete pShell;
		delete pVACore;

		cerr << "Error: " << e << endl;
		return 255;
	} catch (...) {
		delete pShell;
		delete pVACore;

		cerr << "Error: An unknown error occured" << endl;
		return 255;
	}
}
