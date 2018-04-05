/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */


#include "fdb5/toc/RootManager.h"

#include "eckit/types/Types.h"
#include "eckit/config/Resource.h"
#include "eckit/parser/Tokenizer.h"
#include "eckit/parser/StringTools.h"
#include "eckit/utils/Translator.h"
#include "eckit/thread/Mutex.h"
#include "eckit/thread/AutoLock.h"

#include "fdb5/LibFdb.h"
#include "fdb5/database/Key.h"
#include "fdb5/toc/Root.h"
#include "fdb5/toc/FileSpace.h"
#include "fdb5/config/Config.h"

using namespace eckit;

namespace fdb5 {
    class DbPathNamer;
    class FileSpace;
}
namespace eckit {
    template <> struct VectorPrintSelector<fdb5::DbPathNamer> { typedef VectorPrintSimple selector; };
    template <> struct VectorPrintSelector<fdb5::FileSpace> { typedef VectorPrintSimple selector; };
}

namespace fdb5 {

//----------------------------------------------------------------------------------------------------------------------

class DbPathNamer {
public:

    DbPathNamer(const std::string& keyregex, const std::string& format) :
        format_(format){
        crack(keyregex);
        eckit::Log::debug<LibFdb>() << "Building " << *this << std::endl;
    }

    /// Full match of the incomming key with the key regex
    /// but partial match for values
    bool match(const Key& k, const char* missing = 0) const {

//        std::cout << " Trying to key matching " << *this << " with key " << k << std::endl;

        if(k.size() != keyregex_.size()) return false;

        for(Key::const_iterator i = k.begin(); i != k.end(); ++i) {

//            std::cout << "     Match " << i->first << " " << i->second << std::endl;

            std::map<std::string, Regex>::const_iterator j = keyregex_.find(i->first);

            if(j == keyregex_.end()) {
                return false;
            }

//            std::cout << "     Found " << j->first << " " << j->second << std::endl;

            if(!missing || i->second != missing) {
                if(!j->second.match(i->second)) {
                    return false;
                }
            }
        }

//        std::cout << " Match successfull " << *this << " with key " << k << std::endl;

        return true;
    }

    std::string name(const Key& key) const {
        return substituteVars(format_, key);
    }

    std::string namePartial(const Key& key, const char* missing) const {
        return substituteVars(format_, key, missing);
    }

    friend std::ostream& operator<<(std::ostream &s, const DbPathNamer& x) {
        x.print(s);
        return s;
    }

private: // methods

    void crack(const std::string& regexstr) {

        eckit::Tokenizer parse1(",");
        eckit::StringList v;

        parse1(regexstr, v);

        eckit::Tokenizer parse2("=");
        for (eckit::StringList::const_iterator i = v.begin(); i != v.end(); ++i) {

            eckit::StringList kv;
            parse2(*i, kv);

            if(kv.size() == 2) {
                keyregex_[kv[0]] = kv[1];
            }
            else {
                if(kv.size() == 1) {
                    keyregex_[kv[0]] = std::string(".*");
                }
                else {
                    std::ostringstream msg;
                    msg << "Malformed regex in DbNamer " << regexstr;
                    throw BadValue(msg.str(), Here());
                }
            }
        }
    }

