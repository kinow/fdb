/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "eckit/io/DataHandle.h"

#include "fdb5/api/FDB.h"
#include "fdb5/api/FDBFactory.h"
#include "fdb5/database/Key.h"

namespace fdb5 {

//----------------------------------------------------------------------------------------------------------------------

FDB::FDB(const Config &config) :
    internal_(FDBFactory::instance().build(config)),
    dirty_(false),
    reportStats_(config.getBool("statistics", false)) {}


FDB::~FDB() {
    flush();
    if (reportStats_ && internal_) {
        stats_.report(eckit::Log::info(), (internal_->name() + " ").c_str());
        internal_->stats().report(eckit::Log::info(), (internal_->name() + " internal ").c_str());
    }
}

void FDB::archive(const Key& key, const void* data, size_t length) {
    eckit::Timer timer;
    timer.start();

    internal_->archive(key, data, length);
    dirty_ = true;

    timer.stop();
    stats_.addArchive(length, timer);
}

eckit::DataHandle* FDB::retrieve(const MarsRequest& request) {

    eckit::Timer timer;
    timer.start();
    eckit::DataHandle* dh = internal_->retrieve(request);
    timer.stop();
    stats_.addRetrieve(dh->estimate(), timer);

    return dh;
}

ListIterator FDB::list(const FDBToolRequest& request) {
    return internal_->list(request);
}

DumpIterator FDB::dump(const FDBToolRequest& request, bool simple) {
    return internal_->dump(request, simple);
}

WhereIterator FDB::where(const FDBToolRequest& request) {
    return internal_->where(request);
}

WipeIterator FDB::wipe(const FDBToolRequest& request, bool doit) {
    return internal_->wipe(request, doit);
}

PurgeIterator FDB::purge(const FDBToolRequest &request, bool doit) {
    return internal_->purge(request, doit);
}

StatsIterator FDB::stats(const FDBToolRequest &request) {
    return internal_->stats(request);
}

const std::string FDB::id() const {
    return internal_->id();
}

FDBStats FDB::stats() const {
    return stats_;
}

FDBStats FDB::internalStats() const {
    return internal_->stats();
}

void FDB::print(std::ostream& s) const {
    s << *internal_;
}

void FDB::flush() {
    if (dirty_) {

        eckit::Timer timer;
        timer.start();

        internal_->flush();
        dirty_ = false;

        timer.stop();
        stats_.addFlush(timer);
    }
}

bool FDB::dirty() const {
    return dirty_;
}

void FDB::disable() {
    internal_->disable();
}

bool FDB::disabled() const {
    return internal_->disabled();
}

bool FDB::writable() const {
    return internal_->writable();
}

bool FDB::visitable() const {
    return internal_->visitable();
}

//----------------------------------------------------------------------------------------------------------------------

} // namespace fdb5
