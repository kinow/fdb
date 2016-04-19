/*
 * (C) Copyright 1996-2016 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include <algorithm>
#include <sstream>

#include "eckit/config/Resource.h"
#include "eckit/io/DataHandle.h"
#include "eckit/log/Timer.h"

#include "marslib/MarsTask.h"

#include "fdb5/Key.h"
#include "fdb5/Archiver.h"
#include "fdb5/MasterConfig.h"

#include "fdb5/ArchiveVisitor.h"
#include "fdb5/AdoptVisitor.h"

using namespace eckit;

namespace fdb5 {

//----------------------------------------------------------------------------------------------------------------------


Archiver::Archiver() :
    current_(0)
{
    fdbWriterDB_ = eckit::Resource<std::string>("fdbWriterDB","toc.writer");
}

Archiver::~Archiver()
{
    flush(); // certify that all sessions are flushed before closing them
}

void Archiver::write(const DataBlobPtr blob)
{
    NOTIMP; /// @todo this will substitute the GribArchiver
}

void Archiver::write(const Key& key, const void* data, size_t length)
{
    Log::info() << "Archiver write " << length << std::endl;

    ASSERT(data);

    const Rules& rules = MasterConfig::instance().rules();

    ArchiveVisitor visitor(*this, key, data, length);

    rules.expand(key, visitor);

    if(visitor.rule() == 0) { // Make sure we did find a rule that matched
        std::ostringstream oss;
        oss << "FDB: Could not find a rule to archive " << key;
        throw SeriousBug(oss.str());
    }
}

void Archiver::adopt(const Key& key, const PathName& path, Offset offset, Length length)
{
    const Rules& rules = MasterConfig::instance().rules();

    AdoptVisitor visitor(*this, key, path, offset, length);

    rules.expand(key, visitor);

    if(visitor.rule() == 0) { // Make sure we did find a rule that matched
        std::ostringstream oss;
        oss << "FDB: Could not find a rule to archive " << key;
        throw SeriousBug(oss.str());
    }
}

void Archiver::flush()
{
    for(store_t::iterator i = databases_.begin(); i != databases_.end(); ++i) {
        i->second->flush();
    }
}


DB& Archiver::session(const Key& key)
{
    store_t::iterator i = databases_.find(key);

    if(i != databases_.end() )
        return *(i->second.get());

    eckit::SharedPtr<DB> newSession ( DBFactory::build(fdbWriterDB_, key) );
    ASSERT(newSession);
    databases_[key] = newSession;
    return *newSession;
}

void Archiver::print(std::ostream& out) const
{
    out << "Archiver("
        << ")"
        << std::endl;
}

//----------------------------------------------------------------------------------------------------------------------

} // namespace fdb5