    std::string substituteVars(const std::string& s, const Key& k, const char * missing = 0) const
    {
        std::string result;
        size_t len = s.length();
        bool var = false;
        std::string word;
        std::map<std::string,std::string>::const_iterator j;

        for(size_t i = 0; i < len; i++)
        {
            switch(s[i])
            {
                case '{':
                    if(var) {
                        std::ostringstream os;
                        os << "FDB RootManager substituteVars: unexpected { found in " <<s << " at position " << i;
                        throw UserError(os.str());
                    }
                    var = true;
                    word = "";
                    break;

                case '}':
                    if(!var) {
                        std::ostringstream os;
                        os << "FDB RootManager substituteVars: unexpected } found in " <<s << " at position " << i;
                        throw UserError(os.str());
                    }
                    var = false;

                    j = k.find(word);
                    if(j != k.end()) {
                        if(!missing) {
                            result += (*j).second;
                        }
                        else {
                            if((*j).second == missing) {
                                result += keyregex_.find(word)->second; // we know it exists because it is ensured in match()
                            }
                            else
                            {
                                result += (*j).second;
                            }
                        }
                    }
                    else {
                        std::ostringstream os;
                        os << "FDB RootManager substituteVars: cannot find a value for '" << word << "' in " <<s << " at position " << i;
                        throw UserError(os.str());
                    }
                    break;

                default:
                    if(var)
                        word += s[i];
                    else
                        result += s[i];
                    break;
            }
        }
        if(var) {
            std::ostringstream os;
            os << "FDB RootManager substituteVars: missing } in " << s;
            throw UserError(os.str());
        }
        return result;
    }

    void print( std::ostream &out ) const {
        out << "DbPathNamer(keyregex=" << keyregex_ << ", format=" << format_ << ")";
    }

    std::map<std::string, Regex> keyregex_;

