/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2009 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Generate Makefile for cppcheck

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "../lib/fileLister_win32.h"
#else // POSIX-style system
#include "../lib/filelister_unix.h"
#endif

std::string objfile(std::string cppfile)
{
    cppfile.erase(cppfile.rfind("."));
    return cppfile + ".o";
}

void getDeps(const std::string &filename, std::vector<std::string> &depfiles)
{
    // Is the dependency already included?
    if(std::find(depfiles.begin(), depfiles.end(), filename) != depfiles.end())
        return;

    std::ifstream f(filename.c_str());
    if(! f.is_open())
    {
        if(filename.compare(0, 4, "cli/") == 0 || filename.compare(0, 5, "test/") == 0)
            getDeps("lib" + filename.substr(filename.find("/")), depfiles);
        return;
    }
    if(filename.find(".c") == std::string::npos)
        depfiles.push_back(filename);

    std::string path(filename);
    if(path.find("/") != std::string::npos)
        path.erase(1 + path.rfind("/"));

    std::string line;
    while(std::getline(f, line))
    {
        std::string::size_type pos1 = line.find("#include \"");
        if(pos1 == std::string::npos)
            continue;
        pos1 += 10;

        std::string::size_type pos2 = line.find("\"", pos1);
        std::string hfile(path + line.substr(pos1, pos2 - pos1));
        if(hfile.find("/../") != std::string::npos)	// TODO: Ugly fix
            hfile.erase(0, 4 + hfile.find("/../"));
        getDeps(hfile, depfiles);
    }
}

static void compilefiles(std::ostream &fout, const std::vector<std::string> &files)
{
    for(unsigned int i = 0; i < files.size(); ++i)
    {
        fout << objfile(files[i]) << ": " << files[i];
        std::vector<std::string> depfiles;
        getDeps(files[i], depfiles);
        for(unsigned int dep = 0; dep < depfiles.size(); ++dep)
            fout << " " << depfiles[dep];
        fout << "\n\t$(CXX) $(CXXFLAGS) -Ilib -c -o " << objfile(files[i]) << " " << files[i] << "\n\n";
    }
}

static void getCppFiles(std::vector<std::string> &files, const std::string &path)
{
    getFileLister()->recursiveAddFiles(files, path, true);
    // only get *.cpp files..
    for(std::vector<std::string>::iterator it = files.begin(); it != files.end();)
    {
        if(it->find(".cpp") == std::string::npos)
            it = files.erase(it);
        else
            ++it;
    }
}

int main(int argc, char **argv)
{
    const bool release(argc >= 2 && std::string(argv[1]) == "--release");

    // Get files..
    std::vector<std::string> libfiles;
    getCppFiles(libfiles, "lib/");

    std::vector<std::string> clifiles;
    getCppFiles(clifiles, "cli/");

    std::vector<std::string> testfiles;
    getCppFiles(testfiles, "test/");


    // QMAKE - lib/lib.pri
    {
        std::ofstream fout1("lib/lib.pri");
        if(fout1.is_open())
        {
            fout1 << "# no manual edits - this file is autogenerated by dmake\n\n";
            fout1 << "HEADERS += $$PWD/check.h \\\n";
            for(unsigned int i = 0; i < libfiles.size(); ++i)
            {
                std::string fname(libfiles[i].substr(4));
                if(fname.find(".cpp") == std::string::npos)
                    continue;   // shouldn't happen
                fname.erase(fname.find(".cpp"));
                fout1 << std::string(11, ' ') << "$$PWD/" << fname << ".h";
                if(i < libfiles.size() - 1)
                    fout1 << " \\\n";
            }
            fout1 << "\n\nSOURCES += ";
            for(unsigned int i = 0; i < libfiles.size(); ++i)
            {
                fout1 << "$$PWD/" << libfiles[i].substr(4);
                if(i < libfiles.size() - 1)
                    fout1 << " \\\n" << std::string(11, ' ');
            }
            fout1 << "\n";
        }
    }


    std::ofstream fout("Makefile");

    // Makefile settings..
    // TODO: add more compiler warnings.
    // -Wsign-conversion : generates too many compiler warnings currently
    // -Wlogical-op      : doesn't work on older GCC
    fout << "CXXFLAGS=-Wall -Wextra -pedantic -Wno-long-long -Wfloat-equal -Wcast-qual ";
    fout << (release ? "-O2 -DNDEBUG" : "-g -D_GLIBCXX_DEBUG") << "\n";
    fout << "CXX=g++\n";
    fout << "BIN=${DESTDIR}/usr/bin\n\n";
    fout << "# For 'make man': sudo apt-get install xsltproc docbook-xsl docbook-xml\n";
    fout << "DB2MAN=/usr/share/sgml/docbook/stylesheet/xsl/nwalsh/manpages/docbook.xsl\n";
    fout << "XP=xsltproc -''-nonet -''-param man.charmap.use.subset \"0\"\n";
    fout << "MAN_SOURCE=man/cppcheck.1.xml\n\n";

    fout << "\n###### Object Files\n\n";
    fout << "LIBOBJ =     " << objfile(libfiles[0]);
    for(unsigned int i = 1; i < libfiles.size(); ++i)
        fout << " \\" << std::endl << std::string(14, ' ') << objfile(libfiles[i]);
    fout << "\n\n";
    fout << "CLIOBJ =     " << objfile(clifiles[0]);
    for(unsigned int i = 1; i < clifiles.size(); ++i)
        fout << " \\" << std::endl << std::string(14, ' ') << objfile(clifiles[i]);
    fout << "\n\n";
    fout << "TESTOBJ =     " << objfile(testfiles[0]);
    for(unsigned int i = 1; i < testfiles.size(); ++i)
        fout << " \\" << std::endl << std::string(14, ' ') << objfile(testfiles[i]);
    fout << "\n\n";


    fout << "\n###### Targets\n\n";
    fout << "cppcheck:\t$(LIBOBJ)\t$(CLIOBJ)\n";
    fout << "\t$(CXX) $(CXXFLAGS) -o cppcheck $(CLIOBJ) $(LIBOBJ) $(LDFLAGS)\n\n";
    fout << "all:\tcppcheck\ttestrunner\ttools\n\n";
    fout << "testrunner:\t$(TESTOBJ)\t$(LIBOBJ)\n";
    fout << "\t$(CXX) $(CXXFLAGS) -o testrunner $(TESTOBJ) $(LIBOBJ) $(LDFLAGS)\n\n";
    fout << "test:\tall\n";
    fout << "\t./testrunner\n\n";
    fout << "clean:\n";
#ifdef _WIN32
    fout << "\tdel lib\*.o\n\tdel cli\*.o\n\tdel test\*.o\n\tdel *.exe\n";
#else
    fout << "\trm -f lib/*.o cli/*.o test/*.o testrunner cppcheck\n\n";
    fout << "man:\t$(MAN_SOURCE)\n";
    fout << "\t$(XP) $(DB2MAN) $(MAN_SOURCE)\n\n";
    fout << "install:\tcppcheck\n";
    fout << "\tinstall -d ${BIN}\n";
    fout << "\tinstall cppcheck ${BIN}\n\n";
#endif

    fout << "\n###### Build\n\n";

    compilefiles(fout, libfiles);
    compilefiles(fout, clifiles);
    compilefiles(fout, testfiles);

    return 0;
}