    std::string format_;

};

typedef std::vector<fdb5::DbPathNamer> DbPathNamerTable;
typedef std::map<eckit::PathName, DbPathNamerTable> DbPathNamerMap;

eckit::Mutex pathNamerMutex;
static DbPathNamerMap dbPathNamers;

static const DbPathNamerTable& readDbNamers(const Config& config) {

    static std::string fdbDbNamesFile = eckit::Resource<std::string>("fdbDbNamesFile;$FDB_DBNAMES_FILE", "~fdb/etc/fdb/dbnames");

    eckit::PathName filename(config.expandPath(fdbDbNamesFile));

    eckit::AutoLock<eckit::Mutex> lock(pathNamerMutex);

    // Memoise the results

    auto it = dbPathNamers.find(filename);
    if (it != dbPathNamers.end()) {
        return it->second;
    }

    // Or read it!

    DbPathNamerTable& table(dbPathNamers[filename]);

    if(filename.exists()) {

        eckit::Log::debug<LibFdb>() << "Loading FDB DBPathNames from " << fdbDbNamesFile << std::endl;

        std::ifstream in(filename.localPath());

        if (!in) {
            eckit::Log::error() << fdbDbNamesFile << eckit::Log::syserr << std::endl;
            return table;
        }

        eckit::Tokenizer parse(" ");

        char line[1024];
        while (in.getline(line, sizeof(line))) {

            std::vector<std::string> s;
            parse(line, s);

            size_t i = 0;
            while (i < s.size()) {
                if (s[i].length() == 0) {
                    s.erase(s.begin() + i);
                } else {
                    i++;
                }
            }

            if (s.size() == 0 || s[0][0] == '#') {
                continue;
            }

            switch (s.size()) {
                case 2: {
                    const std::string& regex     = s[0];
                    const std::string& format    = s[1];

                    table.push_back(DbPathNamer(regex, format));
                    break;
                }

                default:
                    eckit::Log::warning() << "FDB DBPathNames invalid line ignored: " << line << std::endl;
                break;
            }
        }
    }

    return table;
}

//----------------------------------------------------------------------------------------------------------------------

typedef std::vector<fdb5::FileSpace> FileSpaceTable;
typedef std::map<eckit::PathName, FileSpaceTable> FileSpaceMap;

eckit::Mutex fileSpacesMutex;
static FileSpaceMap spacesTables;

static std::vector<Root> readRoots(const eckit::PathName& fdbRootsFile) {

    std::vector<Root> result;

    std::ifstream in(fdbRootsFile.localPath());

    eckit::Log::debug<LibFdb>() << "Loading FDB roots from " << fdbRootsFile << std::endl;

    if (!in) {
        eckit::Log::error() << fdbRootsFile << eckit::Log::syserr << std::endl;
        return result;
    }

    eckit::Translator<std::string,bool> str2bool;

    eckit::Tokenizer parse(" ");

    char line[1024];
    while (in.getline(line, sizeof(line))) {

        std::vector<std::string> s;
        parse(line, s);

        size_t i = 0;
        while (i < s.size()) {
            if (s[i].length() == 0) {
                s.erase(s.begin() + i);
            } else {
                i++;
            }
        }

        if (s.size() == 0 || s[0][0] == '#') {
            continue;
        }

        switch (s.size()) {
            case 4: {
                const std::string& path       = s[0];
                const std::string& filespace  = s[1];
                bool writable        = str2bool(s[2]);
                bool visit           = str2bool(s[3]);

                result.push_back(Root(path, filespace, writable, visit));
                break;
            }

        default:
            eckit::Log::warning() << "FDB RootManager: Invalid line ignored: " << line << std::endl;
            break;
        }
    }

    return result;
}

static std::vector<Root> fileSpaceRoots(const std::vector<Root>& all, const std::string& filespace) {

    std::vector<Root> roots;

    for (std::vector<Root>::const_iterator i = all.begin(); i != all.end() ; ++i) {
        if (i->filespace() == filespace) {
            roots.push_back(*i);
        }
    }
    return roots;
}

static const FileSpaceTable& readFileSpaces(const eckit::PathName& fdbHome) {

    eckit::AutoLock<eckit::Mutex> lock(fileSpacesMutex);

    // File spaces are memoised, so only read it once

    FileSpaceMap::const_iterator it = spacesTables.find(fdbHome);
    if (it != spacesTables.end()) {
        return it->second;
    }

    eckit::PathName fdbRootsFile = eckit::Resource<eckit::PathName>("fdbRootsFile;$FDB_ROOTS_FILE", fdbHome / "etc/fdb/roots");
    std::vector<Root> allRoots = readRoots(fdbRootsFile);

    eckit::PathName fdbSpacesFile = eckit::Resource<eckit::PathName>("fdbSpacesFile;$FDB_SPACES_FILE", fdbHome / "etc/fdb/spaces");
    std::ifstream in(fdbSpacesFile.localPath());

    eckit::Log::debug<LibFdb>() << "Loading FDB file spaces from " << fdbSpacesFile << std::endl;

    if (!in) {
        throw eckit::ReadError(fdbSpacesFile, Here());
    }

    FileSpaceTable& table(spacesTables[fdbHome]);

    eckit::Tokenizer parse(" ");

    char line[1024];
    while (in.getline(line, sizeof(line))) {

        std::vector<std::string> s;
        parse(line, s);

        size_t i = 0;
        while (i < s.size()) {
            if (s[i].length() == 0) {
                s.erase(s.begin() + i);
            } else {
                i++;
            }
        }

        if (s.size() == 0 || s[0][0] == '#') {
            continue;
        }

        switch (s.size()) {
            case 3: {
                const std::string& regex     = s[0];
                const std::string& filespace = s[1];
                const std::string& handler   = s[2];

                std::vector<Root> roots = fileSpaceRoots(allRoots, filespace);

                if(!roots.size()) {
                    std::ostringstream oss;
                    oss << "No roots found for filespace " << filespace;
                    throw UserError(oss.str(), Here());
                }

                table.push_back(FileSpace(filespace, regex, handler, roots));
                break;
            }

        default:
            eckit::Log::warning() << "FDB RootManager: Invalid line ignored: " << line << std::endl;
            break;

        }
    }

    return table;
}

//----------------------------------------------------------------------------------------------------------------------

RootManager::RootManager(const Config& config) :
    spacesTable_(readFileSpaces(config.expandPath("~fdb/"))),
    dbPathNamers_(readDbNamers(config)) {

//    eckit::Log::info() << "Root manager: " << spacesTable_ << std::endl;
}


std::string RootManager::dbPathName(const Key& key)
{
    std::string dbpath;
    for (DbPathNamerTable::const_iterator i = dbPathNamers_.begin(); i != dbPathNamers_.end() ; ++i) {
        if(i->match(key)) {
            dbpath = i->name(key);
            eckit::Log::debug<LibFdb>() << "DbName is " << dbpath << " for key " << key <<  std::endl;
            return dbpath;
        }
    }

    // default naming convention for DB's
    dbpath = key.valuesToString();
    eckit::Log::debug<LibFdb>() << "Using default naming convention for key " << key << " -> " << dbpath <<  std::endl;
    return dbpath;
}

std::vector<std::string> RootManager::possibleDbPathNames(const Key& key, const char* missing)
{
    std::vector<std::string> result;
    for (DbPathNamerTable::const_iterator i = dbPathNamers_.begin(); i != dbPathNamers_.end() ; ++i) {
        if(i->match(key, missing)) {
            std::string dbpath = i->namePartial(key, missing);
            eckit::Log::debug<LibFdb>() << "Matched " << *i << " with key " << key << " resulting in dbpath " << dbpath << std::endl;
            result.push_back(dbpath);
        }
    }

    // default naming convention for DB's
    result.push_back(key.valuesToString());

    eckit::Log::debug<LibFdb>() << "Using default naming convention for key " << key << " -> " << result.back() <<  std::endl;

    return result;
}


eckit::PathName RootManager::directory(const Key& key) {

    PathName dbpath = dbPathName(key);

    eckit::Log::debug<LibFdb>() << "Choosing directory for key " << key << " dbpath " << dbpath << std::endl;

    // override root location for testing purposes only

    static std::string fdbRootDirectory = eckit::Resource<std::string>("fdbRootDirectory;$FDB_ROOT_DIRECTORY", "");

    if(!fdbRootDirectory.empty()) {
        return fdbRootDirectory + "/" + dbpath;
    }

    // returns the first filespace that matches

    std::string keystr = key.valuesToString();
    for (FileSpaceTable::const_iterator i = spacesTable_.begin(); i != spacesTable_.end() ; ++i) {
        if(i->match(keystr)) {
            PathName root = i->filesystem(key, dbpath);
            eckit::Log::debug<LibFdb>() << "Directory root " << root << " dbpath " << dbpath <<  std::endl;
            return root / dbpath;
        }
    }

    std::ostringstream oss;
    oss << "No FDB file space for " << key << " (" << keystr << ")";
    throw eckit::SeriousBug(oss.str());
}

std::vector<PathName> RootManager::allRoots(const Key& key)
{
    eckit::StringSet roots;

    std::string k = key.valuesToString();

    for (FileSpaceTable::const_iterator i = spacesTable_.begin(); i != spacesTable_.end() ; ++i) {
        if(i->match(k)) {
            i->all(roots);
        }
    }

    return std::vector<eckit::PathName>(roots.begin(), roots.end());
}


std::vector<eckit::PathName> RootManager::visitableRoots(const Key& key) {

    eckit::StringSet roots;

    std::string k = key.valuesToString();

    Log::debug<LibFdb>() << "RootManager::visitableRoots() trying to match key " << k << std::endl;

    for (FileSpaceTable::const_iterator i = spacesTable_.begin(); i != spacesTable_.end() ; ++i) {
        if(i->match(k) || key.empty()) {
            Log::debug<LibFdb>() << "MATCH space " << *i << std::endl;
            i->visitable(roots);
        }
        else {
            Log::debug<LibFdb>() << "FAIL to match space " << *i << std::endl;
        }
    }

    Log::debug<LibFdb>() << "Visitable Roots " << roots << std::endl;

    return std::vector<eckit::PathName>(roots.begin(), roots.end());
}

std::vector<eckit::PathName> RootManager::writableRoots(const Key& key) {

    eckit::StringSet roots;

    std::string k = key.valuesToString();

    for (FileSpaceTable::const_iterator i = spacesTable_.begin(); i != spacesTable_.end() ; ++i) {
        if(i->match(k)) {
            i->writable(roots);
        }
    }

    return std::vector<eckit::PathName>(roots.begin(), roots.end());
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace fdb5
